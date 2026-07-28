#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#include <zephyr/sys/util.h>
#include <esb.h>

#define SAMPLE_RATE            8000
#define AUDIO_WORD_SIZE         24
#define AUDIO_BLOCK_COUNT        4
#define AUDIO_SAMPLES_PER_BLOCK 512
#define AUDIO_BLOCK_SIZE        (sizeof(int32_t) * AUDIO_SAMPLES_PER_BLOCK)

#define ESB_CHUNK_DATA_SIZE      128
struct chunk_hdr {
    uint8_t seq;
    uint8_t reserved;
};
#define CHUNK_PKT_SIZE (sizeof(struct chunk_hdr) + ESB_CHUNK_DATA_SIZE)

const struct device *i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));
struct i2s_config tx_cfg;
struct i2s_config rx_cfg; /* dummy RX, kept alive for I2S_DIR_BOTH -- matches your working loopback config */

K_MEM_SLAB_DEFINE(tx_mem_slab, AUDIO_BLOCK_SIZE, AUDIO_BLOCK_COUNT, 4);
K_MEM_SLAB_DEFINE(rx_mem_slab, AUDIO_BLOCK_SIZE, AUDIO_BLOCK_COUNT, 4);

/* ---- Ring buffer of raw sample bytes, same idea as the UDP reference:
 * ESB event handler pushes bytes as chunks arrive, playback pulls a
 * block's worth whenever ready, falling back to silence if starved. ---- */
#define RING_SIZE 8000   /* bytes, ~0.5s at 8kHz/16-bit -- shrink for less latency */
static uint8_t ring[RING_SIZE];
static volatile int ring_head = 0;
static volatile int ring_tail = 0;

static inline int ring_available(void)
{
    return (ring_head - ring_tail + RING_SIZE) % RING_SIZE;
}

static inline void ring_push(uint8_t b)
{
    int next = (ring_head + 1) % RING_SIZE;
    if (next != ring_tail) {
        ring[ring_head] = b;
        ring_head = next;
    }
    /* else: ring full, byte dropped -- counted below */
}

static struct esb_payload rx_payload;

static uint32_t dbg_esb_recv = 0;
static uint32_t dbg_ring_drops = 0;
static uint32_t dbg_play_blocks = 0;
static uint32_t dbg_seq_gaps = 0;
static uint8_t  last_seq = 0;
static bool     have_last_seq = false;

static void event_handler(struct esb_evt const *event)
{
    switch (event->evt_id) {
    case ESB_EVENT_RX_RECEIVED:
        while (esb_read_rx_payload(&rx_payload) == 0) {
            if (rx_payload.length != CHUNK_PKT_SIZE) continue;

            struct chunk_hdr hdr;
            memcpy(&hdr, rx_payload.data, sizeof(hdr));

            if (have_last_seq && (uint8_t)(hdr.seq - last_seq) != 1) {
                dbg_seq_gaps++;
            }
            last_seq = hdr.seq;
            have_last_seq = true;
            dbg_esb_recv++;

            uint8_t *payload = rx_payload.data + sizeof(hdr);
            int before = ring_available();
            for (int i = 0; i < ESB_CHUNK_DATA_SIZE; i++) {
                ring_push(payload[i]);
            }
            int after = ring_available();
            int pushed = after - before;
            if (pushed < ESB_CHUNK_DATA_SIZE) {
                dbg_ring_drops += (ESB_CHUNK_DATA_SIZE - pushed);
            }
        }
        break;
    case ESB_EVENT_TX_SUCCESS:
    case ESB_EVENT_TX_FAILED:
        break;
    }
}

static int esb_initialize(void)
{
    int err;
    uint8_t base_addr_0[4] = {0xE7, 0xE7, 0xE7, 0xE7};
    uint8_t base_addr_1[4] = {0xC2, 0xC2, 0xC2, 0xC2};
    uint8_t addr_prefix[8] = {0xE7, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8};
    struct esb_config config = ESB_DEFAULT_CONFIG;

    config.protocol = ESB_PROTOCOL_ESB_DPL;
    config.bitrate = ESB_BITRATE_2MBPS;
    config.event_handler = event_handler;
    config.mode = ESB_MODE_PRX;
    config.selective_auto_ack = false;

    err = esb_init(&config);
    if (err) return err;
    err = esb_set_base_address_0(base_addr_0);
    if (err) return err;
    err = esb_set_base_address_1(base_addr_1);
    if (err) return err;
    err = esb_set_prefixes(addr_prefix, ARRAY_SIZE(addr_prefix));
    if (err) return err;
    err = esb_set_rf_channel(2); /* must match sender exactly */
    if (err) return err;
    return esb_start_rx();
}

static int configure_i2s_rxntx(void)
{
    if (!device_is_ready(i2s_dev)) {
        printk("I2S not ready\n");
        return -ENODEV;
    }
    printk("I2S ready\n");

    tx_cfg.word_size = AUDIO_WORD_SIZE;
    tx_cfg.channels = 1;
    tx_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
    tx_cfg.options = I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER;
    tx_cfg.frame_clk_freq = SAMPLE_RATE;
    tx_cfg.mem_slab = &tx_mem_slab;
    tx_cfg.block_size = AUDIO_BLOCK_SIZE;
    tx_cfg.timeout = SYS_FOREVER_MS;

    int tx_ret = i2s_configure(i2s_dev, I2S_DIR_TX, &tx_cfg);
    printk("I2S TX configure: %d\n", tx_ret);
    if (tx_ret) return tx_ret;

    rx_cfg.word_size = AUDIO_WORD_SIZE;
    rx_cfg.channels = 1;
    rx_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
    rx_cfg.options = I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER;
    rx_cfg.frame_clk_freq = SAMPLE_RATE;
    rx_cfg.mem_slab = &rx_mem_slab;
    rx_cfg.block_size = AUDIO_BLOCK_SIZE;
    rx_cfg.timeout = SYS_FOREVER_MS;

    int rx_ret = i2s_configure(i2s_dev, I2S_DIR_RX, &rx_cfg);
    printk("I2S RX configure: %d\n", rx_ret);
    if (rx_ret) return rx_ret;

    return 0;
}

static void preload_silence(void)
{
    for (int i = 0; i < AUDIO_BLOCK_COUNT; i++) {
        void *buf;
        k_mem_slab_alloc(&tx_mem_slab, &buf, K_FOREVER);
        memset(buf, 0, AUDIO_BLOCK_SIZE);
        int ret = i2s_write(i2s_dev, buf, AUDIO_BLOCK_SIZE);
        printk("Preload write %d = %d\n", i, ret);
    }
}

K_SEM_DEFINE(rx_drain_ready_sem, 0, 1);

/* dummy RX drain, keeps I2S_DIR_BOTH stable -- matches your proven-working loopback config */
static void rx_drain_task(void *p1, void *p2, void *p3)
{
    k_sem_take(&rx_drain_ready_sem, K_FOREVER);
    printk("RX drain thread started\n");

    void *buf;
    size_t size;
    while (1) {
        int ret = i2s_read(i2s_dev, &buf, &size);
        if (ret == 0) {
            k_mem_slab_free(&rx_mem_slab, buf);
        }
    }
}
K_THREAD_DEFINE(rx_drain_id, 2048, rx_drain_task, NULL, NULL, NULL, 6, 0, 0);

static void print_debug(void)
{
    static int64_t last = 0;
    int64_t now = k_uptime_get();
    if (now - last < 2000) return;
    last = now;
    printk("---------------------------------\n");
    printk("[ESB]  recv=%u seq_gaps=%u\n", dbg_esb_recv, dbg_seq_gaps);
    printk("[RING] avail=%d drops=%u\n", ring_available(), dbg_ring_drops);
    printk("[PLAY] blocks=%u\n", dbg_play_blocks);
    printk("---------------------------------\n");
    dbg_esb_recv = 0;
    dbg_seq_gaps = 0;
    dbg_ring_drops = 0;
    dbg_play_blocks = 0;
}

int main(void)
{
    k_msleep(1000);
    printk("RECEIVER START\n");

    if (configure_i2s_rxntx()) {
        return 0;
    }

    int err = esb_initialize();
    printk("ESB init result: %d\n", err);
    if (err) {
        printk("ESB init failed: %d\n", err);
        return 0;
    }

    preload_silence();

    int ret = i2s_trigger(i2s_dev, I2S_DIR_BOTH, I2S_TRIGGER_START);
    printk("I2S BOTH trigger: %d\n", ret);

    k_sem_give(&rx_drain_ready_sem);

    while (1) {
        void *buf;
        if (k_mem_slab_alloc(&tx_mem_slab, &buf, K_FOREVER) != 0) {
            continue;
        }

        int32_t *out = (int32_t *)buf;
        int avail = ring_available();
        int needed = AUDIO_SAMPLES_PER_BLOCK * (int)sizeof(int16_t);

        if (avail >= needed) {
            for (int i = 0; i < AUDIO_SAMPLES_PER_BLOCK; i++) {
                int16_t sample;
                uint8_t *sb = (uint8_t *)&sample;
                sb[0] = ring[ring_tail]; ring_tail = (ring_tail + 1) % RING_SIZE;
                sb[1] = ring[ring_tail]; ring_tail = (ring_tail + 1) % RING_SIZE;
                out[i] = ((int32_t)sample) << 16;
            }
            dbg_play_blocks++;
        } else {
            memset(buf, 0, AUDIO_BLOCK_SIZE); /* not enough buffered yet, play silence */
        }

        int w = i2s_write(i2s_dev, buf, AUDIO_BLOCK_SIZE);
        if (w) {
            printk("i2s_write failed: %d\n", w);
            k_mem_slab_free(&tx_mem_slab, buf);
        }

        print_debug();
    }

    return 0;
}