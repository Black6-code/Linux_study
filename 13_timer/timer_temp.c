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
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/ioctl.h>

#define CLOSE_CMD       _IO(0xEF, 1)        /* 关闭命令 */
#define OPEN_CMD        _IO(0xEF, 2)        /* 打开命令 */
#define SETPERIOD_CMD   _IOW(0xEF, 3, int)  /* 设置周期命令 */


#define TIMER_CNT     1
#define TIMER_NAME    "timer"
#define TIMER0VALUE   0xf0
#define INVATIMER     0


/* timer 设备结构体 */
struct timer_dev{
    dev_t               devid;
    int                 major;
    int                 minor;
    struct cdev         cdev;
    struct class        *class;
    struct device       *device;
    struct device_node  *nd;
    struct timer_list   timer;          /* 定时器 */
    int                 timeperiod;     /* 定时器周期 */
    int                 gpio_led;
};

struct timer_dev timerdev;     /* timer */

static int timer_open(struct inode *inode, struct file *file)
{
    file->private_data = &timerdev;
    return 0;
}


static ssize_t timer_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;

    return ret;
}

static long timer_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    int value = 0;
    struct timer_dev *dev = file->private_data;
    switch(cmd)
    {
        case CLOSE_CMD:
            del_timer_sync(&dev->timer);
        break;

        case OPEN_CMD:
            mod_timer(&dev->timer, jiffies + msecs_to_jiffies(dev->timeperiod));
        break;

        case SETPERIOD_CMD:
            ret = copy_from_user(&value, (int __user *)arg, sizeof(int));
            if(ret < 0)
            {
                return -EFAULT;
            }
            dev->timeperiod = value;
            mod_timer(&dev->timer, jiffies + msecs_to_jiffies(dev->timeperiod));
        break;

        default:
        break;
    }


    return ret;
}

static int timer_release(struct inode *inode, struct file *file)
{

    return 0;
}

/* 字符操作集 */
static struct file_operations timer_fops = {
    .owner          = THIS_MODULE,
    .open           = timer_open,
    .release        = timer_release,
    .unlocked_ioctl = timer_ioctl,
    .read           = timer_read,
};


/* 定时器处理函数 */
void timer_function(unsigned long arg)
{
    struct timer_dev *dev = (struct timer_dev *)arg;
    static int sta = 1;
    sta = !sta;
    gpio_set_value(dev->gpio_led, sta);

    /* 重新定时 */
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(dev->timeperiod));
}


/* 初始化LED灯 */
int led_init(struct timer_dev *dev)
{
    int ret = 0;

    dev->nd = of_find_node_by_path("/gpioled");
    if(dev->nd == NULL)
    {
        ret = -ENAVAIL;
        goto fail_nd;
    }

    dev->gpio_led = of_get_named_gpio(dev->nd, "led-gpios", 0);
    if(dev->gpio_led < 0)
    {
        ret = -ENAVAIL;
        goto fail_gpio;
    }

    ret = gpio_request(dev->gpio_led, "led");
    if(ret)
    {
        ret = -EBUSY;
        printk(KERN_ERR "Failed to request GPIO\n");
        goto fail_request;
    }

    ret = gpio_direction_output(dev->gpio_led, 1);  /* 设置为输出 默认关灯 */
    if(ret < 0)
    {
        ret = -EBUSY;
        printk(KERN_ERR "Failed to set GPIO direction\n");
        goto fail_direction;
    }

    return 0;

fail_direction:
    gpio_free(dev->gpio_led);

fail_request:

fail_gpio:

fail_nd:
    return ret;
}


/* 驱动入口函数 */
static int __init timer_init(void)
{
    /* 注册字符驱动设备 */
    int ret = 0;
    timerdev.major = 0;


    if(timerdev.major)   /*给定设备号*/
    {
        timerdev.devid = MKDEV(timerdev.major, 0);
        ret = register_chrdev_region(timerdev.devid, TIMER_CNT, TIMER_NAME);
    }
    else        /*没给定设备号*/
    {
        ret = alloc_chrdev_region(&timerdev.devid, 0, TIMER_CNT, TIMER_NAME);
        timerdev.major = MAJOR(timerdev.devid);
        timerdev.minor = MINOR(timerdev.devid);
    }


    if(ret < 0)
    {
        printk(KERN_ERR "Faitimer to register device\n");
        goto fail_devid;
    }

    /* 初始化 cdev*/
    cdev_init(&timerdev.cdev, &timer_fops);

    /* 添加 cdev */
    ret = cdev_add(&timerdev.cdev, timerdev.devid, TIMER_CNT);
    if(ret < 0)
    {
        printk(KERN_ERR "Faitimer to add cdev\n");
        goto fail_cdev_add;
    }

    /* 创建类 class */
    timerdev.class = class_create(THIS_MODULE, TIMER_NAME);
    if(IS_ERR(timerdev.class))
    {
        printk(KERN_ERR "Faitimer to create class\n");
        ret = PTR_ERR(timerdev.class);
        goto fail_class_create;
    }

    /* 创建设备 */
    timerdev.device = device_create(timerdev.class, NULL, timerdev.devid, NULL, TIMER_NAME);
    if(IS_ERR(timerdev.device))
    {
        printk(KERN_ERR "Faitimer to create device\n");
        ret = PTR_ERR(timerdev.device);
        goto fail_device_create;
    }
    
    /* 初始化LED */
    ret = led_init(&timerdev);
    if(ret < 0)
    {
        printk(KERN_ERR "Failed to init led\n");
        goto fail_led_init;
    }

    /* 初始化定时器 */
    init_timer(&timerdev.timer);

    timerdev.timeperiod = 500;
    timerdev.timer.function = timer_function;
    timerdev.timer.expires = jiffies + msecs_to_jiffies(timerdev.timeperiod);
    timerdev.timer.data = (unsigned long)&timerdev;
    add_timer(&timerdev.timer);


    return 0;

fail_led_init:
fail_device_create:
    device_destroy(timerdev.class, timerdev.devid);       /* 删除设备 */

fail_class_create:
    class_destroy(timerdev.class);       /* 删除类 */

fail_cdev_add:
    cdev_del(&timerdev.cdev);        /* 删除 cdev */
    
fail_devid:
    unregister_chrdev_region(timerdev.devid, TIMER_CNT);   /* 注销设备号 */
    return ret;

}

static void __exit timer_exit(void)
{
    /* 关灯 */
    gpio_set_value(timerdev.gpio_led, 1);
    /* 释放GPIO */
    gpio_free(timerdev.gpio_led);

    /* 删除定时器 */
    del_timer(&timerdev.timer);

    /* 删除 cdev */
    cdev_del(&timerdev.cdev);
    unregister_chrdev_region(timerdev.devid, TIMER_CNT);

    device_destroy(timerdev.class, timerdev.devid);
    class_destroy(timerdev.class);

}


module_init(timer_init);
module_exit(timer_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("xyl");



