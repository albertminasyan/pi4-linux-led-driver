/*
 * led_driver.c
 *
 *  Created on: 15 Sept 2026
 *      Author: albert.minasyan@gmail.com
 */

// In your workspace:
// cd ~/eclipse-workspace/pi4-linux-led-driver
// make -C ~/eclipse-workspace/linux-mainline M=$(pwd) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules

// ON Raspberry Pi4b:
// sudo insmod led_driver.ko
// sudo cat /sys/kernel/debug/gpio | grep -A1 "gpio-17"   # should show "out lo" (init state)
// echo -n 1 | sudo tee /dev/led0   # LED on
// sudo cat /sys/kernel/debug/gpio | grep -A1 "gpio-17"   # should now show "out hi"
// echo -n 0 | sudo tee /dev/led0   # LED off
// sudo cat /sys/kernel/debug/gpio | grep -A1 "gpio-17"   # should show "out lo"


/* led_driver.c */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>

MODULE_LICENSE("GPL");

#define LED_GPIO 17

static struct gpio_desc *led_gpio;

static struct gpiod_lookup_table led_lookup = {
//    .dev_id = "led_driver",
    .dev_id = NULL,
    .table = {
//        GPIO_LOOKUP("gpiochip0", LED_GPIO, "led", GPIO_ACTIVE_HIGH),
        GPIO_LOOKUP("pinctrl-bcm2711", LED_GPIO, "led", GPIO_ACTIVE_HIGH),
        { }
    },
};

/*
 * led_write() - handler for write() on /dev/led0
 *
 * Called whenever userspace writes to /dev/led0 (e.g. via
 * `echo -n 1 | sudo tee /dev/led0`, or led_app). Copies exactly one
 * byte from the caller's buffer into kernel memory with
 * copy_from_user() (userspace pointers can't be dereferenced
 * directly from kernel code), then drives the LED's GPIO line high
 * if that byte is the ASCII character '1', low for anything else.
 * Returns the number of bytes consumed (len) to the caller.
 */
static ssize_t led_write(struct file *f, const char __user *buf,
                          size_t len, loff_t *off)
{
    char c;
    if (copy_from_user(&c, buf, 1))
        return -EFAULT;

    gpiod_set_value(led_gpio, c == '1' ? 1 : 0);
    return len;
}

/*
 * led_read() - handler for read() on /dev/led0
 *
 * Called whenever userspace reads from /dev/led0 (e.g. `cat /dev/led0`).
 * Reads the GPIO's current logic level with gpiod_get_value(),
 * converts it to the ASCII character '1' or '0', and copies that
 * single byte back to userspace with copy_to_user(). The
 * `*off > 0` check makes a second read() in the same session return
 * 0 (end-of-file) instead of the same byte forever, so tools like
 * cat don't loop indefinitely.
 */
static ssize_t led_read(struct file *f, char __user *buf,
                         size_t len, loff_t *off)
{
    char c = gpiod_get_value(led_gpio) ? '1' : '0';
    if (*off > 0) return 0;
    if (copy_to_user(buf, &c, 1)) return -EFAULT;
    *off += 1;
    return 1;
}

static const struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .write = led_write,
    .read  = led_read,
};

static struct miscdevice led_misc = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = "led0",
    .fops  = &led_fops,
};

/*
 * led_init() - module entry point, run once when insmod loads the driver
 *
 * 1. Registers the manual GPIO lookup table (led_lookup) so that
 *    gpiod_get() below can resolve the name "led" to a physical pin,
 *    without needing a device-tree overlay.
 * 2. Requests that GPIO line as an output, initially low, storing
 *    the resulting descriptor in the global led_gpio.
 * 3. If that fails (IS_ERR), logs an error, undoes the lookup-table
 *    registration, and aborts the module load with the error code.
 * 4. Otherwise registers the misc device (led_misc) to create
 *    /dev/led0 and wire up led_fops. If that also fails, releases
 *    the GPIO and lookup table before returning the error.
 * 5. On success, logs "loaded, /dev/led0 ready". The __init tag
 *    hints to the kernel that this function's code can be placed in
 *    a memory-reclaimable section once initialization completes.
 */
static int __init led_init(void)
{
    int ret;

    gpiod_add_lookup_table(&led_lookup);

    led_gpio = gpiod_get(NULL, "led", GPIOD_OUT_LOW);

    if (IS_ERR(led_gpio))
    {
        pr_err("led_driver: failed to get GPIO\n");
        gpiod_remove_lookup_table(&led_lookup);
        return PTR_ERR(led_gpio);
    }

    ret = misc_register(&led_misc);

    if (ret)
    {
        gpiod_put(led_gpio);
        gpiod_remove_lookup_table(&led_lookup);
        return ret;
    }

    pr_info("led_driver: loaded, /dev/led0 ready\n");

    return 0;
}

/*
 * led_exit() - module cleanup routine, run when rmmod unloads the driver
 *
 * Tears down everything led_init() set up, in reverse order:
 * removes /dev/led0, releases the GPIO line back to the kernel, and
 * unregisters the lookup table. Skipping this cleanup, or doing it
 * out of order, is a common source of resource leaks and
 * repeat-insmod GPIO acquisition failures.
 */
static void __exit led_exit(void)
{
    misc_deregister(&led_misc);
    gpiod_put(led_gpio);
    gpiod_remove_lookup_table(&led_lookup);
    pr_info("led_driver: unloaded\n");
}

/* Register led_init/led_exit as the module's load/unload entry points. */
module_init(led_init);
module_exit(led_exit);


