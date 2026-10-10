// seq_demo.c
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/seqlock.h>
#include <linux/uaccess.h>

static struct {
    int a;
    int b;           /* invariant: b == a * 2 */
} data;

static struct task_struct *writer_thread;

static int writer_fn(void *arg)
{
    int i = 0;
    while (!kthread_should_stop()) {
        i++;
        data.a = i;
        udelay(50);          /* widen the window between the two writes */
        data.b = i * 2;
        usleep_range(100, 200);
    }
    return 0;
}

static ssize_t proc_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    char msg[64];
    int a, b, n;

    a = data.a;
    b = data.b;
    n = scnprintf(msg, sizeof(msg), "a=%d b=%d %s\n", a, b,
                  (b == a * 2) ? "OK" : "TORN");
    return simple_read_from_buffer(buf, len, off, msg, n);
}

static const struct proc_ops pops = { .proc_read = proc_read };

static int __init seq_init(void)
{
    proc_create("seq_demo", 0444, NULL, &pops);
    writer_thread = kthread_run(writer_fn, NULL, "seq_writer");
    if (IS_ERR(writer_thread)) {
        remove_proc_entry("seq_demo", NULL);
        return PTR_ERR(writer_thread);
    }
    return 0;
}

static void __exit seq_exit(void)
{
    kthread_stop(writer_thread);
    remove_proc_entry("seq_demo", NULL);
}

module_init(seq_init);
module_exit(seq_exit);
MODULE_LICENSE("GPL");