#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>   /* class_create / device_create */
#include <linux/err.h>      /* IS_ERR / PTR_ERR */

#define LED_MAJOR       200
#define LED_NAME        "led"

#define NEWCHRLED_NAME      "newchrld"
#define NEWCHRLED_COUNT     1

/* 寄存器物理地址 */
#define CCM_CCGR1_BASE              (0x020C406C)
#define SW_MUX_GPIO1_IO03_BASE      (0x020E0068)
#define SW_PAD_GPIO1_IO03_BASE      (0x020E02F4)
#define GPIO1_DR_BASE               (0x0209C000)
#define GPIO1_GDIR_BASE             (0x0209C004)

/* 地址映射后的虚拟地址指针 */
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;

#define LED_ON  0       /* 开灯 */
#define LED_OFF 1       /* 关灯 */

struct newchrled_dev
{
    struct cdev cdev;       /* cdev结构体 */
    dev_t   devid;          /*设备号*/
    struct class *class;    /*类*/
    struct device *device;  /* 设备 */
    int     major;          /*主设备号*/
    int     minor;          /*次设备号*/
};

struct newchrled_dev newchrled;     /* led设备 */

/* LED开灯 关灯实验 */
static void led_switch(unsigned char state)
{
    u32 val = 0;
    if(state == LED_ON)
    {
        val = readl(GPIO1_DR);
        val &= ~(1 << 3);
       	writel(val, GPIO1_DR);
    }
    else
    {
        val = readl(GPIO1_DR);
        val |= (1 << 3);
       	writel(val, GPIO1_DR);
    }

}

static int newchrled_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &newchrled;
    return 0;
}

static ssize_t newchrled_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{   
    int retvalue;
    unsigned char bufdata[1];

    retvalue = copy_from_user(bufdata, buf, count);
    if(retvalue < 0)
    {
        printk(KERN_ERR "newchrled: copy_from_user err\r\n");
        return -EFAULT;
    }

    led_switch(bufdata[0]);
    return 0;
}

static int newchrled_release(struct inode *inode, struct file *filp)
{
    /* 提取文件私有数据 */
    struct newchrled_dev *dev = (struct newchrled_dev *)filp->private_data;
    return 0;
}


static const struct file_operations newchrled_fops = 
{
    .owner      = THIS_MODULE,
    .open       = newchrled_open,
    .write      = newchrled_write,
    .release    = newchrled_release,
};


/* 入口函数 */
static int __init newchrled_init(void)
{
    int ret = 0;
    unsigned int val = 0;
    printk(KERN_INFO "newchrled: init\r\n");

    /* 初始化LED灯  地址映射 */
    IMX6U_CCM_CCGR1     = ioremap(CCM_CCGR1_BASE, 4);
    SW_MUX_GPIO1_IO03   = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
    SW_PAD_GPIO1_IO03   = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
    GPIO1_DR            = ioremap(GPIO1_DR_BASE, 4);
    GPIO1_GDIR          = ioremap(GPIO1_GDIR_BASE, 4);

    /* 初始化 */
    val = readl(IMX6U_CCM_CCGR1);
    val &= ~(3 << 26);  /* 清除之前的配置 */
    val |= (3 << 26);  /* 设置为GPIO模式 */
    writel(val, IMX6U_CCM_CCGR1);

    writel(0x5, SW_MUX_GPIO1_IO03);     /* 设置复用 */
    writel(0x10B0, SW_PAD_GPIO1_IO03);  /* 设置电气属性 */

    val = readl(GPIO1_GDIR);
    val |= (1 << 3);  /* 设置为输出模式 */
    writel(val, GPIO1_GDIR);

    val = readl(GPIO1_DR);
    val &= ~(1 << 3);  /* 清0 打开LED灯 */
    writel(val, GPIO1_DR);

    newchrled.major = 0;
    if(newchrled.major) /* 给定设备号 */
    {
        newchrled.devid = MKDEV(newchrled.major, 0);
        ret = register_chrdev_region(newchrled.devid, NEWCHRLED_COUNT, NEWCHRLED_NAME);
    }
    else    /* 没有给定设备号 进行申请 */
    {
        ret = alloc_chrdev_region(&newchrled.devid, 0, NEWCHRLED_COUNT, NEWCHRLED_NAME);
        newchrled.major = MAJOR(newchrled.devid);
        newchrled.minor = MINOR(newchrled.devid);
    }
    if(ret < 0)
    {
        printk(KERN_ERR "newchrled: register_chrdev_region err\r\n");
        goto fail_register_chrdev_region;
    }
    /* 成功打印设备号 */
    printk(KERN_INFO "newchrled: major = %d, minor = %d\r\n", newchrled.major, newchrled.minor);

    /* 注册字符设备 */
    newchrled.cdev.owner = THIS_MODULE;

    cdev_init(&newchrled.cdev, &newchrled_fops);
    
    ret = cdev_add(&newchrled.cdev, newchrled.devid, NEWCHRLED_COUNT);
    if(ret < 0)
    {
        goto fail_cdev_init;
    }

    /* 自动加载设备节点 */
    newchrled.class = class_create(THIS_MODULE, NEWCHRLED_NAME);
    if(IS_ERR(newchrled.class))
    {
       ret = PTR_ERR(newchrled.class);
        goto fail_class_create;
    }

    newchrled.device = device_create(newchrled.class, NULL, newchrled.devid, NULL, NEWCHRLED_NAME);
    if(IS_ERR(newchrled.device))
    {
        ret = PTR_ERR(newchrled.device);
        goto fail_device_create;
    }
    return 0;

fail_device_create:
    class_destroy(newchrled.class);
fail_class_create:
    cdev_del(&newchrled.cdev);
fail_cdev_init:
    unregister_chrdev_region(newchrled.devid, NEWCHRLED_COUNT);
fail_register_chrdev_region:
    return ret;

}


/* 出口函数 */
static void __exit newchrled_exit(void)
{
    unsigned int val = 0;
    val = readl(GPIO1_DR);
    val |= (1 << 3);  /* 设置1 关闭LED灯 */
    writel(val, GPIO1_DR);

    /* 地址取消映射 */
    iounmap(IMX6U_CCM_CCGR1);
    iounmap(SW_MUX_GPIO1_IO03);
    iounmap(SW_PAD_GPIO1_IO03);
    iounmap(GPIO1_DR);
    iounmap(GPIO1_GDIR);

    printk(KERN_INFO "newchrled: exit\r\n");
    /*删除字符设备*/
    cdev_del(&newchrled.cdev);

    /* 注销设备号 */
    unregister_chrdev_region(newchrled.devid, NEWCHRLED_COUNT);

    /* 销毁设备 需要在摧毁类之前进行 */
    device_destroy(newchrled.class, newchrled.devid);

    /* 销毁类 */
    class_destroy(newchrled.class);
}

/* 注册和卸载驱动 */
module_init(newchrled_init);
module_exit(newchrled_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("xyl");
