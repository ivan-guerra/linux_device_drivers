/*
 * kdatasize.c - Print the size of common data items from kernel space.
 *
 * Credit - "Linux Device Drivers" by Alessandro Rubini and Jonathan Corbet,
 * published by O'Reilly and Associates.
 */
#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/utsname.h>
#include <linux/errno.h>

static void __exit data_cleanup(void)
{
	/* never called */
}

int __init data_init(void)
{
	/* print information and return an error */
	printk("arch   Size:  char  short  int  long   ptr long-long "
	       " u8 u16 u32 u64\n");
	printk("%-12s  %3i   %3i   %3i   %3i   %3i   %3i      "
	       "%3i %3i %3i %3i\n",
	       utsname()->machine, (int)sizeof(char), (int)sizeof(short),
	       (int)sizeof(int), (int)sizeof(long), (int)sizeof(void *),
	       (int)sizeof(long long), (int)sizeof(__u8), (int)sizeof(__u16),
	       (int)sizeof(__u32), (int)sizeof(__u64));

	return -ENODEV;
}

module_init(data_init);
module_exit(data_cleanup);

MODULE_LICENSE("Dual BSD/GPL");
