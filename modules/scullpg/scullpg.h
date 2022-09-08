/*
 * scullpg.h - scullpg ('pg' for page) char device definitions.
 *
 * Credit - "Linux Device Drivers" by Alessandro Rubini and Jonathan Corbet,
 * published by O'Reilly and Associates.
 */
#ifndef _SCULLPG_H_
#define _SCULLPG_H_

#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/semaphore.h>

/*
 * Macros to help debugging
 */
#undef PDEBUG /* undef it, just in case */
#ifdef SCULLPG_DEBUG
#ifdef __KERNEL__
/* This one if debugging is on, and kernel space */
#define PDEBUG(fmt, args...) printk(KERN_DEBUG "scullpg: " fmt, ##args)
#else
/* This one for user space */
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
#endif
#else
#define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#undef PDEBUGG
#define PDEBUGG(fmt, args...) /* nothing: it's a placeholder */

#define SCULLPG_MAJOR 0 /* dynamic major by default */
#define SCULLPG_DEVS 4 /* scullpg0 through scullpg3 */

/*
 * The bare device is a variable-length region of memory.
 * Use a linked list of indirect blocks.
 *
 * "scullpg_dev->data" points to an array of pointers, each
 * pointer refers to a memory page.
 *
 * The array (quantum-set) is SCULLPG_QSET long.
 */
#define SCULLPG_ORDER 0 /* one page at a time */
#define SCULLPG_QSET 500

struct scullpg_dev {
	void **data;
	struct scullpg_dev *next; /* next listitem */
	int qset; /* the current array size */
	int order;
	size_t size; /* 32-bit will suffice */
	struct semaphore sem; /* Mutual exclusion */
	struct cdev cdev;
};

extern struct scullpg_dev *scullpg_devices;

extern struct file_operations scullpg_fops;

/*
 * The different configurable parameters
 */
extern int scullpg_major; /* main.c */
extern int scullpg_devs;
extern int scullpg_order;
extern int scullpg_qset;

/*
 * Prototypes for shared functions
 */
int scullpg_trim(struct scullpg_dev *dev);
struct scullpg_dev *scullpg_follow(struct scullpg_dev *dev, int n);

#ifdef SCULLPG_DEBUG
#define SCULLPG_USE_PROC
#endif

#endif
