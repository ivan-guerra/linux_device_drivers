/*
 * scullv.h - scullv char device definitions.
 *
 * Credit - "Linux Device Drivers" by Alessandro Rubini and Jonathan Corbet,
 * published by O'Reilly and Associates.
 */
#ifndef _SCULLV_H_
#define _SCULLV_H_

#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/semaphore.h>

/*
 * Macros to help debugging
 */
#undef PDEBUG /* undef it, just in case */
#ifdef SCULLV_DEBUG
#ifdef __KERNEL__
/* This one if debugging is on, and kernel space */
#define PDEBUG(fmt, args...) printk(KERN_DEBUG "scullv: " fmt, ##args)
#else
/* This one for user space */
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
#endif
#else
#define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */

#define SCULLV_MAJOR 0 /* dynamic major by default */
#define SCULLV_DEVS 4 /* scullv0 through scullv3 */

/*
 * The bare device is a variable-length region of memory.
 * Use a linked list of indirect blocks.
 *
 * "scullv_dev->data" points to an array of pointers, each
 * pointer refers to a memory page.
 *
 * The array (quantum-set) is SCULLV_QSET long.
 */
#define SCULLV_ORDER 0 /* one page at a time */
#define SCULLV_QSET 500

struct scullv_dev {
	void **data;
	struct scullv_dev *next; /* next listitem */
	int qset; /* the current array size */
	int order;
	size_t size; /* 32-bit will suffice */
	struct semaphore sem; /* Mutual exclusion */
	struct cdev cdev;
};

extern struct scullv_dev *scullv_devices;

extern struct file_operations scullv_fops;

/*
 * The different configurable parameters
 */
extern int scullv_major; /* main.c */
extern int scullv_devs;
extern int scullv_order;
extern int scullv_qset;

/*
 * Prototypes for shared functions
 */
int scullv_trim(struct scullv_dev *dev);
struct scullv_dev *scullv_follow(struct scullv_dev *dev, int n);

#ifdef SCULLV_DEBUG
#define SCULLV_USE_PROC
#endif

#endif
