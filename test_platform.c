#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/version.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/fs.h> 
#include <linux/ioctl.h>  
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/types.h>


/*************************************
 * 
 * This is my first cdev demo
 * 
 *  struct class *class_create(struct module *owner, char *name);
 *  class_destroy(struct class *);
 *  
 *  创建设备
 *  
 *  struct device *device_create(struct class *, struct device *parent, dev_t, 
 *    void *drvdata, const char *fmt, ...);
 * 
 *************************************/

#define GLOBALMEM_SIZE 0x1000
#define GLOBALMEM_MAJOR 0

#define GLOBAL_MAGIC 'c'
#define IOCINIT       _IO(GLOBAL_MAGIC, 0)
#define IOC_MEM_CLR   _IOW(GLOBAL_MAGIC, 1, int)
#define IOC_TEST_READ _IOR(GLOBAL_MAGIC, 2, int)

static int globalmem_major = GLOBALMEM_MAJOR;
module_param(globalmem_major, int, S_IRUGO);


struct globalmem_dev {
    unsigned char mem[GLOBALMEM_SIZE];
    struct cdev cdev;
    dev_t devid;
    struct class *class;
    struct device *device;
    int major;
    int minor;
};

static struct globalmem_dev *pdev; 


static int globalmem_open(struct inode *inode, struct file *filp)
{

    filp->private_data = pdev;
    return 0;
}

static int globalmem_release(struct inode *inode, struct file *filp)
{
    return 0;
}
/**
 * 读函数
 **/

static ssize_t globalmem_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
    unsigned int pos = *ppos;
    unsigned int count = size;
    int ret  = 0;
    struct globalmem_dev *p = filp->private_data;
    if(pos >= GLOBALMEM_SIZE) {
        pr_info("The space is full!\n");
        return 0;
    }

    if(count >= GLOBALMEM_SIZE - pos) 
        count = GLOBALMEM_SIZE - pos;
    
    if(copy_from_user(p->mem + pos, buf, count)) {
        ret = -EFAULT;
    } else {
        *ppos += count;
        ret = count;
        printk(KERN_INFO "write %u bytes from %u\n", count, pos);
    }

    return ret;
}
static long int globalmem_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct globalmem_dev *p = filp->private_data;
    switch(cmd) {
        case IOC_MEM_CLR:
            memset(p->mem, 0, GLOBALMEM_SIZE);
        break;
        case IOC_TEST_READ:
            pr_err("test");
        break;
        default:
            return -2;
    }
    return 0;
}

static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig)
{
    pr_err("offset = %ld, orig = %d\n", offset, orig);
    loff_t ret = 0;
    switch(orig) {

        case 0:
            if(offset > GLOBALMEM_SIZE)
                ret = -EINVAL;
            filp->f_pos = offset;
            ret = filp->f_pos;
        break;

        case 1:
            if(filp->f_pos + offset > GLOBALMEM_SIZE) {
                ret = -EINVAL;
                break;
            }

            filp->f_pos += offset;

            ret = filp->f_pos; 


        break;

        case 2:
            ret = -EINVAL;
        break;   

        default:
        
        return ret= -EINVAL;
    }
    return ret;
}

static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    unsigned int pos = *ppos;
    unsigned int count = size;
    int ret  = 0;
    struct globalmem_dev *p = filp->private_data;
    if(pos >= GLOBALMEM_SIZE) {
        pr_info("The space is full!\n");
        return 0;
    }

    if(count >= GLOBALMEM_SIZE - pos) 
        count = GLOBALMEM_SIZE - pos;
    
    if(copy_to_user(buf, p->mem + pos, count)) {
        ret = -EFAULT;
    } else {
        *ppos += count;
        ret = count;
        printk(KERN_INFO "read %u bytes from %u\n", count, pos);
    }

    return ret;
}

static struct file_operations globalmem_fops = {
    .owner = THIS_MODULE,
    .read = globalmem_read,
    .write = globalmem_write,
    .release = globalmem_release, 
    .open = globalmem_open,
    .unlocked_ioctl = globalmem_ioctl, 
    .llseek = globalmem_llseek,
};


static int globalmem_setup(struct globalmem_dev *dev, int index)
{
    int err;
    if(dev->major) {
        dev->devid = MKDEV(dev->major, index);
        register_chrdev_region(dev->devid, 1, "snowberg");
    } else {
        alloc_chrdev_region(&dev->devid, 0, 1, "snowberg");
        dev->major = MAJOR(dev->devid);
        dev->minor = MINOR(dev->devid);
    }
    
    dev->cdev.owner = THIS_MODULE;
    cdev_init(&dev->cdev, &globalmem_fops);
    err = cdev_add(&dev->cdev, dev->devid, 1);
    if(err) { 
        pr_err("Error %d adding globalmem%d", err, index);
        goto cdevadd_fail;
    }
    dev->class = class_create(THIS_MODULE, "snowbergclass");
    if(IS_ERR(dev->class)) {
        err = -2;
        goto class_fail;
    }
    dev->device = device_create(dev->class, NULL, dev->devid, NULL, "snowdev");
    if(IS_ERR(dev->device)) {
        err = -3;
        goto device_fail;
    }
    return 0;
device_fail:
    class_destroy(dev->class);
class_fail:
    cdev_del(&dev->cdev);
cdevadd_fail:
    return err;
}


static int globalmem_remove(struct platform_device *dev)
{
    int ret;


    return ret;
}

static int globalmem_probe(struct platform_device *dev)
{
    int ret; 
    return ret;
}

static const struct of_device_id globalmem_of_match[] = {
    {
        .compatible = "test-platform",
    },
    {

    },
};
MODULE_DEVICE_TABLE(of, globalmem_of_match);

static struct platform_driver globalmem_driver = {
    .probe = globalmem_probe,
    .remove = globalmem_remove,
    .driver = {
        .name = "test-flatform",
        .of_match_table = globalmem_of_match,
    },
};

static int __init globalmem_init(void)
{
    pr_err("%s %d\n", __func__, __LINE__);
    int ret;
    dev_t devid;
    if(globalmem_major) {
        devid = MKDEV(globalmem_major, 0);
        register_chrdev_region(devid, 1, "snowberg");
    } else {
        alloc_chrdev_region(&devid, 0, 1, "snowberg");
        globalmem_major = MAJOR(devid);
    }

    pdev = kzalloc(sizeof(struct globalmem_dev), GFP_KERNEL);
    if(!pdev) {
        ret = -ENOMEM;
        goto fail_malloc;
    }

    pdev->devid = devid;
    pdev->major = globalmem_major;
    pdev->minor = MINOR(pdev->devid);

    ret = globalmem_setup(pdev, 0);

    if(ret) {
        pr_err("setup failed\n");
        goto setupfail; 
    }

    return platform_driver_register(&globalmem_driver);

setupfail:
    kfree(pdev);
fail_malloc:
    unregister_chrdev_region(devid, 1);
    return ret;
}

static void __exit globalmem_exit(void)
{
    cdev_del(&pdev->cdev);
    unregister_chrdev_region(pdev->devid, 1);
    device_destroy(pdev->class, pdev->devid);
    class_destroy(pdev->class);
    kfree(pdev);
    platform_driver_unregister(&globalmem_driver);
}

module_init(globalmem_init);
module_exit(globalmem_exit);
MODULE_LICENSE("GPL");