/*
 * scullp.c -- the scull pipe char module
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>	/* copy_*_user */

#include "scullp.h"

/*
 * Our parameters which can be set at load time.
 */
int scull_p_major    = SCULL_P_MAJOR;
int scull_p_minor    = 0;
int scull_p_nr_devs = SCULL_P_NR_DEVS;	/* number of bare scullp devices */
int scull_p_buffer  = SCULL_P_BUFFER;	/* buffer size */

module_param(scull_p_major, int, S_IRUGO);
module_param(scull_p_minor, int, S_IRUGO);
module_param(scull_p_nr_devs, int, S_IRUGO);
module_param(scull_p_buffer, int, 0);

MODULE_AUTHOR("Alessandro Rubini, Jonathan Corbet");
MODULE_LICENSE("Dual BSD/GPL");

static struct scull_pipe *scull_p_devices;

static int spacefree(struct scull_pipe *dev);

static int scull_p_open(struct inode *inode, struct file *filp)
{
	struct scull_pipe *dev;

	dev = container_of(inode->i_cdev, struct scull_pipe, cdev);
	filp->private_data = dev;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	if (!dev->buffer) {
		/* allocate the buffer */
		dev->buffer = kmalloc(scull_p_buffer, GFP_KERNEL);
		if (!dev->buffer) {
			up(&dev->sem);
			return -ENOMEM;
		}
	}
	dev->buffersize = scull_p_buffer;
	dev->end = dev->buffer + dev->buffersize;
	dev->rp = dev->wp = dev->buffer; /* rd and wr from the beginning */

	/* use f_mode,not  f_flags: it's cleaner (fs/open.c tells why) */
	if (filp->f_mode & FMODE_READ)
		dev->nreaders++;
	if (filp->f_mode & FMODE_WRITE)
		dev->nwriters++;
	up(&dev->sem);

	return nonseekable_open(inode, filp);
}

static int scull_p_release(struct inode *inode, struct file *filp)
{
	struct scull_pipe *dev = filp->private_data;

	/* remove this filp from the asynchronously notified filp's */
	scull_p_fasync(-1, filp, 0);
	down(&dev->sem);
	if (filp->f_mode & FMODE_READ)
		dev->nreaders--;
	if (filp->f_mode & FMODE_WRITE)
		dev->nwriters--;
	if (dev->nreaders + dev->nwriters == 0) {
		kfree(dev->buffer);
		dev->buffer = NULL; /* the other fields are not checked on open */
	}
	up(&dev->sem);
	return 0;
}

/*
 * Data management: read and write
 */
static ssize_t scull_p_read (struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct scull_pipe *dev = filp->private_data;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	while (dev->rp == dev->wp) { /* nothing to read */
		up(&dev->sem); /* release the lock */
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		PDEBUG("\"%s\" reading: going to sleep\n", current->comm);
		if (wait_event_interruptible(dev->inq, (dev->rp != dev->wp)))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		/* otherwise loop, but first reacquire the lock */
		if (down_interruptible(&dev->sem))
			return -ERESTARTSYS;
	}
	/* ok, data is there, return something */
	if (dev->wp > dev->rp)
		count = min(count, (size_t)(dev->wp - dev->rp));
	else /* the write pointer has wrapped, return data up to dev->end */
		count = min(count, (size_t)(dev->end - dev->rp));
	if (copy_to_user(buf, dev->rp, count)) {
		up (&dev->sem);
		return -EFAULT;
	}
	dev->rp += count;
	if (dev->rp == dev->end)
		dev->rp = dev->buffer; /* wrapped */
	up (&dev->sem);

	/* finally, awake any writers and return */
	wake_up_interruptible(&dev->outq);
	PDEBUG("\"%s\" did read %li bytes\n",current->comm, (long)count);
	return count;
}

/* Wait for space for writing; caller must hold device semaphore.  On
 * error the semaphore will be released before returning. */
static int scull_getwritespace(struct scull_pipe *dev, struct file *filp)
{
	while (spacefree(dev) == 0) { /* full */
		DEFINE_WAIT(wait);

		up(&dev->sem);
		if (filp->f_flags & O_NONBLOCK)
			return -EAGAIN;
		PDEBUG("\"%s\" writing: going to sleep\n",current->comm);
		prepare_to_wait(&dev->outq, &wait, TASK_INTERRUPTIBLE);
		if (spacefree(dev) == 0)
			schedule();
		finish_wait(&dev->outq, &wait);
		if (signal_pending(current))
			return -ERESTARTSYS; /* signal: tell the fs layer to handle it */
		if (down_interruptible(&dev->sem))
			return -ERESTARTSYS;
	}
	return 0;
}

/* How much space is free? */
static int spacefree(struct scull_pipe *dev)
{
	if (dev->rp == dev->wp)
		return dev->buffersize - 1;
	return ((dev->rp + dev->buffersize - dev->wp) % dev->buffersize) - 1;
}

static ssize_t scull_p_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct scull_pipe *dev = filp->private_data;
	int result;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	/* Make sure there's space to write */
	result = scull_getwritespace(dev, filp);
	if (result)
		return result; /* scull_getwritespace called up(&dev->sem) */

	/* ok, space is there, accept something */
	count = min(count, (size_t)spacefree(dev));
	if (dev->wp >= dev->rp)
		count = min(count, (size_t)(dev->end - dev->wp)); /* to end-of-buf */
	else /* the write pointer has wrapped, fill up to rp-1 */
		count = min(count, (size_t)(dev->rp - dev->wp - 1));
	PDEBUG("Going to accept %li bytes to %p from %p\n", (long)count, dev->wp, buf);
	if (copy_from_user(dev->wp, buf, count)) {
		up (&dev->sem);
		return -EFAULT;
	}
	dev->wp += count;
	if (dev->wp == dev->end)
		dev->wp = dev->buffer; /* wrapped */
	up(&dev->sem);

	/* finally, awake any reader */
	wake_up_interruptible(&dev->inq);  /* blocked in read() and select() */

	/* and signal asynchronous readers, explained late in chapter 5 */
	if (dev->async_queue)
		kill_fasync(&dev->async_queue, SIGIO, POLL_IN);
	PDEBUG("\"%s\" did write %li bytes\n",current->comm, (long)count);
	return count;
}

//static unsigned int scull_p_poll(struct file *filp, poll_table *wait)
//{
//	struct scull_pipe *dev = filp->private_data;
//	unsigned int mask = 0;
//
//	/*
//	 * The buffer is circular; it is considered full
//	 * if "wp" is right behind "rp" and empty if the
//	 * two are equal.
//	 */
//	down(&dev->sem);
//	poll_wait(filp, &dev->inq,  wait);
//	poll_wait(filp, &dev->outq, wait);
//	if (dev->rp != dev->wp)
//		mask |= POLLIN | POLLRDNORM;	/* readable */
//	if (spacefree(dev))
//		mask |= POLLOUT | POLLWRNORM;	/* writable */
//	up(&dev->sem);
//	return mask;
//}

int scull_p_fasync(int fd, struct file *filp, int mode)
{
	struct scull_pipe *dev = filp->private_data;

	return fasync_helper(fd, filp, mode, &dev->async_queue);
}

#ifdef SCULL_P_DEBUG

static void *scull_p_seq_start(struct seq_file *s, loff_t *pos)
{
    if (*pos >= scull_p_nr_devs)
        return NULL;
    return (scull_p_devices + *pos);
}

static void *scull_p_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
    (*pos)++;
    if (*pos >= scull_p_nr_devs)
        return NULL;
    return (scull_p_devices + *pos);
}

static void scull_p_seq_stop(struct seq_file *s, void *v)
{
    return;
}

static int scull_p_seq_show(struct seq_file *s, void *v)
{

	int i;
	struct scull_pipe *p = (struct scull_pipe *) v;

	seq_printf(s, "Default buffersize is %i\n", scull_p_buffer);
    if (down_interruptible(&p->sem))
        return -ERESTARTSYS;

    seq_printf(s, "\nDevice %i: %p\n", i, p);
    seq_printf(s, "   Buffer: %p to %p (%i bytes)\n", p->buffer, p->end, p->buffersize);
    /* seq_printf(s, "   Queues: %p %p\n", p->inq, p->outq); */
    seq_printf(s, "   rp %p   wp %p\n", p->rp, p->wp);
    seq_printf(s, "   readers %i   writers %i\n", p->nreaders, p->nwriters);

    up(&p->sem);

    return 0;
}

static const struct seq_operations scull_seq_ops = {
    .start = scull_p_seq_start,
    .next = scull_p_seq_next,
    .stop = scull_p_seq_stop,
    .show = scull_p_seq_show
};

static int scull_p_proc_open(struct inode *inode, struct file *file)
{
    return seq_open(file, &scull_seq_ops);
}

static const struct proc_ops scull_proc_ops = {
    .proc_open = scull_p_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = seq_release
};

/* proc entry pointer, see scull_p_create_proc() and scull_p_remove_proc(). */
static struct proc_dir_entry *entry = NULL;

/*
 * Actually create (and remove) the /proc file(s).
 */
static void scull_p_create_proc(void)
{
    entry = proc_create("scullpseq", 0, NULL, &scull_proc_ops);
}

static void scull_p_remove_proc(void)
{
	/* no problem if it was not registered */
	proc_remove(entry);
}

#endif

/*
 * The ioctl() implementation
 */
long scull_p_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > SCULL_IOC_MAXNR) return -ENOTTY;

    /*
     * See https://github.com/torvalds/linux/commit/96d4f267e40f9509e8a66e2b39e8b95655617693#diff-9efdd363c29c5e515e7631680a121c71434f8921c1c481ff7c4a212cf35d83bd
     * for explanation of the change to the access check.
     */
    if (!access_ok((void __user *)arg, _IOC_SIZE(cmd)))
	    return -EFAULT;

	switch(cmd) {
	  case SCULL_P_IOCTSIZE:
		scull_p_buffer = arg;
		break;
	  case SCULL_P_IOCQSIZE:
		return scull_p_buffer;
	  default:  /* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}

	return 0;
}

__poll_t scull_p_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct scull_pipe *dev = filp->private_data;
	__poll_t mask = 0;

	/*
	 * The buffer is circular; it is considered full
	 * if "wp" is right behind "rp" and empty if the
	 * two are equal.
	 */
	down(&dev->sem);
	poll_wait(filp, &dev->inq,  wait);
	poll_wait(filp, &dev->outq, wait);
	if (dev->rp != dev->wp)
		mask |= POLLIN | POLLRDNORM;	/* readable */
	if (spacefree(dev))
		mask |= POLLOUT | POLLWRNORM;	/* writable */
	up(&dev->sem);

	return mask;
}

static struct file_operations scull_pipe_fops = {
	.owner          = THIS_MODULE,
	.read           = scull_p_read,
	.write          = scull_p_write,
	.unlocked_ioctl = scull_p_ioctl,
    .poll           = scull_p_poll,
    .fasync         = scull_p_fasync,
	.open           = scull_p_open,
	.release        = scull_p_release
};

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
void scull_p_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(scull_p_major, scull_p_minor);

#ifdef SCULL_P_DEBUG
    scull_p_remove_proc();
#endif

	if (!scull_p_devices)
		return; /* nothing else to release */

	for (i = 0; i < scull_p_nr_devs; i++) {
		cdev_del(&scull_p_devices[i].cdev);
		kfree(scull_p_devices[i].buffer);
	}
	kfree(scull_p_devices);
	unregister_chrdev_region(devno, scull_p_nr_devs);
	scull_p_devices = NULL; /* pedantic */
}

/*
 * Set up the char_dev structure for this device.
 */
static void scull_p_setup_cdev(struct scull_pipe *dev, int index)
{
	int err, devno = MKDEV(scull_p_major, scull_p_minor + index);

	cdev_init(&dev->cdev, &scull_pipe_fops);
	dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &scull_pipe_fops;
	err = cdev_add(&dev->cdev, devno, 1);

	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding scullpipe%d", err, index);
}

int scull_p_init_module(void)
{
	int result, i;
	dev_t dev = 0;

    /*
     * Get a range of minor numbers to work with, asking for a dynamic
     * major unless directed otherwise at load time.
     */
	if (scull_p_major) {
		dev = MKDEV(scull_p_major, scull_p_minor);
		result = register_chrdev_region(dev, scull_p_nr_devs, "scullp");
	} else {
		result = alloc_chrdev_region(&dev, scull_p_minor, scull_p_nr_devs,
				"scullp");
		scull_p_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "scullp: can't get major %d\n", scull_p_major);
		return result;
	}

	scull_p_devices = kmalloc(scull_p_nr_devs * sizeof(struct scull_pipe), GFP_KERNEL);
	if (!scull_p_devices) {
        result = -ENOMEM;
        goto fail;
	}
	memset(scull_p_devices, 0, scull_p_nr_devs * sizeof(struct scull_pipe));
	for (i = 0; i < scull_p_nr_devs; i++) {
		init_waitqueue_head(&(scull_p_devices[i].inq));
		init_waitqueue_head(&(scull_p_devices[i].outq));
		sema_init(&scull_p_devices[i].sem, 1);
		scull_p_setup_cdev(scull_p_devices + i, i);
	}

#ifdef SCULL_P_DEBUG
    scull_p_create_proc();
#endif

    return 0; /* success */

  fail:
	scull_p_cleanup_module();
	return result;
}

module_init(scull_p_init_module);
module_exit(scull_p_cleanup_module);
