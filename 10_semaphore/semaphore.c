#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>   
#include <linux/err.h>      
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>

#define GPIOLED_CNT     1
#define GPIOLED_NAME    "gpioled"
#define GPIOLED_ON      1
#define GPIOLED_OFF     0


/* gpio 设备结构体 */
struct gpioled_dev{
    dev_t               devid;
    int                 major;
    int                 minor;
    struct cdev         cdev;
    struct class        *class;
    struct device       *device;
    struct device_node  *nd;
    int                 led_gpio;
    
    struct semaphore    sem;        /* 信号量 */

};

struct gpioled_dev gpioled;     /* LED */

static int gpioled_open(struct inode *inode, struct file *file)
{
    file->private_data = &gpioled;

    down(&gpioled.sem);        /* 下降信号量 */


    return 0;
}

static ssize_t gpioled_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{

    int ret = 0;
    unsigned char databuf[1];
    struct gpioled_dev *dev = file->private_data;

    ret = copy_from_user(databuf, buf, count);
    if(ret < 0)
    {
        printk(KERN_ERR "Failed to copy data from user space\n");
        return ret;
    }

    if(databuf[0] == GPIOLED_ON)
    {
        gpio_set_value(dev->led_gpio, 0);
    }
    else if(databuf[0] == GPIOLED_OFF)
    {
        gpio_set_value(dev->led_gpio, 1);
    }

    
    return 0;
}

static int gpioled_release(struct inode *inode, struct file *file)
{
    struct gpioled_dev *dev = file->private_data;

    up(&dev->sem);        /* 上升信号量 */

    return 0;
}

/* 字符操作集 */
static struct file_operations gpioled_fops = {
    .owner      = THIS_MODULE,
    .open       = gpioled_open,
    .release    = gpioled_release,
    .write      = gpioled_write,
};


static int __init led_init(void)
{
    /* 注册字符驱动设备 */
    int ret = 0;

    gpioled.major = 0;

    /* 初始化信号量 */
    sema_init(&gpioled.sem, 1);


    if(gpioled.major)   /*给定设备号*/
    {
        gpioled.devid = MKDEV(gpioled.major, 0);
        ret = register_chrdev_region(gpioled.devid, GPIOLED_CNT, GPIOLED_NAME);
    }
    else        /*没给定设备号*/
    {
        ret = alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME);
        gpioled.major = MAJOR(gpioled.devid);
        gpioled.minor = MINOR(gpioled.devid);
    }


    if(ret < 0)
    {
        printk(KERN_ERR "Failed to register device\n");
        goto fail_devid;
    }

    /* 初始化 cdev*/
    cdev_init(&gpioled.cdev, &gpioled_fops);

    /* 添加 cdev */
    ret = cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);
    if(ret < 0)
    {
        printk(KERN_ERR "Failed to add cdev\n");
        goto fail_cdev_add;
    }

    /* 创建类 class */
    gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
    if(IS_ERR(gpioled.class))
    {
        printk(KERN_ERR "Failed to create class\n");
        ret = PTR_ERR(gpioled.class);
        goto fail_class_create;
    }

    gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
    if(IS_ERR(gpioled.device))
    {
        printk(KERN_ERR "Failed to create device\n");
        ret = PTR_ERR(gpioled.device);
        goto fail_device_create;
    }
    
    /* 获取设备节点 */
    gpioled.nd = of_find_node_by_path("/gpioled");
    if(gpioled.nd == NULL)
    {
        printk(KERN_ERR "Failed to find node\n");
        ret = -ENODEV;
        goto fail_node;
    }

    /* 获取gpio的属性 */
    gpioled.led_gpio = of_get_named_gpio(gpioled.nd, "led-gpios", 0);
    if(gpioled.led_gpio < 0)
    {
        printk(KERN_ERR "Failed to get led-gpios\n");
        ret = gpioled.led_gpio;
        goto fail_led_gpio;
    }

    printk("led_gpio num = %d \r\n", gpioled.led_gpio);

    /* 申请gpio */
    ret = gpio_request(gpioled.led_gpio, "led-gpios");
    if(ret < 0)
    {
        printk(KERN_ERR "Failed to request gpio\n");
        goto fail_led_gpio;
        ret = -ENODEV;
    }

    /* 使用IO */
    ret = gpio_direction_output(gpioled.led_gpio, 1);
    if(ret < 0)
    {
        printk(KERN_ERR "Failed to set gpio direction\n");
        goto fail_led_gpio;
    }

    /* 输出低电平 点亮LED*/
    gpio_set_value(gpioled.led_gpio, 0);

    printk(KERN_INFO "led_init successful!!!\r\n");
    return 0;



fail_led_gpio:
    gpio_free(gpioled.led_gpio);

fail_node:
    of_node_put(gpioled.nd);

fail_device_create:
    device_destroy(gpioled.class, gpioled.devid);       /* 删除设备 */

fail_class_create:
    class_destroy(gpioled.class);       /* 删除类 */

fail_cdev_add:
    cdev_del(&gpioled.cdev);        /* 删除 cdev */
    
fail_devid:
    unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);   /* 注销设备号 */
    return ret;

}

static void __exit led_exit(void)
{
    gpio_set_value(gpioled.led_gpio, 1);

    /* 删除 cdev */
    cdev_del(&gpioled.cdev);
    unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);

    device_destroy(gpioled.class, gpioled.devid);
    class_destroy(gpioled.class);

    /* 释放gpio */
    gpio_free(gpioled.led_gpio);
}


module_init(led_init);
module_exit(led_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("xyl");



