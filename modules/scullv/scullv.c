/*
 * scullv.c - scullv char device implementation.
 *
 * Credit - "Linux Device Drivers" by Alessandro Rubini and Jonathan Corbet,
 * published by O'Reilly and Associates.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h> /* printk() */
#include <linux/slab.h> /* kmalloc() */
#include <linux/vmalloc.h> /* vmalloc() */
#include <linux/fs.h> /* everything... */
#include <linux/errno.h> /* error codes */
#include <linux/types.h> /* size_t */
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/fcntl.h> /* O_ACCMODE */
#include <asm/uaccess.h>

#include "scullv.h" /* local definitions */

int scullv_major = SCULLV_MAJOR;
int scullv_devs = SCULLV_DEVS; /* number of bare scullv devices */
int scullv_qset = SCULLV_QSET;
int scullv_order = SCULLV_ORDER;

module_param(scullv_major, int, 0);
module_param(scullv_devs, int, 0);
module_param(scullv_qset, int, 0);
module_param(scullv_order, int, 0);
MODULE_AUTHOR("Alessandro Rubini");
MODULE_LICENSE("Dual BSD/GPL");

struct scullv_dev *scullv_devices; /* allocated in scullv_init */

int scullv_trim(struct scullv_dev *dev);
void scullv_cleanup(void);

#ifdef SCULLV_USE_PROC /* don't waste space if unused */

static void *scullv_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos >= scullv_devs)
		return NULL;

	return (scullv_devices + *pos);
}

static void *scullv_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	(*pos)++;
	if (*pos >= scullv_devs)
		return NULL;

	return (scullv_devices + *pos);
}

static void scullv_seq_stop(struct seq_file *s, void *v)
{
	return;
}

static int scullv_seq_show(struct seq_file *s, void *v)
{
	int i, j, quantum, qset;
	struct scullv_dev *d;

	for (i = 0; i < scullv_devs; i++) {
		d = &scullv_devices[i];
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
		up(&scullv_devices[i].sem);
	}
	return 0;
}

static const struct seq_operations scullv_seq_ops = {
	.start = scullv_seq_start,
	.next = scullv_seq_next,
	.stop = scullv_seq_stop,
	.show = scullv_seq_show,
};

static int scullv_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &scullv_seq_ops);
}

static const struct proc_ops scullv_proc_ops = {
	.proc_open = scullv_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

/* proc entry pointer, see scullv_create_proc() and scullv_remove_proc(). */
static struct proc_dir_entry *entry = NULL;

/*
 * Actually create (and remove) the /proc file(s).
 */
static void scullv_create_proc(void)
{
	entry = proc_create("scullvmem", 0, NULL, &scullv_proc_ops);
}

static void scullv_remove_proc(void)
{
	/* no problem if it was not registered */
	proc_remove(entry);
}

#endif /* SCULLV_USE_PROC */

int scullv_open(struct inode *inode, struct file *filp)
{
	struct scullv_dev *dev; /* device information */

	/*  Find the device */
	dev = container_of(inode->i_cdev, struct scullv_dev, cdev);

	/* now trim to 0 the length of the device if open was write-only */
	if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
		if (down_interruptible(&dev->sem))
			return -ERESTARTSYS;
		scullv_trim(dev); /* ignore errors */
		up(&dev->sem);
	}

	/* and use filp->private_data to point to the device data */
	filp->private_data = dev;

	return 0; /* success */
}

int scullv_release(struct inode *inode, struct file *filp)
{
	return 0;
}

struct scullv_dev *scullv_follow(struct scullv_dev *dev, int n)
{
	while (n--) {
		if (!dev->next) {
			dev->next =
				kmalloc(sizeof(struct scullv_dev), GFP_KERNEL);
			memset(dev->next, 0, sizeof(struct scullv_dev));
		}
		dev = dev->next;
		continue;
	}
	return dev;
}

/*
 * Data management: read and write
 */

ssize_t scullv_read(struct file *filp, char __user *buf, size_t count,
		    loff_t *f_pos)
{
	struct scullv_dev *dev = filp->private_data; /* the first listitem */
	struct scullv_dev *dptr;
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
	dptr = scullv_follow(dev, item);

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

ssize_t scullv_write(struct file *filp, const char __user *buf, size_t count,
		     loff_t *f_pos)
{
	struct scullv_dev *dev = filp->private_data;
	struct scullv_dev *dptr;
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
	dptr = scullv_follow(dev, item);
	if (!dptr->data) {
		dptr->data = kmalloc(qset * sizeof(void *), GFP_KERNEL);
		if (!dptr->data)
			goto nomem;
		memset(dptr->data, 0, qset * sizeof(char *));
	}
	/* Allocate a quantum using who pages. */
	if (!dptr->data[s_pos]) {
		dptr->data[s_pos] = (void *)vmalloc(PAGE_SIZE << dptr->order);
		if (!dptr->data[s_pos])
			goto nomem;
		memset(dptr->data[s_pos], 0, PAGE_SIZE << scullv_order);
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

loff_t scullv_llseek(struct file *filp, loff_t off, int whence)
{
	struct scullv_dev *dev = filp->private_data;
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

struct file_operations scullv_fops = {
	.owner = THIS_MODULE,
	.llseek = scullv_llseek,
	.read = scullv_read,
	.write = scullv_write,
	.open = scullv_open,
	.release = scullv_release,
};

int scullv_trim(struct scullv_dev *dev)
{
	struct scullv_dev *next, *dptr;
	int qset = dev->qset; /* "dev" is not-null */
	int i;

	for (dptr = dev; dptr; dptr = next) { /* all the list items */
		if (dptr->data) {
			for (i = 0; i < qset; i++)
				if (dptr->data[i])
					vfree(dptr->data[i]);

			kfree(dptr->data);
			dptr->data = NULL;
		}
		next = dptr->next;
		if (dptr != dev)
			kfree(dptr); /* all of them but the first */
	}
	dev->size = 0;
	dev->qset = scullv_qset;
	dev->order = scullv_order;
	dev->next = NULL;
	return 0;
}

static void scullv_setup_cdev(struct scullv_dev *dev, int index)
{
	int err, devno = MKDEV(scullv_major, index);

	cdev_init(&dev->cdev, &scullv_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scullv_fops;
	err = cdev_add(&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding scull%d", err, index);
}

int scullv_init(void)
{
	int result, i;
	dev_t dev = MKDEV(scullv_major, 0);

	/*
	 * Register your major, and accept a dynamic number.
	 */
	if (scullv_major)
		result = register_chrdev_region(dev, scullv_devs, "scullv");
	else {
		result = alloc_chrdev_region(&dev, 0, scullv_devs, "scullv");
		scullv_major = MAJOR(dev);
	}
	if (result < 0)
		return result;

	/*
	 * allocate the devices -- we can't have them static, as the number
	 * can be specified at load time
	 */
	scullv_devices =
		kmalloc(scullv_devs * sizeof(struct scullv_dev), GFP_KERNEL);
	if (!scullv_devices) {
		result = -ENOMEM;
		goto fail_malloc;
	}
	memset(scullv_devices, 0, scullv_devs * sizeof(struct scullv_dev));
	for (i = 0; i < scullv_devs; i++) {
		scullv_devices[i].qset = scullv_qset;
		scullv_devices[i].order = scullv_order;
		sema_init(&scullv_devices[i].sem, 1);
		scullv_setup_cdev(scullv_devices + i, i);
	}

#ifdef SCULLV_USE_PROC /* only when available */
	scullv_create_proc();
#endif
	return 0; /* succeed */

fail_malloc:
	unregister_chrdev_region(dev, scullv_devs);
	return result;
}

void scullv_cleanup(void)
{
#ifdef SCULLV_USE_PROC
	scullv_remove_proc();
#endif

	for (int i = 0; i < scullv_devs; i++) {
		cdev_del(&scullv_devices[i].cdev);
		scullv_trim(scullv_devices + i);
	}
	kfree(scullv_devices);

	unregister_chrdev_region(MKDEV(scullv_major, 0), scullv_devs);
}

module_init(scullv_init);
module_exit(scullv_cleanup);
