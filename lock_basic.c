#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

struct my_device{
    u32 count;
    spinlock_t lock;
};
static struct my_device *dev[3];


static ssize_t proc_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{   
    struct my_device *d = pde_data(file_inode(f));
    char msg[64];
    int n;

    spin_lock(&d->lock);
    n = scnprintf(msg, sizeof(msg), "counter = %u\n", d->count);
    spin_unlock(&d->lock);

    return simple_read_from_buffer(buf, len, off, msg, n);
}

static ssize_t proc_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
    struct my_device *d = pde_data(file_inode(f));
    char *kbuf;

    kbuf = kmalloc(64, GFP_KERNEL);   
    if (!kbuf) {        
        return -ENOMEM;
    }

    if (len > 63) len = 63;
    if (copy_from_user(kbuf, buf, len)) {   
        kfree(kbuf);
     
        return -EFAULT;
    }
    kbuf[len] = '\0';
    
    spin_lock(&d->lock);
    d->count++;
    spin_unlock(&d->lock);

    kfree(kbuf);
    
    return len;
}

static const struct proc_ops pops = { .proc_read = proc_read, 
                                      .proc_write = proc_write  };

static int my_init(void){

    for(int i = 0; i < 3; i++){
        dev[i] = kmalloc(sizeof(struct my_device), GFP_KERNEL);
        if(!dev[i]){
            return -ENOMEM;
        }
        dev[i]->count = 0;
        spin_lock_init(&dev[i]->lock);
    }
    proc_create_data("counter0", 0666, NULL, &pops, dev[0]);
    proc_create_data("counter1", 0666, NULL, &pops, dev[1]);
    proc_create_data("counter2", 0666, NULL, &pops, dev[2]);
    pr_info("Module loaded \n");

    return 0;
}

static void my_exit(void){

    remove_proc_entry("counter0", NULL);
    remove_proc_entry("counter1", NULL);
    remove_proc_entry("counter2", NULL);
    kfree(dev[0]);
    kfree(dev[1]);
    kfree(dev[2]);
    pr_info("Module unloaded\n");
}


module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Tanuk Kumar");