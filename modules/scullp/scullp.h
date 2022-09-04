/*
 * scullp.h - scull pipe char device definitions.
 *
 * Credit - "Linux Device Drivers" by Alessandro Rubini and Jonathan Corbet,
 * published by O'Reilly and Associates.
 */
#ifndef _SCULL_H_
#define _SCULL_H_

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */
#include <linux/wait.h> /* needed for wait_queue_head_t */
#include <linux/fs.h> /* needed for fasync_struct */
#include <linux/cdev.h>
#include <linux/semaphore.h>

/*
 * Macros to help debugging
 */
#undef PDEBUG /* undef it, just in case */
#ifdef SCULL_P_DEBUG
#ifdef __KERNEL__
/* This one if debugging is on, and kernel space */
#define PDEBUG(fmt, args...) printk(KERN_DEBUG "scull: " fmt, ##args)
#else
/* This one for user space */
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
#endif
#else
#define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */

#ifndef SCULL_P_MAJOR
#define SCULL_P_MAJOR 0 /* dynamic major by default */
#endif

#ifndef SCULL_P_NR_DEVS
#define SCULL_P_NR_DEVS 4 /* scullpipe0 through scullpipe3 */
#endif

/*
 * The pipe device is a simple circular buffer. Here is its default size.
 */
#ifndef SCULL_P_BUFFER
#define SCULL_P_BUFFER 4000
#endif

/*
 * The different configurable parameters
 */
extern int scull_major;
extern int scull_nr_devs;
extern int scull_p_buffer;

struct scull_pipe {
	wait_queue_head_t inq, outq; /* read and write queues */
	char *buffer, *end; /* begin of buf, end of buf */
	int buffersize; /* used in pointer arithmetic */
	char *rp, *wp; /* where to read, where to write */
	int nreaders, nwriters; /* number of openings for r/w */
	struct fasync_struct *async_queue; /* asynchronous readers */
	struct semaphore sem; /* mutual exclusion semaphore */
	struct cdev cdev; /* Char device structure */
};

int scull_p_init(dev_t dev);
void scull_p_cleanup(void);
long scull_p_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);

/*
 * Ioctl definitions
 */

/* Use '0xDF' as magic number */
#define SCULL_IOC_MAGIC 0xDF

#define SCULL_IOCRESET _IO(SCULL_IOC_MAGIC, 0)

/*
 * T means "Tell" directly with the argument value
 * Q means "Query": response is on the return value
 */
#define SCULL_P_IOCTSIZE _IO(SCULL_IOC_MAGIC, 1)
#define SCULL_P_IOCQSIZE _IO(SCULL_IOC_MAGIC, 2)
/* ... more to come */

#define SCULL_IOC_MAXNR 2

#endif /* _SCULL_H_ */
