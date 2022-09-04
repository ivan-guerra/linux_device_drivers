/*
 * jit.c -- the just-in-time module
 *
 * Copyright (C) 2001,2003 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001,2003 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 * $Id: jit.c,v 1.16 2004/09/26 07:02:43 gregkh Exp $
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/time.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>

#include <asm/hardirq.h>

/*
 * This module is a silly one: it only embeds short code fragments
 * that show how time delays can be handled in the kernel.
 */

int delay = HZ; /* the default delay, expressed in jiffies */
int tdelay = 10;

module_param(delay, int, 0);
module_param(tdelay, int, 0);

MODULE_AUTHOR("Alessandro Rubini");
MODULE_LICENSE("Dual BSD/GPL");

/* use these as data pointers, to implement four files in one function */
enum jit_files {
    JIT_BUSY,
    JIT_SCHED,
    JIT_QUEUE,
    JIT_SCHEDTO
};

/*
 * The timer example follows
 */

/* This data structure used as "data" for the timer and tasklet functions */
struct jit_data {
    struct timer_list timer;
    struct tasklet_struct tlet;
    int hi; /* tasklet or tasklet_hi */
    wait_queue_head_t wait;
    unsigned long prevjiffies;
    struct seq_file *buf;
    int loops;
};

#define JIT_ASYNC_LOOPS 5

void jit_timer_fn(struct timer_list *t)
{
    struct jit_data *data = from_timer(data, t, timer);
    unsigned long j = jiffies;
    seq_printf(data->buf, "%9li  %3li     %i    %6i   %s\n",
            j, j - data->prevjiffies, irq_count() ? 1 : 0,
            current->pid, current->comm);

    if (--data->loops) {
        data->prevjiffies = j;
        mod_timer(&data->timer, data->timer.expires + tdelay);
    } else {
        wake_up_interruptible(&data->wait);
    }
}

/* the /proc function: allocate everything to allow concurrency */
static int jit_timer(struct seq_file *m, void *v)
{
    struct jit_data *data;
    unsigned long j = jiffies;

    data = kmalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    init_waitqueue_head (&data->wait);

    /* write the first lines in the buffer */
    seq_printf(m, "   time   delta  inirq       cpu command\n");
    seq_printf(m, "%9li  %3li     %i    %6i  %s\n",
            j, 0L, irq_count() ? 1 : 0, current->pid, current->comm);

    /* fill the data for our timer function */
    data->prevjiffies = j;
    data->buf = m;
    data->loops = JIT_ASYNC_LOOPS;

    /* register the timer */
    timer_setup(&data->timer, jit_timer_fn, 0);
    mod_timer(&data->timer, j + tdelay);

    /* wait for the buffer to fill */
    wait_event_interruptible(data->wait, !data->loops);
    if (signal_pending(current))
        return -ERESTARTSYS;

    kfree(data);

    return 0;
}

void jit_tasklet_fn(unsigned long arg)
{
    struct jit_data *data = (struct jit_data *)arg;
    unsigned long j = jiffies;

    seq_printf(data->buf, "%9li  %3li     %i    %6i   %i   %s\n",
            j, j - data->prevjiffies, in_interrupt() ? 1 : 0,
            current->pid, smp_processor_id(), current->comm);

    if (--data->loops) {
        data->prevjiffies = j;
        if (data->hi)
            tasklet_hi_schedule(&data->tlet);
        else
            tasklet_schedule(&data->tlet);
    } else {
        wake_up_interruptible(&data->wait);
    }
}

/* the /proc function: allocate everything to allow concurrency */
static int jit_tasklet(struct seq_file *m, void *v)
{
    struct jit_data *data;
    unsigned long j = jiffies;
    long hi = (long)v;

    data = kmalloc(sizeof(*data), GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    init_waitqueue_head (&data->wait);

    /* write the first lines in the buffer */
    seq_printf(m, "   time   delta  inirq    cpu command\n");
    seq_printf(m, "%9li  %3li     %i    %6i   %s\n",
            j, 0L, irq_count() ? 1 : 0, current->pid, current->comm);

    /* fill the data for our tasklet function */
    data->prevjiffies = j;
    data->buf = m;
    data->loops = JIT_ASYNC_LOOPS;

    /* register the tasklet */
    tasklet_init(&data->tlet, jit_tasklet_fn, (unsigned long)data);
    data->hi = hi;
    if (hi)
        tasklet_hi_schedule(&data->tlet);
    else
        tasklet_schedule(&data->tlet);

    /* wait for the buffer to fill */
    wait_event_interruptible(data->wait, !data->loops);

    if (signal_pending(current))
        return -ERESTARTSYS;

    kfree(data);

    return 0;
}

/*
 * This function prints one line of data, after sleeping one second.
 * It can sleep in different ways, according to the data pointer
 */
static int jit_fn(struct seq_file *m, void *v)
{
    unsigned long j0, j1; /* jiffies */
    wait_queue_head_t wait;

    init_waitqueue_head (&wait);
    j0 = jiffies;
    j1 = j0 + delay;

    switch((long)v) {
        case JIT_BUSY:
            while (time_before(jiffies, j1))
                cpu_relax();
            break;
        case JIT_SCHED:
            while (time_before(jiffies, j1)) {
                schedule();
            }
            break;
        case JIT_QUEUE:
            wait_event_interruptible_timeout(wait, 0, delay);
            break;
        case JIT_SCHEDTO:
            set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout (delay);
            break;
    }
    j1 = jiffies; /* actual value after we delayed */

    seq_printf(m, "%9li %9li\n", j0, j1);

    return 0;
}

/*
 * This file, on the other hand, returns the current time forever
 */
static int jit_currenttime(struct seq_file *m, void *v)
{
    struct timespec64 tv1, tv2;
    unsigned long j1;
    u64 j2;

    /* get them four */
    j1 = jiffies;
    j2 = get_jiffies_64();
    ktime_get_real_ts64(&tv1);
    ktime_get_coarse_real_ts64(&tv2);

    /* print */
    seq_printf(m, "0x%08lx 0x%016Lx %10i.%06i\n"
            "%40i.%09i\n",
            j1, j2,
            (int) tv1.tv_sec, (int) tv1.tv_nsec,
            (int) tv2.tv_sec, (int) tv2.tv_nsec);

    return 0;
}

static int currenttime_open(struct inode *inode, struct file *file)
{
    return single_open(file, jit_currenttime, NULL);
}

static const struct proc_ops currenttime_proc_ops = {
    .proc_open    = currenttime_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release
};

static int jitbusy_open(struct inode *inode, struct file *file)
{
    return single_open(file, jit_fn, (void *)JIT_BUSY);
}

static const struct proc_ops jitbusy_proc_ops = {
    .proc_open    = jitbusy_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release
};

static int jitsched_open(struct inode *inode, struct file *file)
{
    return single_open(file, jit_fn, (void *)JIT_SCHED);
}

static const struct proc_ops jitsched_proc_ops = {
    .proc_open    = jitsched_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release
};

static int jitqueue_open(struct inode *inode, struct file *file)
{
    return single_open(file, jit_fn, (void*)JIT_QUEUE);
}

static const struct proc_ops jitqueue_proc_ops = {
    .proc_open    = jitqueue_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release
};

static int jitschedto_open(struct inode *inode, struct file *file)
{
    return single_open(file, jit_fn, (void *)JIT_SCHEDTO);
}

static const struct proc_ops jitschedto_proc_ops = {
    .proc_open    = jitschedto_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release
};

static int jitimer_open(struct inode *inode, struct file *file)
{
    return single_open(file, jit_timer, NULL);
}

static const struct proc_ops jitimer_proc_ops = {
    .proc_open    = jitimer_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release
};

static int jitasklet_open(struct inode *inode, struct file *file)
{
    return single_open(file, jit_tasklet, NULL);
}

static const struct proc_ops jitasklet_proc_ops = {
    .proc_open    = jitasklet_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release
};

static int jitasklethi_open(struct inode *inode, struct file *file)
{
    return single_open(file, jit_tasklet, (void *)1);
}

static const struct proc_ops jitasklethi_proc_ops = {
    .proc_open    = jitasklethi_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release
};

int __init jit_init(void)
{
    proc_create("currenttime", 0, NULL, &currenttime_proc_ops);
    proc_create("jitbusy", 0, NULL, &jitbusy_proc_ops);
    proc_create("jitsched", 0, NULL, &jitsched_proc_ops);
    proc_create("jitqueue", 0, NULL, &jitqueue_proc_ops);
    proc_create("jitschedto", 0, NULL, &jitschedto_proc_ops);

    proc_create("jitimer", 0, NULL, &jitimer_proc_ops);
    proc_create("jitasklet", 0, NULL, &jitasklet_proc_ops);
    proc_create("jitasklethi", 0, NULL, &jitasklethi_proc_ops);

    return 0; /* success */
}

void __exit jit_cleanup(void)
{
    remove_proc_entry("currenttime", NULL);
    remove_proc_entry("jitbusy", NULL);
    remove_proc_entry("jitsched", NULL);
    remove_proc_entry("jitqueue", NULL);
    remove_proc_entry("jitschedto", NULL);

    remove_proc_entry("jitimer", NULL);
    remove_proc_entry("jitasklet", NULL);
    remove_proc_entry("jitasklethi", NULL);
}

module_init(jit_init);
module_exit(jit_cleanup);
