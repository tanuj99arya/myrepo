// irq_intro.c
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/ktime.h>
#include <linux/math64.h>

static int mode;                  /* 0 = polling, 1 = event-driven */
module_param(mode, int, 0444);
static unsigned int poll_us = 1000;
module_param(poll_us, uint, 0444);

static struct timer_list dev_timer;
static struct task_struct *consumer;
static DECLARE_COMPLETION(ev_done);

static int data_ready;            /* fake device status register */
static u64 t_event_ns;            /* when the "device" raised the event */
static u64 polls, events, total_lat_ns;

static void dev_timer_fn(struct timer_list *t)
{
	WRITE_ONCE(t_event_ns, ktime_get_ns());
	WRITE_ONCE(data_ready, 1);
	if (mode == 1)
		complete(&ev_done);
	mod_timer(&dev_timer, jiffies + msecs_to_jiffies(200));
}

static void handle_event(void)
{
	total_lat_ns += ktime_get_ns() - READ_ONCE(t_event_ns);
	events++;
	WRITE_ONCE(data_ready, 0);
}

static int consumer_fn(void *arg)
{
	while (!kthread_should_stop()) {
		if (mode == 0) {
			polls++;
			if (READ_ONCE(data_ready))
				handle_event();
			usleep_range(poll_us, poll_us + 50);
		} else {
			if (wait_for_completion_timeout(&ev_done, HZ / 2))
				handle_event();
		}
	}
	return 0;
}

static int __init intro_init(void)
{
	timer_setup(&dev_timer, dev_timer_fn, 0);
	consumer = kthread_run(consumer_fn, NULL, "irq_intro");
	if (IS_ERR(consumer))
		return PTR_ERR(consumer);
	mod_timer(&dev_timer, jiffies + msecs_to_jiffies(200));
	return 0;
}

static void __exit intro_exit(void)
{
	del_timer_sync(&dev_timer);   /* timer_delete_sync() on kernels >= 6.15 */
	kthread_stop(consumer);
	pr_info("irq_intro: mode=%d poll_us=%u polls=%llu events=%llu avg_latency=%llu us\n",
		mode, poll_us, polls, events,
		events ? div64_u64(total_lat_ns, events * 1000) : 0);
}

module_init(intro_init);
module_exit(intro_exit);
MODULE_LICENSE("GPL");