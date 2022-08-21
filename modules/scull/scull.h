#ifndef _SCULL_H_
#define _SCULL_H_

#include <linux/cdev.h>
#include <linux/semaphore.h>

#ifndef SCULL_MAJOR
#define SCULL_MAJOR 0 /* use dynamic assignment */
#endif

#ifndef SCULL_NR_DEVS
#define SCULL_NR_DEVS 4 /* scull0 through scull3 */
#endif

#ifndef SCULL_QUANTUM
#define SCULL_QUANTUM 4000
#endif

#ifndef SCULL_QSET
#define SCULL_QSET    1000
#endif

extern int scull_major;
extern int scull_nr_devs;
extern int scull_quantum;
extern int scull_qset;

/*
 * Representation of scull quantum sets.
 */
struct scull_qset {
	void **data;
	struct scull_qset *next;
};

struct scull_dev {
	struct scull_qset *data;  /* Pointer to first quantum set */
	int quantum;              /* the current quantum size */
	int qset;                 /* the current array size */
	unsigned long size;       /* amount of data stored here */
	unsigned int access_key;  /* used by sculluid and scullpriv */
	struct semaphore sem;     /* mutual exclusion */
	struct cdev cdev;	      /* Char device structure */
};

int scull_trim(struct scull_dev *dev);
ssize_t scull_read(struct file *filp, char __user *buf, size_t count,
                   loff_t *f_pos);
ssize_t scull_write(struct file *filp, const char __user *buf, size_t count,
                    loff_t *f_pos);

#endif
