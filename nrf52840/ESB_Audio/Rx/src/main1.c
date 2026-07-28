#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#include <zephyr/sys/util.h>
#include <esb.h>

/* ---- Audio config (must match sender) ---- */
#define SAMPLE_RATE            8000
#define AUDIO_WORD_SIZE         24
#define AUDIO_BLOCK_COUNT        4
#define AUDIO_SAMPLES_PER_BLOCK 512
#define AUDIO_BLOCK_SIZE        (sizeof(int32_t) * AUDIO_SAMPLES_PER_BLOCK) /* 2048, for I2S TX/RX */

/* ---- ESB chunking (must match sender) ---- */
#define ESB_CHUNK_DATA_SIZE      128
#define SAMPLES_PER_CHUNK        (ESB_CHUNK_DATA_SIZE / sizeof(int16_t))       /* 64 */
#define CHUNKS_PER_BLOCK         (AUDIO_SAMPLES_PER_BLOCK / SAMPLES_PER_CHUNK) /* 8 */

struct chunk_hdr {
    uint8_t chunk_idx;
    uint8_t total_chunks;
};
#define CHUNK_PKT_SIZE (sizeof(struct chunk_hdr) + ESB_CHUNK_DATA_SIZE) /* 130 */

/* ---- Device / config ---- */
const struct device *i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));
struct i2s_config tx_cfg;
struct i2s_config rx_cfg; /* dummy RX -- kept active to match the loopback's I2S_DIR_BOTH pattern,
                              which is the config that was confirmed working on this hardware */

K_MEM_SLAB_DEFINE(tx_mem_slab, AUDIO_BLOCK_SIZE, AUDIO_BLOCK_COUNT, 4);
K_MEM_SLAB_DEFINE(rx_mem_slab, AUDIO_BLOCK_SIZE, AUDIO_BLOCK_COUNT, 4); /* dummy RX slab, drained/discarded */

K_MSGQ_DEFINE(esb_rx_queue, CHUNK_PKT_SIZE, 16, 4);
K_MSGQ_DEFINE(audio_play_queue, sizeof(void *), AUDIO_BLOCK_COUNT, 4);

K_SEM_DEFINE(i2s_rx_dummy_ready_sem, 0, 1);
K_SEM_DEFINE(esb_ready_sem, 0, 1);
K_SEM_DEFINE(play_ready_sem, 0, 1);

static struct esb_payload rx_payload;

/* ---- ESB event handler (radio ISR context — keep fast, no blocking calls) ---- */
static void event_handler(struct esb_evt const *event)
{
    switch (event->evt_id) {
    case ESB_EVENT_RX_RECEIVED:
        while (esb_read_rx_payload(&rx_payload) == 0) {
            if (rx_payload.length == CHUNK_PKT_SIZE) {
                // struct chunk_hdr *h = (struct chunk_hdr *)rx_payload.data;
                // int16_t *d = (int16_t *)(rx_payload.data + sizeof(struct chunk_hdr));
                // printk("[RX] chunk %d/%d first samples: %d %d %d\n",
                //        h->chunk_idx, h->total_chunks, d[0], d[1], d[2]);

                k_msgq_put(&esb_rx_queue, rx_payload.data, K_NO_WAIT);
            } else {
                printk("[RX] unexpected payload length: %d\n", rx_payload.length);
            }
        }
        break;
    case ESB_EVENT_TX_SUCCESS:
    case ESB_EVENT_TX_FAILED:
        break; /* PRX doesn't TX in this demo */
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

/* ---- I2S config -- both directions, matching loopback exactly ---- */
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

/* ---- Dummy RX drain thread: keeps the RX side alive/fed so I2S_DIR_BOTH
 *      stays stable, matching the config that was confirmed working ---- */
static void rx_drain_task(void *p1, void *p2, void *p3)
{
    k_sem_take(&i2s_rx_dummy_ready_sem, K_FOREVER);
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

/* ---- Reassembly thread: radio chunks -> 32-bit expanded I2S block.
 *      Partial-tolerant: queues whatever it has as soon as the NEXT
 *      block's chunk 0 arrives, instead of requiring every chunk. ---- */
static void reassemble_task(void *p1, void *p2, void *p3)
{
    k_sem_take(&esb_ready_sem, K_FOREVER);
    printk("Reassembly thread started\n");

    void *block = NULL;
    uint8_t raw[CHUNK_PKT_SIZE];

    while (1) {
        k_msgq_get(&esb_rx_queue, raw, K_FOREVER);

        struct chunk_hdr hdr;
        memcpy(&hdr, raw, sizeof(hdr));
        int16_t *chunk_samples = (int16_t *)(raw + sizeof(hdr));

        if (hdr.chunk_idx >= CHUNKS_PER_BLOCK || hdr.total_chunks != CHUNKS_PER_BLOCK) {
            continue; /* malformed packet */
        }

        if (hdr.chunk_idx == 0) {
            if (block) {
                // int32_t *out = (int32_t *)block;
                // printk("[RX] block queued, samples: %d %d %d %d\n",
                //        out[0], out[1], out[2], out[3]);
                k_msgq_put(&audio_play_queue, &block, K_NO_WAIT);
                block = NULL;
            }
            if (k_mem_slab_alloc(&tx_mem_slab, &block, K_NO_WAIT) != 0) {
                block = NULL; /* slab full, skip this block */
            } else {
                memset(block, 0, AUDIO_BLOCK_SIZE); /* missing chunks default to silence */
            }
        }

        if (block) {
            /* Expand 16-bit -> 32-bit, same bit alignment as loopback's ulaw decode */
            int32_t *out = (int32_t *)block;
            int base = hdr.chunk_idx * SAMPLES_PER_CHUNK;
            for (int i = 0; i < SAMPLES_PER_CHUNK; i++) {
                out[base + i] = ((int32_t)chunk_samples[i]) << 16;
            }
        }
    }
}

/* ---- Playback thread: pulls finished blocks, falls back to silence on timeout ---- */
static void play_task(void *p1, void *p2, void *p3)
{
    k_sem_take(&play_ready_sem, K_FOREVER);
    printk("Playback thread started\n");

    void *block;
    const int32_t block_ms = (AUDIO_SAMPLES_PER_BLOCK * 1000) / SAMPLE_RATE; /* 64ms */

    while (1) {
        int ret = k_msgq_get(&audio_play_queue, &block, K_MSEC(block_ms / 2));

        if (ret != 0) {
            if (k_mem_slab_alloc(&tx_mem_slab, &block, K_NO_WAIT) != 0) {
                continue;
            }
            memset(block, 0, AUDIO_BLOCK_SIZE);
        }

        int w = i2s_write(i2s_dev, block, AUDIO_BLOCK_SIZE);
        if (w) {
            printk("i2s_write failed: %d\n", w);
            k_mem_slab_free(&tx_mem_slab, block);
        }
    }
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

    k_sem_give(&i2s_rx_dummy_ready_sem);
    k_sem_give(&esb_ready_sem);
    k_sem_give(&play_ready_sem);

    while (1) {
        k_sleep(K_SECONDS(1));
    }
    return 0;
}

K_THREAD_DEFINE(rx_drain_task_id, 2048, rx_drain_task, NULL, NULL, NULL, 6, 0, 0);
K_THREAD_DEFINE(reassemble_task_id, 4096, reassemble_task, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(play_task_id, 4096, play_task, NULL, NULL, NULL, 4, 0, 0);