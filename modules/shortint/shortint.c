/*
 * shortint.c - Simple Hardware Operations and Raw Tests (w/ interrupts)
 *
 * FIXME - This driver is not safe with concurrent readers/writers.
 *
 * Credit - "Linux Device Drivers" by Alessandro Rubini and Jonathan Corbet,
 * published by O'Reilly and Associates.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/sched.h>
#include <linux/kernel.h> /* printk() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/delay.h> /* udelay */
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>

#include <asm/io.h>

#define SHORT_NR_PORTS 8 /* use 8 ports by default */

/*
 * all of the parameters have no "short_" prefix, to save typing when
 * specifying them at load time
 */
static int major = 0; /* dynamic by default */
module_param(major, int, 0);

static int use_mem = 0; /* default is I/O-mapped */
module_param(use_mem, int, 0);

/* default is the first printer port on PC's. "short_base" is there too
   because it's what we want to use in the code */
static unsigned long base = 0x378;
unsigned long short_base = 0;
module_param(base, long, 0);

/* The interrupt line is undefined by default. "short_irq" is as above */
static int irq = -1;
volatile int short_irq = -1;
module_param(irq, int, 0);

static int probe = 0; /* select at load time how to probe irq line */
module_param(probe, int, 0);

static int wq = 0; /* select at load time whether a workqueue is used */
module_param(wq, int, 0);

static int tasklet = 0; /* select whether a tasklet is used */
module_param(tasklet, int, 0);

static int share = 0; /* select at load time whether install a shared irq */
module_param(share, int, 0);

MODULE_AUTHOR("Alessandro Rubini");
MODULE_LICENSE("Dual BSD/GPL");

unsigned long short_buffer = 0;
unsigned long volatile short_head;
volatile unsigned long short_tail;
DECLARE_WAIT_QUEUE_HEAD(short_queue);

/* Set up our tasklet if we're doing that. */
void short_do_tasklet(struct tasklet_struct *t);
DECLARE_TASKLET(short_tasklet, short_do_tasklet);

void short_do_work(struct work_struct *w);
void short_do_thingy(void);

/*
 * Atomicly increment an index into short_buffer
 */
static inline void short_incr_bp(volatile unsigned long *index, int delta)
{
	unsigned long new = *index + delta;
	barrier(); /* Don't optimize these two together */
	*index = (new >= (short_buffer + PAGE_SIZE)) ? short_buffer : new;
}

/*
 * The devices with low minor numbers write/read burst of data to/from
 * specific I/O ports (by default the parallel ones).
 *
 * The device with 128 as minor number returns ascii strings telling
 * when interrupts have been received. Writing to the device toggles
 * 00/FF on the parallel data lines. If there is a loopback wire, this
 * generates interrupts.
 */

int short_open(struct inode *inode, struct file *filp)
{
	return 0;
}

int short_release(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t short_i_read(struct file *filp, char __user *buf, size_t count,
		     loff_t *f_pos)
{
	int count0;
	DEFINE_WAIT(wait);

	while (short_head == short_tail) {
		prepare_to_wait(&short_queue, &wait, TASK_INTERRUPTIBLE);
		if (short_head == short_tail)
			schedule();
		finish_wait(&short_queue, &wait);
		if (signal_pending(current)) /* a signal arrived */
			return -ERESTARTSYS; /* tell the fs layer to handle it */
	}
	/* count0 is the number of readable data bytes */
	count0 = short_head - short_tail;
	if (count0 < 0) /* wrapped */
		count0 = short_buffer + PAGE_SIZE - short_tail;
	if (count0 < count)
		count = count0;

	if (copy_to_user(buf, (char *)short_tail, count))
		return -EFAULT;
	short_incr_bp(&short_tail, count);
	return count;
}

ssize_t short_i_write(struct file *filp, const char __user *buf, size_t count,
		      loff_t *f_pos)
{
	int written = 0, odd = *f_pos & 1;
	unsigned long port = short_base; /* output to the parallel data latch */
	void *address = (void *)short_base;

	if (use_mem) {
		while (written < count)
			iowrite8(0xff * ((++written + odd) & 1), address);
	} else {
		while (written < count)
			outb(0xff * ((++written + odd) & 1), port);
	}

	*f_pos += count;
	return written;
}

struct file_operations short_i_fops = {
	.owner = THIS_MODULE,
	.read = short_i_read,
	.write = short_i_write,
	.open = short_open,
	.release = short_release,
};

irqreturn_t short_interrupt(int irq, void *dev_id)
{
	struct timespec64 tv;
	int written;

	ktime_get_real_ts64(&tv);

	/* Write a 16 byte record. Assume PAGE_SIZE is a multiple of 16 */
	written = sprintf((char *)short_head, "%08u.%06u\n",
			  (int)(tv.tv_sec % 100000000),
			  (int)(tv.tv_nsec) / 1000);
	BUG_ON(written != 16);
	short_incr_bp(&short_head, written);
	wake_up_interruptible(&short_queue); /* awake any reading process */
	return IRQ_HANDLED;
}

/*
 * The following two functions are equivalent to the previous one,
 * but split in top and bottom half. First, a few needed variables
 */

#define NR_TIMEVAL 512 /* length of the array of time values */

struct timespec64 tv_data[NR_TIMEVAL]; /* too lazy to allocate it */
volatile struct timespec64 *tv_head = tv_data;
volatile struct timespec64 *tv_tail = tv_data;

static struct work_struct short_wq;

int short_wq_count = 0;

/*
 * Increment a circular buffer pointer in a way that nobody sees
 * an intermediate value.
 */
static inline void short_incr_tv(volatile struct timespec64 **tvp)
{
	if (*tvp == (tv_data + NR_TIMEVAL - 1))
		*tvp = tv_data; /* Wrap */
	else
		(*tvp)++;
}

void short_do_tasklet(struct tasklet_struct *t)
{
	short_do_thingy();
}

void short_do_work(struct work_struct *w)
{
	short_do_thingy();
}

void short_do_thingy(void)
{
	int savecount = short_wq_count, written;
	short_wq_count = 0; /* we have already been removed from the queue */
	/*
	 * The bottom half reads the tv array, filled by the top half,
	 * and prints it to the circular text buffer, which is then consumed
	 * by reading processes
	 */

	/* First write the number of interrupts that occurred before this bh */
	written = sprintf((char *)short_head, "bh after %6i\n", savecount);
	short_incr_bp(&short_head, written);

	/*
	 * Then, write the time values. Write exactly 16 bytes at a time,
	 * so it aligns with PAGE_SIZE
	 */

	do {
		written = sprintf((char *)short_head, "%08u.%06u\n",
				  (int)(tv_tail->tv_sec % 100000000),
				  (int)(tv_tail->tv_nsec) / 1000);
		short_incr_bp(&short_head, written);
		short_incr_tv(&tv_tail);
	} while (tv_tail != tv_head);

	wake_up_interruptible(&short_queue); /* awake any reading process */
}

irqreturn_t short_wq_interrupt(int irq, void *dev_id)
{
	/* Grab the current time information. */
	ktime_get_real_ts64((struct timespec64 *)tv_head);
	short_incr_tv(&tv_head);

	/* Queue the bh. Don't worry about multiple enqueueing */
	schedule_work(&short_wq);

	short_wq_count++; /* record that an interrupt arrived */
	return IRQ_HANDLED;
}

/*
 * Tasklet top half
 */

irqreturn_t short_tl_interrupt(int irq, void *dev_id)
{
	ktime_get_real_ts64(
		(struct timespec64 *)
			tv_head); /* cast to stop 'volatile' warning */
	short_incr_tv(&tv_head);
	tasklet_schedule(&short_tasklet);
	short_wq_count++; /* record that an interrupt arrived */
	return IRQ_HANDLED;
}

irqreturn_t short_sh_interrupt(int irq, void *dev_id)
{
	int value, written;
	struct timespec64 tv;

	/* If it wasn't short, return immediately */
	value = inb(short_base);
	if (!(value & 0x80))
		return IRQ_NONE;

	/* clear the interrupting bit */
	outb(value & 0x7F, short_base);

	/* the rest is unchanged */

	ktime_get_real_ts64(&tv);
	written = sprintf((char *)short_head, "%08u.%06u\n",
			  (int)(tv.tv_sec % 100000000),
			  (int)(tv.tv_nsec) / 1000);
	short_incr_bp(&short_head, written);
	wake_up_interruptible(&short_queue); /* awake any reading process */
	return IRQ_HANDLED;
}

void short_kernelprobe(void)
{
	int count = 0;
	do {
		unsigned long mask;

		mask = probe_irq_on();
		outb_p(0x10, short_base + 2); /* enable reporting */
		outb_p(0x00, short_base); /* clear the bit */
		outb_p(0xFF, short_base); /* set the bit: interrupt! */
		outb_p(0x00, short_base + 2); /* disable reporting */
		udelay(5); /* give it some time */
		short_irq = probe_irq_off(mask);

		if (short_irq == 0) { /* none of them? */
			printk(KERN_INFO
			       "shortint: no irq reported by probe\n");
			short_irq = -1;
		}
		/*
		 * if more than one line has been activated, the result is
		 * negative. We should service the interrupt (no need for lpt port)
		 * and loop over again. Loop at most five times, then give up
		 */
	} while (short_irq < 0 && count++ < 5);
	if (short_irq < 0)
		printk("shortint: probe failed %i times, giving up\n", count);
}

void short_cleanup(void)
{
	if (short_irq >= 0) {
		outb(0x0, short_base + 2); /* disable the interrupt */
		if (!share)
			free_irq(short_irq, NULL);
		else
			free_irq(short_irq, short_sh_interrupt);
	}
	/* Make sure we don't leave work queue/tasklet functions running */
	if (tasklet)
		tasklet_disable(&short_tasklet);
	else
		flush_scheduled_work();

	unregister_chrdev(major, "shortint");
	if (use_mem) {
		iounmap((void __iomem *)short_base);
		release_mem_region(short_base, SHORT_NR_PORTS);
	} else {
		release_region(short_base, SHORT_NR_PORTS);
	}
	if (short_buffer)
		free_page(short_buffer);
}

int short_init(void)
{
	int result;

	/*
	 * first, sort out the base/short_base ambiguity: we'd better
	 * use short_base in the code, for clarity, but allow setting
	 * just "base" at load time. Same for "irq".
	 */
	short_base = base;
	short_irq = irq;

	/* Get our needed resources. */
	if (!use_mem) {
		if (!request_region(short_base, SHORT_NR_PORTS, "shortint")) {
			printk(KERN_INFO
			       "shortint: can't get I/O port address 0x%lx\n",
			       short_base);
			return -ENODEV;
		}

	} else {
		if (!request_mem_region(short_base, SHORT_NR_PORTS,
					"shortint")) {
			printk(KERN_INFO
			       "shortint: can't get I/O mem address 0x%lx\n",
			       short_base);
			return -ENODEV;
		}

		/* also, ioremap it */
		short_base = (unsigned long)ioremap(short_base, SHORT_NR_PORTS);
		/* Hmm... we should check the return value */
	}
	/* Here we register our device - should not fail thereafter */
	result = register_chrdev(major, "shortint", &short_i_fops);
	if (result < 0) {
		printk(KERN_INFO "shortint: can't get major number\n");
		if (!use_mem) {
			release_region(short_base, SHORT_NR_PORTS);
		} else {
			release_mem_region(short_base, SHORT_NR_PORTS);
		}
		return result;
	}
	if (major == 0)
		major = result; /* dynamic */

	short_buffer = __get_free_pages(GFP_KERNEL, 0);
	if (!short_buffer) {
		printk(KERN_INFO "shortint: can't allocate buffer page\n");
		short_cleanup();
	}
	short_head = short_tail = short_buffer;

	/*
	 * Fill the workqueue structure, used for the bottom half handler.
	 * The cast is there to prevent warnings about the type of the
	 * (unused) argument.
	 */
	/* this line is in short_init() */
	INIT_WORK(&short_wq, short_do_work);

	/*
	 * Now we deal with the interrupt: either kernel-based
	 * autodetection, DIY detection or default number
	 */

	if (short_irq < 0 && probe == 1)
		short_kernelprobe();

	if (short_irq < 0) /* not yet specified: force the default on */
		switch (short_base) {
		case 0x378:
			short_irq = 7;
			break;
		case 0x278:
			short_irq = 2;
			break;
		case 0x3bc:
			short_irq = 5;
			break;
		}

	/*
	 * If shared has been specified, installed the shared handler
	 * instead of the normal one. Do it first, before a -EBUSY will
	 * force short_irq to -1.
	 */
	if (short_irq >= 0 && share > 0) {
		result = request_irq(short_irq, short_sh_interrupt, IRQF_SHARED,
				     "shortint", short_sh_interrupt);
		if (result) {
			printk(KERN_INFO
			       "shortint: can't get assigned irq %i\n",
			       short_irq);
			short_irq = -1;
		} else { /* actually enable it -- assume this *is* a parallel port */
			outb(0x10, short_base + 2);
		}
		return 0; /* the rest of the function only installs handlers */
	}

	if (short_irq >= 0) {
		result = request_irq(short_irq, short_interrupt, 0, "shortint",
				     NULL);
		if (result) {
			printk(KERN_INFO
			       "shortint: can't get assigned irq %i\n",
			       short_irq);
			short_irq = -1;
		} else { /* actually enable it -- assume this *is* a parallel port */
			outb(0x10, short_base + 2);
		}
	}

	/*
	 * Ok, now change the interrupt handler if using top/bottom halves
	 * has been requested
	 */
	if (short_irq >= 0 && (wq + tasklet) > 0) {
		free_irq(short_irq, NULL);
		result = request_irq(short_irq,
				     tasklet ? short_tl_interrupt :
					       short_wq_interrupt,
				     0, "short-bh", NULL);
		if (result) {
			printk(KERN_INFO
			       "short-bh: can't get assigned irq %i\n",
			       short_irq);
			short_irq = -1;
		}
	}

	return 0;
}

module_init(short_init);
module_exit(short_cleanup);
