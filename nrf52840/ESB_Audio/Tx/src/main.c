#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/sys/util.h>
#include <esb.h>

#define SAMPLE_RATE            8000
#define AUDIO_WORD_SIZE         24
#define AUDIO_BLOCK_COUNT        8
#define AUDIO_SAMPLES_PER_BLOCK 512
#define AUDIO_BLOCK_SIZE        (sizeof(int32_t) * AUDIO_SAMPLES_PER_BLOCK)

#define MIC_GAIN 8   

#define ESB_CHUNK_DATA_SIZE      128
#define SAMPLES_PER_CHUNK        (ESB_CHUNK_DATA_SIZE / sizeof(int16_t))       /* 64 */
#define CHUNKS_PER_BLOCK         (AUDIO_SAMPLES_PER_BLOCK / SAMPLES_PER_CHUNK) /* 8 */

#define BUTTON_NODE DT_ALIAS(sw0)


struct chunk_hdr {
    uint8_t seq;
    uint8_t reserved;
};
#define CHUNK_PKT_SIZE (sizeof(struct chunk_hdr) + ESB_CHUNK_DATA_SIZE) /* 130 */

//dt

const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ssd1306));

const struct device *i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));
struct i2s_config rx_cfg;

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(BUTTON_NODE,gpios);

K_MEM_SLAB_DEFINE(rx_mem_slab, AUDIO_BLOCK_SIZE, AUDIO_BLOCK_COUNT, 4);

static bool ready = true;
static struct esb_payload rx_payload;
static struct esb_payload tx_payload;
static uint8_t seq_counter = 0;


static uint32_t dbg_esb_sent = 0;
static uint32_t dbg_esb_failed = 0;
static int32_t  dbg_mic_peak = 0;

static void print_debug(void)
{
    static int64_t last = 0;
    int64_t now = k_uptime_get();
    if (now - last < 2000) return;
    last = now;
    printk("---------------------------------\n");
    printk("[MIC] peak(after gain)=%d\n", dbg_mic_peak);
    printk("[ESB] sent=%u failed=%u\n", dbg_esb_sent, dbg_esb_failed);
    printk("---------------------------------\n");

   char line1[32], line2[32];

    snprintf(line1, sizeof(line1), "MIC peak=%d", dbg_mic_peak);
    snprintf(line2, sizeof(line2), "ESB send=%ufailed=%u", dbg_esb_sent, dbg_esb_failed);

    cfb_framebuffer_clear(dev, true);
    cfb_print(dev, line1, 0, 0);
    cfb_print(dev, line2, 0, 16);   
    cfb_framebuffer_finalize(dev);
    dbg_esb_sent = 0;
    dbg_esb_failed = 0;
    dbg_mic_peak = 0;
}

static void event_handler(struct esb_evt const *event)
{
    ready = true;
    switch (event->evt_id) {
    case ESB_EVENT_TX_SUCCESS:
        break;
    case ESB_EVENT_TX_FAILED:
        dbg_esb_failed++;
        break;
    case ESB_EVENT_RX_RECEIVED:
        while (esb_read_rx_payload(&rx_payload) == 0) {
            /* PTX doesn't expect data back; drain and discard */
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

   if (!gpio_is_ready_dt(&button)) {
    printk("button not ready\n");
    return 0;
    }
    int gret = gpio_pin_configure_dt(&button, GPIO_INPUT);
    if (gret) {
        printk("Button gpio configure failed: %d\n", gret);
        return 0;
    }

    //i2s_trigger(i2s_dev, I2S_DIR_RX, I2S_TRIGGER_START);

    static int16_t shrunk[AUDIO_SAMPLES_PER_BLOCK];

    //display initialize

    if (!device_is_ready(dev)) {
        printk("Display device not ready\n");
    }

    if (cfb_framebuffer_init(dev)) {
        printk("Framebuffer init failed\n");
    }

    cfb_framebuffer_clear(dev, true);
    display_blanking_off(dev);

    static bool was_talking = false;
    while (1) {
        void *read_buf;
        size_t read_size;
        bool talking = gpio_pin_get_dt(&button);

        if(talking && !was_talking){
            i2s_trigger(i2s_dev, I2S_DIR_RX, I2S_TRIGGER_START);

            printk("Mic started\n");
        }
        else if (!talking && was_talking){
            i2s_trigger(i2s_dev, I2S_DIR_RX, I2S_TRIGGER_STOP);
            printk("Mic stopped\n");
        }
        was_talking = talking;

        if(!talking){
            k_msleep(100);
            continue;
        }

        int r = i2s_read(i2s_dev, &read_buf, &read_size);
        if (r) {
            printk("i2s_read failed: %d\n", r);
            continue;
        }

        int32_t *in = (int32_t *)read_buf;
        for (int i = 0; i < AUDIO_SAMPLES_PER_BLOCK; i++) {
            int32_t shifted = in[i] >> 16;
            int32_t boosted = shifted * MIC_GAIN;
            if (boosted > 32767) boosted = 32767;
            if (boosted < -32768) boosted = -32768;
            shrunk[i] = (int16_t)boosted;

            int32_t mag = boosted < 0 ? -boosted : boosted;
            if (mag > dbg_mic_peak) dbg_mic_peak = mag;
        }
        k_mem_slab_free(&rx_mem_slab, read_buf);

        uint8_t *p = (uint8_t *)shrunk;
        for (int c = 0; c < CHUNKS_PER_BLOCK; c++) {
            struct chunk_hdr hdr = { .seq = seq_counter++, .reserved = 0 };

            tx_payload.length = sizeof(hdr) + ESB_CHUNK_DATA_SIZE;
            memcpy(tx_payload.data, &hdr, sizeof(hdr));
            memcpy(tx_payload.data + sizeof(hdr), p + (c * ESB_CHUNK_DATA_SIZE), ESB_CHUNK_DATA_SIZE);

            while (!ready) { k_yield(); }
            ready = false;

            int werr = esb_write_payload(&tx_payload);
            if (werr) {
                esb_flush_tx();
                ready = true;
                dbg_esb_failed++;
            } else {
                dbg_esb_sent++;
            }
        }

        print_debug();
    
    }

    return 0;
}