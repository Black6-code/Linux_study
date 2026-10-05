#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define CHRDEVBASE_MAJOR       200     //主设备号
#define CHRDEVBASE_NAME        "chrdevbase"

static char readbuf[100];
static char writebuf[100];
static char kerneldata[] = {"kernel data!"};


static int chrdevbase_open(struct inode *inode, struct file *filp)
{
    //printk("chrdevbase_open\r\n");
    return 0;
}

static int chrdevbase_release(struct inode *inode, struct file *filp)
{
    //printk("chrdevbase_release\r\n");
    return 0;
}

static ssize_t chrdevbase_read(struct file *filp, __user char *buf, size_t count,
			loff_t *ppos)
{
    //printk("chrdevbase_read\r\n");

    int ret = 0;
    memcpy(readbuf, kerneldata, sizeof(kerneldata));
    ret = copy_to_user(buf, readbuf, count);
    if(ret == 0)
    {

    }
    else
    {

    }

    return ret;
}

static ssize_t chrdevbase_write(struct file *filp, const char __user *buf,
			 size_t count, loff_t *ppos)
{
    //printk("chrdevbase_write\r\n");
    int ret = 0;
    ret = copy_from_user(writebuf, buf, count);
    if(ret == 0)
    {
        printk("chrdevbase_write copy_from_user %s\r\n", writebuf);
    }
    else
    {

    }

    return 0;
}

static struct file_operations chrdevbase_fops =
{
    .owner = THIS_MODULE,
    .open = chrdevbase_open,
    .release = chrdevbase_release,
    .read = chrdevbase_read,
    .write = chrdevbase_write
};


static int __init chrdevbse_init(void)
{
    int ret = 0;
    printk(KERN_INFO "chrdevbse_init\r\n");
    /*注册字符设备*/
    ret = register_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME, &chrdevbase_fops);
    if(ret < 0)
    {
        printk("chrdevbase_init register_chrdev failed\r\n");
    }
    
    return 0;
}

static void __exit chrdevbse_exit(void)
{
    /*卸载字符设备*/
    unregister_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME);
    printk(KERN_INFO "chrdevbse_exit\r\n");
}


/*
*   模块入口
*/
module_init(chrdevbse_init)

/*
*   模块出口
*/
module_exit(chrdevbse_exit)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xyl");

