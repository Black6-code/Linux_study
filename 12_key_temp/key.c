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

#define KEY_CNT     1
#define KEY_NAME    "key"
#define KEY0VALUE   0xf0
#define INVAKEY     0


/* gpio 设备结构体 */
struct key_dev{
    dev_t               devid;
    int                 major;
    int                 minor;
    struct cdev         cdev;
    struct class        *class;
    struct device       *device;
    struct device_node  *nd;
    int                 key_gpio;
    atomic_t            key_value;
};

struct key_dev key;     /* LED */

static int key_open(struct inode *inode, struct file *file)
{
    file->private_data = &key;
    return 0;
}

static ssize_t key_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;

    return ret;
}

static ssize_t key_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    int ret = 0;
    int value = 0;

    struct key_dev* dev = file->private_data;

    if(gpio_get_value(dev->key_gpio) == 0)  /* 按下 */
    {
        while(gpio_get_value(dev->key_gpio) == 0); /* 一直处于按下的状态 */
        atomic_set(&dev->key_value, KEY0VALUE);
    }
    else
    {
        atomic_set(&dev->key_value, INVAKEY);
    }

    value = atomic_read(&dev->key_value);

    ret = copy_to_user(buf, &value, count);     /* 内核返回给用户的数据 */

    return ret;
}



static int key_release(struct inode *inode, struct file *file)
{

    return 0;
}

/* 字符操作集 */
static struct file_operations key_fops = {
    .owner      = THIS_MODULE,
    .open       = key_open,
    .release    = key_release,
    .write      = key_write,
    .read       = key_read,
};

/* key init 函数*/
static int keyio_init(struct key_dev *dev)
{
    int ret = 0;

    dev->nd = of_find_node_by_path("/key");

    if(dev->nd == NULL)
    {
        ret = -EINVAL;
        goto fail_nd;
    }

    dev->key_gpio = of_get_named_gpio(dev->nd, "key-gpios", 0);
    if(dev->key_gpio < 0)
    {
        ret = -EINVAL;
        goto fail_gpio;
    }

    ret = gpio_request(dev->key_gpio, "key0");
    if(ret)
    {
        ret = -EBUSY;
        printk("IO %d quest fail \r\n", dev->key_gpio);
        goto fail_request;
    }

    ret = gpio_direction_input(dev->key_gpio);
    if(ret < 0)
    {
        ret = -EINVAL;
        goto fail_input;
    }

    return 0;


fail_input:
fail_request:
    gpio_free(key.key_gpio);
fail_gpio:
    of_node_put(key.nd);
fail_nd:
    return ret;

}

/* 驱动入口函数 */
static int __init key_init(void)
{
    /* 注册字符驱动设备 */
    int ret = 0;
    key.major = 0;

    /* 初始化atomic */
    atomic_set(&key.key_value, INVAKEY);


    if(key.major)   /*给定设备号*/
    {
        key.devid = MKDEV(key.major, 0);
        ret = register_chrdev_region(key.devid, KEY_CNT, KEY_NAME);
    }
    else        /*没给定设备号*/
    {
        ret = alloc_chrdev_region(&key.devid, 0, KEY_CNT, KEY_NAME);
        key.major = MAJOR(key.devid);
        key.minor = MINOR(key.devid);
    }


    if(ret < 0)
    {
        printk(KERN_ERR "Faikey to register device\n");
        goto fail_devid;
    }

    /* 初始化 cdev*/
    cdev_init(&key.cdev, &key_fops);

    /* 添加 cdev */
    ret = cdev_add(&key.cdev, key.devid, KEY_CNT);
    if(ret < 0)
    {
        printk(KERN_ERR "Faikey to add cdev\n");
        goto fail_cdev_add;
    }

    /* 创建类 class */
    key.class = class_create(THIS_MODULE, KEY_NAME);
    if(IS_ERR(key.class))
    {
        printk(KERN_ERR "Faikey to create class\n");
        ret = PTR_ERR(key.class);
        goto fail_class_create;
    }

    /* 创建设备 */
    key.device = device_create(key.class, NULL, key.devid, NULL, KEY_NAME);
    if(IS_ERR(key.device))
    {
        printk(KERN_ERR "Faikey to create device\n");
        ret = PTR_ERR(key.device);
        goto fail_device_create;
    }
    
    ret = keyio_init(&key);
    if(ret < 0)
    {
        printk("key_io init fail \n");
        goto fail_device_create;
    }

    return 0;

fail_device_create:
    device_destroy(key.class, key.devid);       /* 删除设备 */

fail_class_create:
    class_destroy(key.class);       /* 删除类 */

fail_cdev_add:
    cdev_del(&key.cdev);        /* 删除 cdev */
    
fail_devid:
    unregister_chrdev_region(key.devid, KEY_CNT);   /* 注销设备号 */
    return ret;

}

static void __exit key_exit(void)
{
    

    /* 删除 cdev */
    cdev_del(&key.cdev);
    unregister_chrdev_region(key.devid, KEY_CNT);

    device_destroy(key.class, key.devid);
    class_destroy(key.class);

    /* 释放gpio */
    gpio_free(key.key_gpio);
}


module_init(key_init);
module_exit(key_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("xyl");



