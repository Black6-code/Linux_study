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

#define DTSLED_CNT      1           /*设备个数*/
#define DTSLED_NAME     "dtsled"    

#define LEDON      0
#define LEDOFF     1

/* 映射后的寄存器虚拟地址指针 */
static void __iomem *IMX6U_CCM_CCGR1;
static void __iomem *SW_MUX_GPIO1_IO03;
static void __iomem *SW_PAD_GPIO1_IO03;
static void __iomem *GPIO1_DR;
static void __iomem *GPIO1_GDIR;


/* 设备结构体 */
struct dtsled_dev{
    dev_t               devid;      /* 设备号 */
    struct cdev         cdev;       /* 字符设备 */
    struct class        *class;     /* 类 */
    struct device       *device;    /* 设备 */
    int                 major;      /* 主设备号 */
    int                 minor;      /* 次设备号 */
    struct device_node  *nd;        /* 设备节点 */
};

struct dtsled_dev dtsled;

void led_switch(u8 sta)
{
	u32 val = 0;
	if(sta == LEDON) 
    {
		val = readl(GPIO1_DR);
		val &= ~(1 << 3);	
		writel(val, GPIO1_DR);
	}else if(sta == LEDOFF) 
    {
		val = readl(GPIO1_DR);
		val|= (1 << 3);	
		writel(val, GPIO1_DR);
	}	
}

static int dtsled_open(struct inode *inode, struct file* filp)
{
    filp->private_data = &dtsled;
    return 0;
}

static ssize_t dtsled_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offt)
{
    //struct dteled_dev *dev =  filp->private_data;

    int retvalue;
	unsigned char databuf[1];
	unsigned char ledstat;

	retvalue = copy_from_user(databuf, buf, cnt);
	if(retvalue < 0) 
    {
		printk("kernel write failed!\r\n");
		return -EFAULT;
	}

	ledstat = databuf[0];		/* 获取状态值 */

	if(ledstat == LEDON) 
    {	
		led_switch(LEDON);		/* 打开LED灯 */
	} else if(ledstat == LEDOFF) 
    {
		led_switch(LEDOFF);	    /* 关闭LED灯 */
	}
	return 0;
    return 0;
}

static ssize_t dtsled_release(struct file *filp, char __user *buf, size_t cnt, loff_t *offt)
{
	return 0;
}

/* dtsled 字符设备操作集合 */
static const struct file_operations dtsled_fops = 
{
    .owner      = THIS_MODULE,
    .write      = dtsled_write,
    .open       = dtsled_open,
    .release    = dtsled_release, 
};


static int __init dtsled_init(void)
{
    int ret = 0;
    const char* str;
    u32 regdata[10];
    u8 i = 0;
    u32 val;

    /* 1 申请设备号 */
    dtsled.major = 0;
    if(dtsled.major)
    {
        dtsled.devid = MKDEV(dtsled.major, 0);
        ret = register_chrdev_region(dtsled.devid, DTSLED_CNT, DTSLED_NAME);
    }
    else
    {
        ret = alloc_chrdev_region(&dtsled.devid, 0, DTSLED_CNT, DTSLED_NAME);
        dtsled.major = MAJOR(dtsled.devid);
        dtsled.minor = MINOR(dtsled.devid);
    }
    if(ret < 0)
    {
        printk("fail_devid \r\n");
        goto fail_devid;
    }

    /* 2 添加字符设备 */
    dtsled.cdev.owner = THIS_MODULE;
    cdev_init(&dtsled.cdev, &dtsled_fops);
    ret = cdev_add(&dtsled.cdev, dtsled.devid, DTSLED_CNT);
    if(ret < 0)
    {
        printk("fail_cdev \r\n");
        goto fail_cdev;
    }

    /* 3 自动创建设备节点 */
    dtsled.class = class_create(THIS_MODULE, DTSLED_NAME);
    if(IS_ERR(dtsled.class))
    {
        ret = PTR_ERR(dtsled.class);
        goto fail_class;
    }

    dtsled.device = device_create(dtsled.class, NULL, dtsled.devid, NULL, DTSLED_NAME);
    if(IS_ERR(dtsled.device))
    {
        ret = PTR_ERR(dtsled.device);
        goto fail_device;
    }

    /* 获取设备树属性 */
    dtsled.nd = of_find_node_by_path("/alphaled");
    if(dtsled.nd == NULL)
    {
        ret = -EINVAL;
        goto fail_findnd;
    }
    ret = of_property_read_string(dtsled.nd, "status", &str);
    if(ret < 0)
    {
        goto fail_rs;
    }
    else
    {
        printk("status = %s \r\n", str);
    }

    ret = of_property_read_string(dtsled.nd, "compatible", &str);
    if(ret < 0)
    {
        goto fail_rs;
    }
    else
    {
        printk("status = %s \r\n", str);
    }

    ret = of_property_read_u32_array(dtsled.nd, "reg", regdata, 10);
    if(ret < 0)
    {
        goto fail_rs;
    }
    else
    {
        for(i = 0; i < 10; i++)
        {
            printk("%x ", regdata[i]);
        }
        printk("\r\n");
    }

    /* LED 灯初始化 */
	IMX6U_CCM_CCGR1 = ioremap(regdata[0], regdata[1]);
	SW_MUX_GPIO1_IO03 = ioremap(regdata[2], regdata[3]);
  	SW_PAD_GPIO1_IO03 = ioremap(regdata[4], regdata[5]);
	GPIO1_DR = ioremap(regdata[6], regdata[7]);
	GPIO1_GDIR = ioremap(regdata[8], regdata[9]);

    /* 初始化IO */
    val = readl(IMX6U_CCM_CCGR1);
	val &= ~(3 << 26);	/* 清楚以前的设置 */
	val |= (3 << 26);	/* 设置新值 */

	writel(val, IMX6U_CCM_CCGR1);
	writel(0x5, SW_MUX_GPIO1_IO03);
	writel(0x10B0, SW_PAD_GPIO1_IO03);

	val = readl(GPIO1_GDIR);
	val &= ~(1 << 3);	/* 清除以前的设置 */
	val |= (1 << 3);	/* 设置为输出 */
	writel(val, GPIO1_GDIR);

	val = readl(GPIO1_DR);
	val |= (1 << 3);	
	writel(val, GPIO1_DR);

    return 0;


fail_rs:
fail_findnd:
    device_destroy(dtsled.class, dtsled.devid);
fail_device:
    class_destroy(dtsled.class);
fail_class:
    cdev_del(&dtsled.cdev);
fail_cdev:
    unregister_chrdev_region(dtsled.devid, DTSLED_CNT);
fail_devid:
    return ret;
}


static void __exit dtsled_exit(void)
{

    u32 val = 0;
    val = readl(GPIO1_DR);
    val |= (1 << 3);
    writel(val, GPIO1_DR);

    /* 取消地址映射 */
    iounmap(IMX6U_CCM_CCGR1);
	iounmap(SW_MUX_GPIO1_IO03);
	iounmap(SW_PAD_GPIO1_IO03);
	iounmap(GPIO1_DR);
	iounmap(GPIO1_GDIR);

    /* 删除字符设备 */
    cdev_del(&dtsled.cdev);
    /* 释放设备号 */
    unregister_chrdev_region(dtsled.devid, DTSLED_CNT);
    /* 摧毁设备 */
    device_destroy(dtsled.class, dtsled.devid);
    /* 摧毁类 */
    class_destroy(dtsled.class);

}


/* 模块出入口函数 */
module_init(dtsled_init);
module_exit(dtsled_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("xyl");

