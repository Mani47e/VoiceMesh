#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/sys/printk.h>
#include<string.h>

#define AUDIO_BLOCK_COUNT 4
#define AUDIO_SAMPLES_PER_BLOCK 512

const struct device *i2s_dev =
                        DEVICE_DT_GET(DT_NODELABEL(i2s0));

K_MEM_SLAB_DEFINE(rx_mem_slab , 2048,4,4);
K_MEM_SLAB_DEFINE(tx_mem_slab , 2048,4,4);

struct i2s_config rx_cfg;
struct i2s_config tx_cfg;


K_MSGQ_DEFINE(filled_queue, sizeof(void *), AUDIO_BLOCK_COUNT, 4);



K_SEM_DEFINE(i2s_rx_ready_sem, 0, 1);
K_SEM_DEFINE(i2s_tx_ready_sem,0,1);





static void configure_i2s_rxntx(){

        
        if (!device_is_ready(i2s_dev)) {
                printk("I2S not ready\n");
                return;
        }
        printk("I2S ready\n");

        //TX Config

        tx_cfg.word_size = 24;
        tx_cfg.channels = 1;
        tx_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
        tx_cfg.options = I2S_OPT_BIT_CLK_CONTROLLER|I2S_OPT_FRAME_CLK_CONTROLLER;
        tx_cfg.frame_clk_freq = 8000;
        tx_cfg.mem_slab = &tx_mem_slab;
        tx_cfg.block_size = 2048;
        tx_cfg.timeout = SYS_FOREVER_MS;

        int tx_conf_ret = i2s_configure(i2s_dev,I2S_DIR_TX,&tx_cfg);
        if(tx_conf_ret){
                printk("I2S TX Configure Failed");
                return;
        }
        printk("I2S TX configure : %d\n",tx_conf_ret);

        //RX Config
        rx_cfg.word_size = 24;
        rx_cfg.channels = 1;
        rx_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
        rx_cfg.options = 0;
        rx_cfg.frame_clk_freq = 8000;
        rx_cfg.mem_slab = &rx_mem_slab;
        rx_cfg.block_size = 2048;
        rx_cfg.timeout = SYS_FOREVER_MS;

        int rx_conf_ret = i2s_configure(i2s_dev,I2S_DIR_RX,&rx_cfg);
        if(rx_conf_ret){
                printk("I2S RX Configure Failed");
                return;
        }
        printk("I2S RX configure : %d\n",rx_conf_ret);

        return;
        
}

static void i2s_read_task(void *p1,void *p2,void *p3){

        printk("RX thread started before sem\n");

        k_sem_take(&i2s_rx_ready_sem, K_FOREVER); 

        printk("RX thread started after sem\n");

        void *read_buf;
        size_t read_size;
        /*i2s_read instead of i2s_buf_read ,since i2s_read directly give the pointer whereas 
        i2s_buf_read copies into the buffer and frees takes more time instead of directly acessing by pointer*/

        while(1){
                int ret = i2s_read(i2s_dev,&read_buf,&read_size);
                // printk("I2S_READ ret: %d\n ",ret);
                if (ret) {
                printk("Read failed: %d\n", ret);
                return ;
                }

                int32_t *samples = (int32_t *)read_buf;
                int count = read_size / sizeof(int32_t);
                printk("Count : %d\n",count);

                for (int i = 0; i < 20; i+=2) {
                        printk("%d\n",samples[i]);
                }

                k_msgq_put(&filled_queue,&read_buf,K_FOREVER);

                //Need to send to TX so donot free
                //k_mem_slab_free(&mem_slab, read_buf);
        }


}

static void i2s_write_task(void *p1,void *p2,void *p3){
        
        printk("TX thread started before sem\n");

        k_sem_take(&i2s_tx_ready_sem, K_FOREVER); 

        printk("TX thread started after sem\n");

        void *read_buf;
        void *write_buf;

        while(1){

                k_msgq_get(&filled_queue, &read_buf, K_FOREVER);

                //Allocate TX buffer
                int alloc_ret = k_mem_slab_alloc(&tx_mem_slab, &write_buf, K_FOREVER);
                if (alloc_ret) {
                printk("TX alloc failed: %d \n", alloc_ret);
                k_mem_slab_free(&rx_mem_slab, read_buf);
                continue;
                }

                memcpy(write_buf, read_buf, 2048);

                k_mem_slab_free(&rx_mem_slab, read_buf);

                int write_ret = i2s_write(i2s_dev,write_buf,2048);
                // printk("I2S_WRITE RET: %d \n",write_ret);
                if (write_ret) {
                printk("Write failed: %d\n", write_ret);
                k_mem_slab_free(&tx_mem_slab, write_buf);  // free it before continuing
                continue ;
                }

        }

}


int main(void)
{       
        printk("APP START");

        //Configure all I2S in this
        configure_i2s_rxntx();
        
        //Trigger Start here
        // Before triggering, pre-fill TX with silence
        // void *silence_buf;
        // k_mem_slab_alloc(&tx_mem_slab, &silence_buf, K_FOREVER);
        // memset(silence_buf, 0, 2048);
        // i2s_write(i2s_dev, silence_buf, 2048);  // queue it up first

        for (int i = 0; i < 4; i++) {
                void *buf;

                k_mem_slab_alloc(&tx_mem_slab, &buf, K_FOREVER);
                memset(buf, 0, 2048);

                int ret = i2s_write(i2s_dev, buf, 2048);
                printk("Initial write %d = %d\n", i, ret);
        }
        
        // int tx_trig_ret = i2s_trigger(i2s_dev,I2S_DIR_TX,I2S_TRIGGER_START);
        // printk("TX Trigger return : %d\n",tx_trig_ret);

        // k_msleep(10);

        // int rx_trig_ret = i2s_trigger(i2s_dev,I2S_DIR_RX,I2S_TRIGGER_START);
        // printk("RX Trigger return : %d\n",rx_trig_ret);

        
        
        int both_trig_ret = i2s_trigger(i2s_dev,I2S_DIR_BOTH,I2S_TRIGGER_START);
        printk("Both Trigger Return: %d\n",both_trig_ret);


        k_sem_give(&i2s_tx_ready_sem);
        k_sem_give(&i2s_rx_ready_sem);

        while (1) {
                k_sleep(K_SECONDS(1));
        }
        return 0;

}
                                                                                        
K_THREAD_DEFINE(i2s_read_task_id, 4096,i2s_read_task, NULL, NULL, NULL,5, 0, 0 /*delay*/);

K_THREAD_DEFINE(i2s_write_task_id, 4096,i2s_write_task, NULL, NULL, NULL,5, 0, 0 /*delay*/);