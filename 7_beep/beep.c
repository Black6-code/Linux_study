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

#define BEEP_CNT     1
#define BEEP_NAME    "beep"
#define BEEP_ON      1
#define BEEP_OFF     0


/* gpio 设备结构体 */
struct beep_dev{
    dev_t               devid;
    int                 major;
    int                 minor;
    struct cdev         cdev;
    struct class        *class;
    struct device       *device;
    struct device_node  *nd;
    int                 beep_gpio;
};

struct beep_dev beep;     /* LED */

static int beep_open(struct inode *inode, struct file *file)
{
    file->private_data = &beep;
    return 0;
}

static ssize_t beep_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;
    unsigned char databuf[1];

    struct beep_dev *dev = file->private_data;

    ret = copy_from_user(databuf, buf, count);

    if(ret < 0)
    {
        printk(KERN_ERR "Faibeep to copy from user\n");
        return -EFAULT;
    }

    if(databuf[0] == BEEP_ON)
    {
        gpio_set_value(dev->beep_gpio, 0);
    }
    else if(databuf[0] == BEEP_OFF)
    {
        gpio_set_value(dev->beep_gpio, 1);
    }

    return 0;
}

static int beep_release(struct inode *inode, struct file *file)
{

    return 0;
}

/* 字符操作集 */
static struct file_operations beep_fops = {
    .owner      = THIS_MODULE,
    .open       = beep_open,
    .release    = beep_release,
    .write      = beep_write,
};


static int __init beep_init(void)
{
    /* 注册字符驱动设备 */
    int ret = 0;

    beep.major = 0;
    if(beep.major)   /*给定设备号*/
    {
        beep.devid = MKDEV(beep.major, 0);
        ret = register_chrdev_region(beep.devid, BEEP_CNT, BEEP_NAME);
    }
    else        /*没给定设备号*/
    {
        ret = alloc_chrdev_region(&beep.devid, 0, BEEP_CNT, BEEP_NAME);
        beep.major = MAJOR(beep.devid);
        beep.minor = MINOR(beep.devid);
    }


    if(ret < 0)
    {
        printk(KERN_ERR "Faibeep to register device\n");
        goto fail_devid;
    }

    /* 初始化 cdev*/
    cdev_init(&beep.cdev, &beep_fops);

    /* 添加 cdev */
    ret = cdev_add(&beep.cdev, beep.devid, BEEP_CNT);
    if(ret < 0)
    {
        printk(KERN_ERR "Faibeep to add cdev\n");
        goto fail_cdev_add;
    }

    /* 创建类 class */
    beep.class = class_create(THIS_MODULE, BEEP_NAME);
    if(IS_ERR(beep.class))
    {
        printk(KERN_ERR "Faibeep to create class\n");
        ret = PTR_ERR(beep.class);
        goto fail_class_create;
    }

    /* 创建设备 */
    beep.device = device_create(beep.class, NULL, beep.devid, NULL, BEEP_NAME);
    if(IS_ERR(beep.device))
    {
        printk(KERN_ERR "Faibeep to create device\n");
        ret = PTR_ERR(beep.device);
        goto fail_device_create;
    }
    

// ========================================== 初始化beep ==================================================
    /* 获取设备节点 */
    beep.nd = of_find_node_by_path("/beep");
    if(beep.nd == NULL)
    {
        printk(KERN_ERR "Faibeep to find node\n");
        ret = -ENODEV;
        goto fail_node;
    }

    /* 获取gpio的属性 */
    beep.beep_gpio = of_get_named_gpio(beep.nd, "beep-gpios", 0);
    if(beep.beep_gpio < 0)
    {
        printk(KERN_ERR "Faibeep to get beep-gpios\n");
        ret = beep.beep_gpio;
        goto fail_beep_gpio;
    }

    printk("beep_gpio num = %d \r\n", beep.beep_gpio);

    /* 申请gpio */
    ret = gpio_request(beep.beep_gpio, "beep-gpios");
    if(ret < 0)
    {
        printk(KERN_ERR "Faibeep to request gpio\n");
        goto fail_beep_gpio;
        ret = -ENODEV;
    }

    /* 使用IO */
    ret = gpio_direction_output(beep.beep_gpio, 0);
    if(ret < 0)
    {
        printk(KERN_ERR "Faibeep to set gpio direction\n");
        goto fail_beep_gpio;
    }

    gpio_set_value(beep.beep_gpio, 0);

    printk(KERN_INFO "beep_init successful!!!\r\n");
    return 0;



fail_beep_gpio:
    gpio_free(beep.beep_gpio);

fail_node:
    of_node_put(beep.nd);

fail_device_create:
    device_destroy(beep.class, beep.devid);       /* 删除设备 */

fail_class_create:
    class_destroy(beep.class);       /* 删除类 */

fail_cdev_add:
    cdev_del(&beep.cdev);        /* 删除 cdev */
    
fail_devid:
    unregister_chrdev_region(beep.devid, BEEP_CNT);   /* 注销设备号 */
    return ret;

}

static void __exit beep_exit(void)
{
    gpio_set_value(beep.beep_gpio, 1);


    /* 删除 cdev */
    cdev_del(&beep.cdev);
    unregister_chrdev_region(beep.devid, BEEP_CNT);

    device_destroy(beep.class, beep.devid);
    class_destroy(beep.class);

    /* 释放gpio */
    gpio_free(beep.beep_gpio);
}


module_init(beep_init);
module_exit(beep_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("xyl");



