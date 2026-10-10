#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

struct config {
    int value;
};

static struct config __rcu *cfg;   /* __rcu annotation: documents + enables sparse checking */
static struct task_struct *writer_thread;

static int writer_fn(void *arg)
{
    int counter = 0;

    while (!kthread_should_stop()) {
        struct config *old_cfg, *new_cfg;

        new_cfg = kmalloc(sizeof(*new_cfg), GFP_KERNEL);
        if (!new_cfg) {
            msleep(1000);
            continue;
        }
        new_cfg->value = counter++;

        old_cfg = rcu_dereference_protected(cfg, true); /* writer-side access, see note below */
        rcu_assign_pointer(cfg, new_cfg);

        synchronize_rcu();   /* block until all pre-existing readers are done with old_cfg */
        kfree(old_cfg);       /* now safe */

        msleep(1000);
    }
    return 0;
}

static ssize_t proc_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    char msg[64];
    int n, val;

    rcu_read_lock();
    val = rcu_dereference(cfg)->value;
    rcu_read_unlock();

    n = scnprintf(msg, sizeof(msg), "value = %d\n", val);
    return simple_read_from_buffer(buf, len, off, msg, n);
}

static const struct proc_ops pops = { .proc_read = proc_read };

static int __init rcu_demo_init(void)
{
    struct config *initial = kmalloc(sizeof(*initial), GFP_KERNEL);
    if (!initial)
        return -ENOMEM;
    initial->value = 0;
    RCU_INIT_POINTER(cfg, initial);  /* first-time init, no concurrent readers yet, so no barrier needed */

    proc_create("rcu_demo", 0444, NULL, &pops);

    writer_thread = kthread_run(writer_fn, NULL, "rcu_writer");
    if (IS_ERR(writer_thread)) {
        remove_proc_entry("rcu_demo", NULL);
        kfree(initial);
        return PTR_ERR(writer_thread);
    }

    pr_info("rcu_demo loaded\n");
    return 0;
}

static void __exit rcu_demo_exit(void)
{
    struct config *last;

    kthread_stop(writer_thread);
    remove_proc_entry("rcu_demo", NULL);

    last = rcu_dereference_protected(cfg, true);
    kfree(last);

    pr_info("rcu_demo unloaded\n");
}

module_init(rcu_demo_init);
module_exit(rcu_demo_exit);
MODULE_LICENSE("GPL");