// tophalf.c
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/preempt.h>

#define ST_PLUG    BIT(0)
#define ST_IRQ_HPD BIT(1)

struct fake_dev {
	struct hrtimer     timer;       /* the "device": raises an event every 100 ms */
	struct work_struct work;        /* the bottom half */
	spinlock_t         lock;        /* protects pending[] */
	u32                hw_status;   /* simulated device status register */
	u32                pending[2];  /* top half -> bottom half mailbox */
	u32                n;
	/* verification only */
	u32                raised[2];
	u32                handled[2];
	u32                coalesced;
};

static struct fake_dev gdev;

static void top_half(struct fake_dev *d)
{
	u32 st;

	pr_info_once("top half:    hardirq=%d softirq=%d task=%d current=%s/%d\n",
		     !!in_hardirq(), !!in_softirq(), !!in_task(),
		     current->comm, current->pid);

	st = d->hw_status;              /* read device status */
	d->hw_status = 0;               /* device-level ack */

	spin_lock(&d->lock);
	d->pending[0] += !!(st & ST_PLUG);
	d->pending[1] += !!(st & ST_IRQ_HPD);
	spin_unlock(&d->lock);

	if (!schedule_work(&d->work))
		d->coalesced++;
}

static void bottom_half(struct work_struct *w)
{
	struct fake_dev *d = container_of(w, struct fake_dev, work);
	unsigned long flags;
	u32 p[2];

	pr_info_once("bottom half: hardirq=%d softirq=%d task=%d current=%s/%d\n",
		     !!in_hardirq(), !!in_softirq(), !!in_task(),
		     current->comm, current->pid);

	spin_lock_irqsave(&d->lock, flags);
	p[0] = d->pending[0];
	p[1] = d->pending[1];
	d->pending[0] = d->pending[1] = 0;
	spin_unlock_irqrestore(&d->lock, flags);

	d->handled[0] += p[0];
	d->handled[1] += p[1];

	msleep(250);    /* stand-in for AUX reads, link training, ... */
}

static enum hrtimer_restart fake_device_fn(struct hrtimer *t)
{
	struct fake_dev *d = container_of(t, struct fake_dev, timer);
	int bit = (d->n++ % 3) ? 1 : 0;     /* 1 plug event per 3 */

	d->raised[bit]++;
	d->hw_status |= BIT(bit);           /* device latches the event ... */
	top_half(d);                        /* ... and the CPU takes the interrupt */

	hrtimer_forward_now(t, ms_to_ktime(100));
	return HRTIMER_RESTART;
}

static int __init tophalf_init(void)
{
	spin_lock_init(&gdev.lock);
	INIT_WORK(&gdev.work, bottom_half);
	hrtimer_init(&gdev.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gdev.timer.function = fake_device_fn;
	hrtimer_start(&gdev.timer, ms_to_ktime(100), HRTIMER_MODE_REL);
	return 0;
}

static void __exit tophalf_exit(void)
{
	hrtimer_cancel(&gdev.timer);        /* stop new events first ... */
	cancel_work_sync(&gdev.work);       /* ... then the bottom half */
	pr_info("tophalf: PLUG raised=%u handled=%u left=%u | IRQ_HPD raised=%u handled=%u left=%u | coalesced=%u\n",
		gdev.raised[0], gdev.handled[0], gdev.pending[0],
		gdev.raised[1], gdev.handled[1], gdev.pending[1],
		gdev.coalesced);
}

module_init(tophalf_init);
module_exit(tophalf_exit);
MODULE_LICENSE("GPL");