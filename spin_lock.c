// sleep_in_atomic.c
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

static spinlock_t my_lock;

static ssize_t proc_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
    char *kbuf;

    spin_lock(&my_lock);

    kbuf = kmalloc(64, GFP_KERNEL);   /* BUG: GFP_KERNEL can sleep, we're atomic here */
    if (!kbuf) {
        spin_unlock(&my_lock);
        return -ENOMEM;
    }

    if (len > 63) len = 63;
    if (copy_from_user(kbuf, buf, len)) {   /* also potentially sleeps (page fault) */
        kfree(kbuf);
        spin_unlock(&my_lock);
        return -EFAULT;
    }
    kbuf[len] = '\0';
    pr_info("received: %s\n", kbuf);

    kfree(kbuf);
    spin_unlock(&my_lock);
    return len;
}

static const struct proc_ops pops = { .proc_write = proc_write };

static int __init sia_init(void)
{
    spin_lock_init(&my_lock);
    proc_create("sleep_in_atomic", 0222, NULL, &pops);
    pr_info("sleep_in_atomic loaded\n");
    return 0;
}

static void __exit sia_exit(void)
{
    remove_proc_entry("sleep_in_atomic", NULL);
    pr_info("sleep_in_atomic unloaded\n");
}

module_init(sia_init);
module_exit(sia_exit);
MODULE_LICENSE("GPL");