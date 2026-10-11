//completion_demo.c
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/completion.h>
#include <linux/uaccess.h>

static struct completion done;
static struct timer_list my_timer;


/* Simulates a hardirq handler by running in timer (softirq) context */
static void timer_callback(struct timer_list *t)
{
   complete(&done);
}

static ssize_t proc_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    char msg[64];
    int n;

    if (*off > 0)
        return 0;
    
    mod_timer(&my_timer, jiffies + 2*HZ); //timer fire for 2 sec
    if(wait_for_completion_timeout(&done, 3*HZ))
        return -ETIMEDOUT;
    n = scnprintf(msg, sizeof(msg), "event fired after wait\n");    

    return simple_read_from_buffer(buf, len, off, msg, n);
}

static const struct proc_ops pops = { .proc_read = proc_read };

static int __init compl_init(void)
{
    init_completion(&done); 
    proc_create("completion_demo", 0444, NULL, &pops);
    timer_setup(&my_timer, timer_callback, 0);
    
    pr_info("completion demo loaded\n");
    return 0;
}

static void __exit compl_exit(void)
{
    remove_proc_entry("completion_demo", NULL);
    del_timer_sync(&my_timer);
  
    pr_info("completion demo unloaded\n");
}

module_init(compl_init);
module_exit(compl_exit);
MODULE_LICENSE("GPL");