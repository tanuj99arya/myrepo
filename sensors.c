#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/rwsem.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#define NUM_SENSORS 5

struct sensor_data {
    int readings[NUM_SENSORS];
    struct rw_semaphore rwsem;
};

static struct sensor_data *sdata;
static struct task_struct *writer_thread;

static int writer_fn(void *arg)
{
    int next_slot = 0;

    while (!kthread_should_stop()) {
        down_write(&sdata->rwsem);
        sdata->readings[next_slot] = get_random_u32() % 100;
        up_write(&sdata->rwsem);

        next_slot = (next_slot + 1) % NUM_SENSORS;
        msleep(1000);
    }
    return 0;
}

static ssize_t proc_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    char msg[128];
    int n, i, pos = 0;

    down_read(&sdata->rwsem);
    for (i = 0; i < NUM_SENSORS; i++)
        pos += scnprintf(msg + pos, sizeof(msg) - pos, "sensor%d = %d\n",
                          i, sdata->readings[i]);
    up_read(&sdata->rwsem);

    n = pos;
    return simple_read_from_buffer(buf, len, off, msg, n);
}

static const struct proc_ops pops = { .proc_read = proc_read };

static int __init sensors_init(void)
{
    sdata = kmalloc(sizeof(*sdata), GFP_KERNEL);
    if (!sdata)
        return -ENOMEM;

    memset(sdata->readings, 0, sizeof(sdata->readings));
    init_rwsem(&sdata->rwsem);

    proc_create("sensors", 0444, NULL, &pops);

    writer_thread = kthread_run(writer_fn, NULL, "sensor_writer");
    if (IS_ERR(writer_thread)) {
        remove_proc_entry("sensors", NULL);
        kfree(sdata);
        return PTR_ERR(writer_thread);
    }

    pr_info("sensors module loaded\n");
    return 0;
}

static void __exit sensors_exit(void)
{
    kthread_stop(writer_thread);
    remove_proc_entry("sensors", NULL);
    kfree(sdata);
    pr_info("sensors module unloaded\n");
}

module_init(sensors_init);
module_exit(sensors_exit);
MODULE_LICENSE("GPL");