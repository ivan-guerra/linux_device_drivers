#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include <linux/kernel.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "scull.h"

MODULE_AUTHOR("Ivan Guerra");
MODULE_DESCRIPTION("Linux Device Drivers 3rd Edition scull sample driver.");
MODULE_LICENSE("GPL");

int scull_major   = SCULL_MAJOR;
int scull_minor   = 0;
int scull_nr_devs = SCULL_NR_DEVS; /* number of bare scull devices */
int scull_quantum = SCULL_QUANTUM;
int scull_qset    = SCULL_QSET;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset, int, S_IRUGO);

struct scull_dev *scull_devices; /* allocated in scull_init_module */

#ifdef SCULL_DEBUG /* use proc only if debugging */

/*
 * The proc filesystem: function to read and entry
 */
static int scull_proc_show(struct seq_file *m, void *data)
{
    struct scull_dev *devices = m->private;

	for (int i = 0; i < scull_nr_devs; i++) {
		struct scull_dev *d = &devices[i];
		struct scull_qset *qs = d->data;
		if (down_interruptible(&d->sem))
			return -ERESTARTSYS;
		seq_printf(m, "\nDevice %i: qset %i, q %i, sz %li\n",
                   i, d->qset, d->quantum, d->size);
		for (; qs; qs = qs->next) { /* scan the list */
			seq_printf(m, "  item at %px, qset at %px\n", qs, qs->data);
			if (qs->data && !qs->next) /* dump only the last item */
				for (int j = 0; j < d->qset; j++) {
					if (qs->data[j])
						seq_printf(m, "    % 4i: %8px\n", j, qs->data[j]);
				}
		}
		up(&devices[i].sem);
	}

    return 0;
}

static int scull_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, scull_proc_show, scull_devices);
}

static const struct proc_ops scull_proc_fops = {
    .proc_open = scull_proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release
};

/*
 * Actually create (and remove) the /proc file(s).
 */
static void scull_create_proc(void)
{
	proc_create("scullmem", 0, NULL, &scull_proc_fops);
}

static void scull_remove_proc(void)
{
	/* no problem if it was not registered */
	remove_proc_entry("scullmem", NULL /* parent dir */);
}

#endif

/*
 * Empty out the scull device; must be called with the device
 * semaphore held.
 */
int scull_trim(struct scull_dev *dev)
{
	struct scull_qset *next, *dptr;
	int qset = dev->qset; /* "dev" is not-null */
	int i;

	for (dptr = dev->data; dptr; dptr = next) { /* all the list items */
		if (dptr->data) {
			for (i = 0; i < qset; i++)
				kfree(dptr->data[i]);
			kfree(dptr->data);
			dptr->data = NULL;
		}
		next = dptr->next;
		kfree(dptr);
	}

	dev->size = 0;
	dev->quantum = scull_quantum;
	dev->qset = scull_qset;
	dev->data = NULL;

	return 0;
}

struct scull_qset *scull_follow(struct scull_dev *dev, int n)
{
	struct scull_qset *qs = dev->data;

    /* Allocate first qset explicitly if need be */
	if (! qs) {
		qs = dev->data = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
		if (qs == NULL)
			return NULL;  /* Never mind */
		memset(qs, 0, sizeof(struct scull_qset));
	}

	/* Then follow the list */
	while (n--) {
		if (!qs->next) {
			qs->next = kmalloc(sizeof(struct scull_qset), GFP_KERNEL);
			if (qs->next == NULL)
				return NULL;  /* Never mind */
			memset(qs->next, 0, sizeof(struct scull_qset));
		}
		qs = qs->next;
		continue;
	}
	return qs;
}

ssize_t scull_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
	struct scull_qset *dptr;	/* the first listitem */
	int quantum = dev->quantum, qset = dev->qset;
	int itemsize = quantum * qset; /* how many bytes in the listitem */
	int item, s_pos, q_pos, rest;
	ssize_t retval = 0;

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;
	if (*f_pos >= dev->size)
		goto out;
	if (*f_pos + count > dev->size)
		count = dev->size - *f_pos;

	/* find listitem, qset index, and offset in the quantum */
	item = (long)*f_pos / itemsize;
	rest = (long)*f_pos % itemsize;
	s_pos = rest / quantum; q_pos = rest % quantum;

	/* follow the list up to the right position (defined elsewhere) */
	dptr = scull_follow(dev, item);

	if (dptr == NULL || !dptr->data || ! dptr->data[s_pos])
		goto out; /* don't fill holes */

	/* read only up to the end of this quantum */
	if (count > quantum - q_pos)
		count = quantum - q_pos;

	if (copy_to_user(buf, dptr->data[s_pos] + q_pos, count)) {
		retval = -EFAULT;
		goto out;
	}
	*f_pos += count;
	retval = count;

  out:
	up(&dev->sem);
	return retval;
}

ssize_t scull_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
	struct scull_dev *dev = filp->private_data;
	struct scull_qset *dptr;
	int quantum = dev->quantum, qset = dev->qset;
	int itemsize = quantum * qset;
	int item, s_pos, q_pos, rest;
	ssize_t retval = -ENOMEM; /* value used in "goto out" statements */

	if (down_interruptible(&dev->sem))
		return -ERESTARTSYS;

	/* find listitem, qset index and offset in the quantum */
	item = (long)*f_pos / itemsize;
	rest = (long)*f_pos % itemsize;
	s_pos = rest / quantum; q_pos = rest % quantum;

	/* follow the list up to the right position */
	dptr = scull_follow(dev, item);
	if (dptr == NULL)
		goto out;
	if (!dptr->data) {
		dptr->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
		if (!dptr->data)
			goto out;
		memset(dptr->data, 0, qset * sizeof(char *));
	}
	if (!dptr->data[s_pos]) {
		dptr->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
		if (!dptr->data[s_pos])
			goto out;
	}
	/* write only up to the end of this quantum */
	if (count > quantum - q_pos)
		count = quantum - q_pos;

	if (copy_from_user(dptr->data[s_pos]+q_pos, buf, count)) {
		retval = -EFAULT;
		goto out;
	}
	*f_pos += count;
	retval = count;

    /* update the size */
	if (dev->size < *f_pos)
		dev->size = *f_pos;

  out:
	up(&dev->sem);
	return retval;
}

int scull_open(struct inode *inode, struct file *filp)
{
	struct scull_dev *dev; /* device information */

	dev = container_of(inode->i_cdev, struct scull_dev, cdev);
	filp->private_data = dev; /* for other methods */

	/* now trim to 0 the length of the device if open was write-only */
	if ( (filp->f_flags & O_ACCMODE) == O_WRONLY) {
		scull_trim(dev); /* ignore errors */
	}

	return 0; /* success */
}

int scull_release(struct inode *inode, struct file *filp)
{
	return 0;
}

struct file_operations scull_fops = {
	.owner        = THIS_MODULE,
	.llseek       = NULL,
	.read         = scull_read,
	.write        = scull_write,
	.compat_ioctl = NULL,
	.open         = scull_open,
	.release      = scull_release
};

void scull_cleanup_module(void)
{
	int i;
	dev_t devno = MKDEV(scull_major, scull_minor);

	/* Get rid of our char dev entries */
	if (scull_devices) {
		for (i = 0; i < scull_nr_devs; i++) {
			scull_trim(scull_devices + i);
			cdev_del(&scull_devices[i].cdev);
		}
		kfree(scull_devices);
	}

#ifdef SCULL_DEBUG /* use proc only if debugging */
	scull_remove_proc();
#endif

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, scull_nr_devs);
}

/*
 * Set up the char_dev structure for this device.
 */
static void scull_setup_cdev(struct scull_dev *dev, int index)
{
	int err, devno = MKDEV(scull_major, scull_minor + index);

	cdev_init(&dev->cdev, &scull_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &scull_fops;
	err = cdev_add (&dev->cdev, devno, 1);
	/* Fail gracefully if need be */
	if (err)
		printk(KERN_NOTICE "Error %d adding scull%d", err, index);
}

int scull_init_module(void)
{
	int result, i;
	dev_t dev = 0;

    /*
     * Get a range of minor numbers to work with, asking for a dynamic
     * major unless directed otherwise at load time.
     */
	if (scull_major) {
		dev = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(dev, scull_nr_devs, "scull");
	} else {
		result = alloc_chrdev_region(&dev, scull_minor, scull_nr_devs,
				"scull");
		scull_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return result;
	}

    /*
	 * allocate the devices -- we can't have them static, as the number
	 * can be specified at load time
	 */
	scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev), GFP_KERNEL);
	if (!scull_devices) {
		result = -ENOMEM;
		goto fail;  /* Make this more graceful */
	}
	memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_dev));

    /* Initialize each device. */
	for (i = 0; i < scull_nr_devs; i++) {
		scull_devices[i].quantum = scull_quantum;
		scull_devices[i].qset = scull_qset;
		sema_init(&scull_devices[i].sem, 1);
		scull_setup_cdev(&scull_devices[i], i);
	}

    /* At this point call the init function for any friend device */
	dev = MKDEV(scull_major, scull_minor + scull_nr_devs);

#ifdef SCULL_DEBUG /* only when debugging */
	scull_create_proc();
#endif

	return 0; /* succeed */

  fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
