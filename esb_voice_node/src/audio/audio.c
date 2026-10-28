#include "audio.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/sys/printk.h>
#include <string.h>

static const struct device *i2s_dev = DEVICE_DT_GET(DT_NODELABEL(i2s0));

static struct i2s_config tx_cfg;
static struct i2s_config rx_cfg;

K_MEM_SLAB_DEFINE(tx_mem_slab, AUDIO_BLOCK_SIZE, AUDIO_BLOCK_COUNT, 4);
K_MEM_SLAB_DEFINE(rx_mem_slab, AUDIO_BLOCK_SIZE, AUDIO_BLOCK_COUNT, 4);

int audio_init(void)
{
        if (!device_is_ready(i2s_dev)) {
                printk("I2S device not ready\n");
                return -ENODEV;
        }

        tx_cfg.word_size = AUDIO_WORD_SIZE;
        tx_cfg.channels = 1;
        tx_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
        tx_cfg.options = I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER;
        tx_cfg.frame_clk_freq = SAMPLE_RATE;
        tx_cfg.mem_slab = &tx_mem_slab;
        tx_cfg.block_size = AUDIO_BLOCK_SIZE;
        tx_cfg.timeout = SYS_FOREVER_MS;

        int ret = i2s_configure(i2s_dev, I2S_DIR_TX, &tx_cfg);
        if (ret) return ret;

        rx_cfg.word_size = AUDIO_WORD_SIZE;
        rx_cfg.channels = 1;
        rx_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
        rx_cfg.options = I2S_OPT_BIT_CLK_CONTROLLER | I2S_OPT_FRAME_CLK_CONTROLLER;
        rx_cfg.frame_clk_freq = SAMPLE_RATE;
        rx_cfg.mem_slab = &rx_mem_slab;
        rx_cfg.block_size = AUDIO_BLOCK_SIZE;
        rx_cfg.timeout = SYS_FOREVER_MS;

        return i2s_configure(i2s_dev, I2S_DIR_RX, &rx_cfg);
}

int audio_start(void)
{
        return i2s_trigger(i2s_dev, I2S_DIR_BOTH, I2S_TRIGGER_START);
}

void audio_preload_silence(void)
{
        for (int i = 0; i < AUDIO_BLOCK_COUNT; i++) {
                void *buf;
                k_mem_slab_alloc(&tx_mem_slab, &buf, K_FOREVER);
                memset(buf, 0, AUDIO_BLOCK_SIZE);
                i2s_write(i2s_dev, buf, AUDIO_BLOCK_SIZE);
        }
}

int  audio_read(void **buf, size_t *size) { return i2s_read(i2s_dev, buf, size); }
void audio_free_rx(void *buf)             { k_mem_slab_free(&rx_mem_slab, buf); }
int  audio_alloc_tx(void **buf)           { return k_mem_slab_alloc(&tx_mem_slab, buf, K_FOREVER); }
int  audio_write(void *buf, size_t size)  { return i2s_write(i2s_dev, buf, size); }
void audio_free_tx(void *buf)             { k_mem_slab_free(&tx_mem_slab, buf); }