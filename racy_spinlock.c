// mutex_sleep_ok.c
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

static DEFINE_MUTEX(my_mutex);   /* static init, no separate mutex_init() call needed */

static ssize_t proc_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
    char *kbuf;

    mutex_lock(&my_mutex);

    kbuf = kmalloc(64, GFP_KERNEL);      /* now legal: mutex doesn't disable preemption */
    if (!kbuf) {
        mutex_unlock(&my_mutex);
        return -ENOMEM;
    }

    if (len > 63) len = 63;
    if (copy_from_user(kbuf, buf, len)) { /* also legal: may sleep on page fault */
        kfree(kbuf);
        mutex_unlock(&my_mutex);
        return -EFAULT;
    }
    kbuf[len] = '\0';
    pr_info("received: %s\n", kbuf);

    kfree(kbuf);
    mutex_unlock(&my_mutex);
    return len;
}

static const struct proc_ops pops = { .proc_write = proc_write };

static int __init mso_init(void)
{
    proc_create("mutex_sleep_ok", 0222, NULL, &pops);
    pr_info("mutex_sleep_ok loaded\n");
    return 0;
}

static void __exit mso_exit(void)
{
    remove_proc_entry("mutex_sleep_ok", NULL);
    pr_info("mutex_sleep_ok unloaded\n");
}

module_init(mso_init);
module_exit(mso_exit);
MODULE_LICENSE("GPL");