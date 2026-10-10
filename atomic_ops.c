// Quick exercise

/*Go back to your very first module (racy_spinlock.c / the counter exercise) — 
the one where shared_counter was protected by spin_lock_bh().
Could that specific case have used atomic_t instead of a spinlock entirely, eliminating the lock altogether?
Walk through whether it fits the "single variable, single read-modify-write operation" criterion, 
and if yes, sketch how timer_callback and proc_read would change. 
If you think it doesn't fully qualify, explain why not — there's a subtlety in that original code worth checking carefully 
(hint: look again at what proc_read actually does with the counter, not just shared_counter++ in the timer).*/

//problem: 
// timer_callback (softirq context)
spin_lock_bh(&my_lock);
shared_counter++;
spin_unlock_bh(&my_lock);

// proc_read (process context)
spin_lock_bh(&my_lock);
n = scnprintf(msg, sizeof(msg), "counter = %d\n", shared_counter);
mdelay(50);
spin_unlock_bh(&my_lock);

//solution:
static atomic_t shared_counter = ATOMIC_INIT(0);

static void timer_callback(struct timer_list *t)
{
    atomic_inc(&shared_counter);
    mod_timer(&my_timer, jiffies + HZ/10);
}

static ssize_t proc_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    char msg[64];
    int n = scnprintf(msg, sizeof(msg), "counter = %d\n", atomic_read(&shared_counter));
    return simple_read_from_buffer(buf, len, off, msg, n);
}