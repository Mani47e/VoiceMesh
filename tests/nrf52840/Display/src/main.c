#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/display/cfb.h>
#include <zephyr/sys/printk.h>

int main(void)
{
    const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(ssd1306));

    if (!device_is_ready(dev)) {
        printk("Display device not ready\n");
        return 0;
    }

    if (cfb_framebuffer_init(dev)) {
        printk("Framebuffer init failed\n");
        return 0;
    }

    cfb_framebuffer_clear(dev, true);
    display_blanking_off(dev);

    

    //cfb_framebuffer_finalize(dev);

    printk("Text displayed\n");

    while (1) {
        cfb_print(dev, "Hello 1", 0, 0);
        cfb_framebuffer_finalize(dev);
        k_msleep(100);
        cfb_print(dev, "Hello 2", 0, 0);
        cfb_framebuffer_finalize(dev);
        k_msleep(100);
    }
}