THIS IS TO CHECK THIS THAT I HAVE TO LEARN

* Mem Slabs how are they structured and how to use them

* Understand why i used these:
    tx_cfg.channels = 1;
    tx_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
    tx_cfg.options = I2S_OPT_BIT_CLK_CONTROLLER|I2S_OPT_FRAME_CLK_CONTROLLER;

    rx_cfg.channels = 1;
    rx_cfg.format = I2S_FMT_DATA_FORMAT_I2S;
    rx_cfg.options = 0;

    why are they different for TX and RX


why am i using MSG queue instead of normal queue?



also how does the read work?Is it like when i trigger the i2s the RX_mem_slab starts filling up and the i2s_read just copies the data onto the readbuf and readsize values? how are their sizes configured?



does k_msg_get just puts the pointer or copies the data?



can the pointer of the mem slab blocks be random?cuz i am dirctly using the write_buf



also arent write and read_buf always same pointer and thts makes use only 1 block? we can never use multiple blocks in this code?



why triggering tx and rx both instead of speratly trigger?

* Working of the i2s_read_task



* * ANS:
        TX is configured as bit-clock and frame-clock controller (master), but RX is left as 0 (target/slave for both clocks). On most SoCs the I2S peripheral shares one clock generator between RX and TX — you don't get two independent clock domains — so this mismatch is likely to cause RX to never see valid clocks, or the driver to reject/ignore one of the configs. Unless your specific SoC's I2S driver genuinely supports asymmetric master/slave role per direction (rare), RX should mirror TX:
    
