#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/sys/printk.h>

const struct device *i2s_dev =
                        DEVICE_DT_GET(DT_NODELABEL(i2s0));

K_MEM_SLAB_DEFINE(mem_slab , 2048,4,4);

int main(void)
{       
        k_msleep(2000);
        printk("APP START");

        if (!device_is_ready(i2s_dev)) {
                printk("I2S not ready\n");
                return 0;
        }
        printk("I2S ready\n");
 
        struct i2s_config cfg;

        cfg.word_size = 24;
        cfg.channels = 1;
        cfg.format = I2S_FMT_DATA_FORMAT_I2S;
        cfg.options = I2S_OPT_BIT_CLK_CONTROLLER|I2S_OPT_FRAME_CLK_CONTROLLER;
        cfg.frame_clk_freq = 8000;
        cfg.mem_slab = &mem_slab;
        cfg.block_size = 2048;
        cfg.timeout = SYS_FOREVER_MS;

        int conf_ret = i2s_configure(i2s_dev,I2S_DIR_RX,&cfg);
        printk("I2S configure : %d\n",conf_ret);

        int trig_ret = i2s_trigger(i2s_dev,I2S_DIR_RX,I2S_TRIGGER_START);
        printk("Trigger return : %d\n",trig_ret);

        void *read_buf;
        size_t read_size;

        /*i2s_read instead of i2s_buf_read ,since i2s_read directly give the pointer whereas 
        i2s_buf_read copies into the buffer and frees takes more time instead of directly acessing by pointer*/
        while(1){
                int ret = i2s_read(i2s_dev,&read_buf,&read_size);
                if (ret) {
                printk("Read failed: %d\n", ret);
                return 0;
                }

                int32_t *samples = (int32_t *)read_buf;
                int count = read_size / sizeof(int16_t);

                for (int i = 0; i < count; i+=2) {
                        
                        printk("%d\n",samples[i]);
                }

                k_mem_slab_free(&mem_slab, read_buf);
                k_msleep(5);
        }




}
