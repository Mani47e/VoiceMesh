#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#include <zephyr/sys/util.h>
#include <esb.h>

/* ---- Audio config (matches loopback) ---- */
#define SAMPLE_RATE            8000
#define AUDIO_WORD_SIZE         24
#define AUDIO_BLOCK_COUNT        4
#define AUDIO_SAMPLES_PER_BLOCK 512
#define AUDIO_BLOCK_SIZE        (sizeof(int32_t) * AUDIO_SAMPLES_PER_BLOCK) /* 2048, raw I2S block */
#define AUDIO16_BLOCK_SIZE      (sizeof(int16_t) * AUDIO_SAMPLES_PER_BLOCK) /* 1024, after shrink */

/* ---- Gain applied after shrinking to 16-bit, since raw mic amplitude
 *      is much smaller than full scale -- tune this if too quiet/clipping ---- */
#define MIC_GAIN 8

/* ---- ESB chunking ---- */
#define ESB_CHUNK_DATA_SIZE      128   /* bytes of audio per radio packet */
#define SAMPLES_PER_CHUNK        (ESB_CHUNK_DATA_SIZE / sizeof(int16_t))       /* 64 */
#define CHUNKS_PER_BLOCK         (AUDIO_SAMPLES_PER_BLOCK / SAMPLES_PER_CHUNK) /* 8 */

struct chunk_hdr {
    uint8_t chunk_idx;
    uint8_t total_chunks;
};
#define CHUNK_PKT_SIZE (sizeof(struct chunk_hdr) + ESB_CHUNK_DATA_SIZE) /* 130 */

/* ---- Device / config structs ---- */
const struct device *i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));
struct i2s_config rx_cfg;

K_MEM_SLAB_DEFINE(rx_mem_slab, AUDIO_BLOCK_SIZE, AUDIO_BLOCK_COUNT, 4);
K_MEM_SLAB_DEFINE(audio16_mem_slab, AUDIO16_BLOCK_SIZE, AUDIO_BLOCK_COUNT, 4);

K_MSGQ_DEFINE(audio_data_queue, sizeof(void *), AUDIO_BLOCK_COUNT, 4);

K_SEM_DEFINE(i2s_rx_ready_sem, 0, 1);
K_SEM_DEFINE(esb_ready_sem, 0, 1);

static bool ready = true;
static struct esb_payload rx_payload;
static struct esb_payload tx_payload;

/* ---- ESB event handler ---- */
static void event_handler(struct esb_evt const *event)
{
    ready = true;

    switch (event->evt_id) {
    case ESB_EVENT_TX_SUCCESS:
        break;
    case ESB_EVENT_TX_FAILED:
        printk("[TX] radio ack failed\n");
        break;
    case ESB_EVENT_RX_RECEIVED:
        while (esb_read_rx_payload(&rx_payload) == 0) {
            /* PTX doesn't expect data back in this demo; drain and discard */
        }
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
    config.mode = ESB_MODE_PTX;
    config.selective_auto_ack = false;

    err = esb_init(&config);
    if (err) return err;

    err = esb_set_base_address_0(base_addr_0);
    if (err) return err;

    err = esb_set_base_address_1(base_addr_1);
    if (err) return err;

    err = esb_set_prefixes(addr_prefix, ARRAY_SIZE(addr_prefix));
    if (err) return err;

    return esb_set_rf_channel(2); /* must match receiver exactly */
}

/* ---- I2S RX config -- matches loopback's RX block exactly ---- */
static int configure_i2s_rx(void)
{
    if (!device_is_ready(i2s_dev)) {
        printk("I2S not ready\n");
        return -ENODEV;
    }
    printk("I2S ready\n");

    rx_cfg.word_size = AUDIO_WORD_SIZE;
    rx_cfg.channels = 1;
    rx_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
    rx_cfg.options = I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER;
    rx_cfg.frame_clk_freq = SAMPLE_RATE;
    rx_cfg.mem_slab = &rx_mem_slab;
    rx_cfg.block_size = AUDIO_BLOCK_SIZE;
    rx_cfg.timeout = SYS_FOREVER_MS;

    int ret = i2s_configure(i2s_dev, I2S_DIR_RX, &rx_cfg);
    if (ret) {
        printk("I2S RX configure failed: %d\n", ret);
        return ret;
    }
    printk("I2S RX configured\n");
    return 0;
}

/* ---- Mic thread: read 32-bit I2S block, shrink+gain to 16-bit, queue it ---- */
static void i2s_read_task(void *p1, void *p2, void *p3)
{
    k_sem_take(&i2s_rx_ready_sem, K_FOREVER);
    printk("RX thread started\n");

    void *read_buf;
    size_t read_size;

    while (1) {
        int ret = i2s_read(i2s_dev, &read_buf, &read_size);
        if (ret) {
            printk("Read failed: %d\n", ret);
            return;
        }

        void *out_buf;
        if (k_mem_slab_alloc(&audio16_mem_slab, &out_buf, K_NO_WAIT) != 0) {
            printk("16-bit slab full, dropping block\n");
            k_mem_slab_free(&rx_mem_slab, read_buf);
            continue;
        }

        /* Shrink 32-bit -> 16-bit (same bit alignment as loopback's ulaw path),
         * then apply gain since raw amplitude alone was too quiet */
        int32_t *in = (int32_t *)read_buf;
        int16_t *out = (int16_t *)out_buf;
        for (int i = 0; i < AUDIO_SAMPLES_PER_BLOCK; i++) {
            int32_t shifted = in[i] >> 16;
            int32_t boosted = shifted * MIC_GAIN;
            if (boosted > 32767) boosted = 32767;
            if (boosted < -32768) boosted = -32768;
            out[i] = (int16_t)boosted;
        }

        /* --- debug --- */
        //printk("[TX] block samples: %d %d %d %d\n", out[0], out[1], out[2], out[3]);

        k_mem_slab_free(&rx_mem_slab, read_buf);

        k_msgq_put(&audio_data_queue, &out_buf, K_FOREVER);
    }
}

/* ---- ESB send thread: chunk each 16-bit block into radio packets ---- */
static void audio_esb_send_task(void *p1, void *p2, void *p3)
{
    k_sem_take(&esb_ready_sem, K_FOREVER);
    printk("ESB send thread started\n");

    void *block;

    while (1) {
        k_msgq_get(&audio_data_queue, &block, K_FOREVER);
        uint8_t *p = (uint8_t *)block;

        for (int i = 0; i < CHUNKS_PER_BLOCK; i++) {
            struct chunk_hdr hdr = { .chunk_idx = i, .total_chunks = CHUNKS_PER_BLOCK };

            tx_payload.length = sizeof(hdr) + ESB_CHUNK_DATA_SIZE;
            memcpy(tx_payload.data, &hdr, sizeof(hdr));
            memcpy(tx_payload.data + sizeof(hdr), p + (i * ESB_CHUNK_DATA_SIZE), ESB_CHUNK_DATA_SIZE);

            while (!ready) {
                k_yield();
            }
            ready = false;

            int err = esb_write_payload(&tx_payload);
            if (err) {
                printk("[TX] chunk %d send failed: %d\n", i, err);
                esb_flush_tx(); /* clear stuck FIFO so next chunk isn't blocked too */
                ready = true;
            }
        }

        k_mem_slab_free(&audio16_mem_slab, block);
    }
}

int main(void)
{
    k_msleep(1000);
    printk("SENDER START\n");

    if (configure_i2s_rx()) {
        return 0;
    }

    int err = esb_initialize();
    printk("ESB init result: %d\n", err);
    if (err) {
        printk("ESB init failed: %d\n", err);
        return 0;
    }

    int ret = i2s_trigger(i2s_dev, I2S_DIR_RX, I2S_TRIGGER_START);
    printk("I2S RX trigger: %d\n", ret);

    k_sem_give(&i2s_rx_ready_sem);
    k_sem_give(&esb_ready_sem);

    while (1) {
        k_sleep(K_SECONDS(1));
    }
    return 0;
}

K_THREAD_DEFINE(i2s_read_task_id, 4096, i2s_read_task, NULL, NULL, NULL, 5, 0, 0);
K_THREAD_DEFINE(audio_esb_send_task_id, 4096, audio_esb_send_task, NULL, NULL, NULL, 5, 0, 0);