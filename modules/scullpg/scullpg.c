/*
 * scullpg.c - scullpg ('pg' for page) char device implementation.
 *
 * Credit - "Linux Device Drivers" by Alessandro Rubini and Jonathan Corbet,
 * published by O'Reilly and Associates.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <asm/uaccess.h>

#include "scullpg.h" /* local definitions */

int scullpg_major = SCULLPG_MAJOR;
int scullpg_devs = SCULLPG_DEVS; /* number of bare scullpg devices */
int scullpg_qset = SCULLPG_QSET;
int scullpg_order = SCULLPG_ORDER;

module_param(scullpg_major, int, 0);
module_param(scullpg_devs, int, 0);
module_param(scullpg_qset, int, 0);
module_param(scullpg_order, int, 0);
MODULE_AUTHOR("Alessandro Rubini");
MODULE_LICENSE("Dual BSD/GPL");

struct scullpg_dev *scullpg_devices; /* allocated in scullpg_init */

int scullpg_trim(struct scullpg_dev *dev);
void scullpg_cleanup(void);

#ifdef SCULLPG_USE_PROC /* don't waste space if unused */

static void *scullpg_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= scullpg_devs)
		return NULL;

	return (scullpg_devices + *pos);
}

static void *scullpg_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos)++;
	if (*pos >= scullpg_devs)
		return NULL;

	return (scullpg_devices + *pos);
}

static void scullpg_seq_stop(struct seq_file *s, void *v)
{
	return;
}

static int scullpg_seq_show(struct seq_file *s, void *v)
{
	int i, j, quantum, qset;
	struct scullpg_dev *d;

	for (i = 0; i < scullpg_devs; i++) {
		d = &scullpg_devices[i];
		if (down_interruptible(&d->sem))
			return -ERESTARTSYS;

		qset = d->qset; /* retrieve the features of each device */
		quantum = PAGE_SIZE << d->order;
		seq_printf(s, "\nDevice %i: qset %i, order %i, sz %li\n", i,
			   qset, d->order, (long)(d->size));
		for (; d; d = d->next) { /* scan the list */
			seq_printf(s, "  item at %p, qset at %p\n", d, d->data);
			if (d->data &&
			    !d->next) /* dump only the last item - save space */
				for (j = 0; j < qset; j++) {
					if (d->data[j])
						seq_printf(s, "    % 4i:%8p\n",
							   j, d->data[j]);
				}
		}
		up(&scullpg_devices[i].sem);
	}
	return 0;
}

static const struct seq_operations scullpg_seq_ops = {
	.start = scullpg_seq_start,
	.next = scullpg_seq_next,
	.stop = scullpg_seq_stop,
	.show = scullpg_seq_show,
};

static int scullpg_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &scullpg_seq_ops);
}

static const struct proc_ops scullpg_proc_ops = {
	.proc_open = scullpg_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

/* proc entry pointer, see scullpg_create_proc() and scullpg_remove_proc(). */
static struct proc_dir_entry *entry = NULL;

/*
 * Actually create (and remove) the /proc file(s).
 */
static void scullpg_create_proc(void)
{
	entry = proc_create("scullpgmem", 0, NULL, &scullpg_proc_ops);
}

static void scullpg_remove_proc(void)
{
	/* no problem if it was not registered */
	proc_remove(entry);
}

#endif /* SCULLPG_USE_PROC */

int scullpg_open(struct inode *inode, struct file *filp)
{
	struct scullpg_dev *dev; /* device information */

	/*  Find the device */
	dev = container_of(inode->i_cdev, struct scullpg_dev, cdev);

	/* now trim to 0 the length of the device if open was write-only */
	if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
		if (down_interruptible(&dev->sem))
			return -ERESTARTSYS;
		scullpg_trim(dev); /* ignore errors */
		up(&dev->sem);
	}

	/* and use filp->private_data to point to the device data */
	filp->private_data = dev;

	return 0; /* success */
}

int scullpg_release(struct inode *inode, struct file *filp)
{
	return 0;
}

struct scullpg_dev *scullpg_follow(struct scullpg_dev *dev, int n)
{
	while (n--) {
		if (!dev->next) {
			dev->next =
				kmalloc(sizeof(struct scullpg_dev), GFP_KERNEL);
			memset(dev->next, 0, sizeof(struct scullpg_dev));
		}
		dev = dev->next;
		continue;
	}
	return dev;
}

/*
 * Data management: read and write
 */

ssize_t scullpg_read(struct file *filp, char __user *buf, size_t count,
		     loff_t *f_pos)
{
	struct scullpg_dev *dev = filp->private_data; /* the first listitem */
	struct scullpg_dev *dptr;
	int quantum = PAGE_SIZE << dev->order;
	int qset = dev->qset;
	int itemsize = quantum * qset; /* how many bytes in the listitem */
	int item, s_pos, q_pos, rest;
	ssize_t retval = 0;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	if (*f_pos > dev->size)
		goto nothing;
	if (*f_pos + count > dev->size)
		count = dev->size - *f_pos;
	/* find listitem, qset index, and offset in the quantum */
	item = ((long)*f_pos) / itemsize;
	rest = ((long)*f_pos) % itemsize;
	s_pos = rest / quantum;
	q_pos = rest % quantum;

	/* follow the list up to the right position (defined elsewhere) */
	dptr = scullpg_follow(dev, item);

	if (!dptr->data)
		goto nothing; /* don't fill holes */
	if (!dptr->data[s_pos])
		goto nothing;
	if (count > quantum - q_pos)
		count = quantum -
			q_pos; /* read only up to the end of this quantum */

	if (copy_to_user(buf, dptr->data[s_pos] + q_pos, count)) {
		retval = -EFAULT;
		goto nothing;
	}
	up(&dev->sem);

	*f_pos += count;
	return count;

nothing:
	up(&dev->sem);
	return retval;
}

ssize_t scullpg_write(struct file *filp, const char __user *buf, size_t count,
		      loff_t *f_pos)
{
	struct scullpg_dev *dev = filp->private_data;
	struct scullpg_dev *dptr;
	int quantum = PAGE_SIZE << dev->order;
	int qset = dev->qset;
	int itemsize = quantum * qset;
	int item, s_pos, q_pos, rest;
	ssize_t retval = -ENOMEM; /* our most likely error */

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	/* find listitem, qset index and offset in the quantum */
	item = ((long)*f_pos) / itemsize;
	rest = ((long)*f_pos) % itemsize;
	s_pos = rest / quantum;
	q_pos = rest % quantum;

	/* follow the list up to the right position */
	dptr = scullpg_follow(dev, item);
	if (!dptr->data) {
		dptr->data = kmalloc(qset * sizeof(void *), GFP_KERNEL);
		if (!dptr->data)
			goto nomem;
		memset(dptr->data, 0, qset * sizeof(char *));
	}
	/* Allocate a quantum using who pages. */
	if (!dptr->data[s_pos]) {
		dptr->data[s_pos] =
			(void *)__get_free_pages(GFP_KERNEL, dptr->order);
		if (!dptr->data[s_pos])
			goto nomem;
		memset(dptr->data[s_pos], 0, PAGE_SIZE << scullpg_order);
	}
	if (count > quantum - q_pos)
		count = quantum -
			q_pos; /* write only up to the end of this quantum */
	if (copy_from_user(dptr->data[s_pos] + q_pos, buf, count)) {
		retval = -EFAULT;
		goto nomem;
	}
	*f_pos += count;

	/* update the size */
	if (dev->size < *f_pos)
		dev->size = *f_pos;
	up(&dev->sem);
	return count;

nomem:
	up(&dev->sem);
	return retval;
}

loff_t scullpg_llseek(struct file *filp, loff_t off, int whence)
{
	struct scullpg_dev *dev = filp->private_data;
	long newpos;

	switch (whence) {
	case 0: /* SEEK_SET */
		newpos = off;
		break;

	case 1: /* SEEK_CUR */
		newpos = filp->f_pos + off;
		break;

	case 2: /* SEEK_END */
		newpos = dev->size + off;
		break;

	default: /* can't happen */
		return -EINVAL;
	}

	if (newpos < 0)
		return -EINVAL;

	filp->f_pos = newpos;

	return newpos;
}

struct file_operations scullpg_fops = {
	.owner = THIS_MODULE,
	.llseek = scullpg_llseek,
	.read = scullpg_read,
	.write = scullpg_write,
	.open = scullpg_open,
	.release = scullpg_release,
};

int scullpg_trim(struct scullpg_dev *dev)
{
	struct scullpg_dev *next, *dptr;
	int qset = dev->qset; /* "dev" is not-null */
	int i;

	for (dptr = dev; dptr; dptr = next) { /* all the list items */
		if (dptr->data) {
			for (i = 0; i < qset; i++)
				if (dptr->data[i])
					free_pages((unsigned long)dptr->data[i],
						   dptr->order);

			kfree(dptr->data);
			dptr->data = NULL;
		}
		next = dptr->next;
		if (dptr != dev)
			kfree(dptr); /* all of them but the first */
	}
	dev->size = 0;
	dev->qset = scullpg_qset;
	dev->order = scullpg_order;
	dev->next = NULL;
	return 0;
}

static void scullpg_setup_cdev(struct scullpg_dev *dev, int index)
{
	int err, devno = MKDEV(scullpg_major, index);

	cdev_init(&dev->cdev, &scullpg_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scullpg_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding scull%d", err, index);
}

int scullpg_init(void)
{
	int result, i;
	dev_t dev = MKDEV(scullpg_major, 0);

	/*
	 * Register your major, and accept a dynamic number.
	 */
	if (scullpg_major)
		result = register_chrdev_region(dev, scullpg_devs, "scullpg");
	else {
		result = alloc_chrdev_region(&dev, 0, scullpg_devs, "scullpg");
		scullpg_major = MAJOR(dev);
	}
	if (result < 0)
		return result;

	/*
	 * allocate the devices -- we can't have them static, as the number
	 * can be specified at load time
	 */
	scullpg_devices =
		kmalloc(scullpg_devs * sizeof(struct scullpg_dev), GFP_KERNEL);
	if (!scullpg_devices) {
		result = -ENOMEM;
		goto fail_malloc;
	}
	memset(scullpg_devices, 0, scullpg_devs * sizeof(struct scullpg_dev));
	for (i = 0; i < scullpg_devs; i++) {
		scullpg_devices[i].qset = scullpg_qset;
		scullpg_devices[i].order = scullpg_order;
		sema_init(&scullpg_devices[i].sem, 1);
		scullpg_setup_cdev(scullpg_devices + i, i);
	}

#ifdef SCULLPG_USE_PROC /* only when available */
	scullpg_create_proc();
#endif
	return 0; /* succeed */

fail_malloc:
	unregister_chrdev_region(dev, scullpg_devs);
	return result;
}

void scullpg_cleanup(void)
{
#ifdef SCULLPG_USE_PROC
	scullpg_remove_proc();
#endif

	for (int i = 0; i < scullpg_devs; i++) {
		cdev_del(&scullpg_devices[i].cdev);
		scullpg_trim(scullpg_devices + i);
	}
	kfree(scullpg_devices);

	unregister_chrdev_region(MKDEV(scullpg_major, 0), scullpg_devs);
}

module_init(scullpg_init);
module_exit(scullpg_cleanup);
