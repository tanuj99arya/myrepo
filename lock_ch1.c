
static struct my_device{
    u32 count;
    spinlock_t lock;
};

static ssize_t proc_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
    char msg[64];
    int n;
    
    spin_lock(&dev->lock);
    n = scnprintf(msg, sizeof(msg), "counter = %u\n", dev->count);
    spin_unlock(&dev->lock);

    return simple_read_from_buffer(buf, len, off, msg, n);
}

static ssize_t proc_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
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
    
    spin_lock(&dev->lock);
    dev->count++;
    spin_unlock(&dev->lock);

    kfree(kbuf);
    
    return len;
}

static const struct proc_ops pops = { .proc_read = proc_read, 
                                      .proc_write = proc_write  };
static struct my_device *dev;
static int my_init(void){
    
    dev = kmalloc(sizeof(struct my_device), GFP_KERNEL);
    if(!dev){
        return -ENOMEM;
    }

    dev->count = 0;
    spin_lock_init(&dev->lock);
    proc_create("counter", 0666, NULL, &pops);
    pr_info("Module loaded \n");

    return 0;
}

static void my_exit(void){

    remove_proc_entry("counter", NULL);
    kfree(dev);
    pr_info("Module unload \n");
}

