#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#define LED_MAJOR       200
#define LED_NAME        "led"

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

/*led灯打开或者关闭*/
static void led_switch(u8 sta)
{
    u32 val = 0;
    if(sta == LED_ON)
    {
        val = readl(GPIO1_DR);
        val &= ~(1 << 3);       /* 清0 打开LED灯 */
        writel(val, GPIO1_DR);
    }
    else if(sta == LED_OFF)
    {
        val = readl(GPIO1_DR);
        val |= (1 << 3);       /* 设置1 关闭LED灯 */
        writel(val, GPIO1_DR);
    }
}

static int led_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int led_close(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t led_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
    int retvalue;

    unsigned char databuf[1];
    retvalue =copy_from_user(databuf, buf, count);
    if(retvalue < 0)
    {
        printk("kernel copy_from_user failed\r\n");
        return -EFAULT;
    }

    /*判断开灯还是关灯*/
    printk("databuf[0] = %d\r\n", databuf[0]);
    led_switch(databuf[0]);

    return 0;
}
            
/*字符设备操作集合*/
static const struct file_operations led_fops =
{
    .owner      = THIS_MODULE,
    .write      = led_write,
    .open       = led_open,
    .release    = led_close,
};

static int __init led_init(void)
{
    int ret = 0;

    unsigned int val = 0;
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


    /* 1 注册字符设备*/
    ret = register_chrdev(LED_MAJOR, LED_NAME, &led_fops);
    if(ret < 0)
    {
        printk("register chardev failed\r\n");
        return -EIO;
    }

    printk("led_init\r\n");
    return 0;
}

static void __exit led_exit(void)
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


    /* 注销字符设备 */
    unregister_chrdev(LED_MAJOR, LED_NAME);
    printk("led_exit\r\n");
}


/*注册驱动加载和卸载*/
module_init(led_init);
module_exit(led_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("xyl");
