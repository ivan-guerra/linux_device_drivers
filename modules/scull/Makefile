# To build modules outside of the kernel tree, we run "make"
# in the kernel source tree; the Makefile these then includes this
# Makefile once again.
# This conditional selects whether we are being included from the
# kernel Makefile or not.

# Comment/uncomment the following line to disable/enable debugging.
DEBUG = y

# Add your debugging flag (or not) to ccflags. For details see
# linux/Documentation/kbuild/modules.rst
ifeq ($(DEBUG),y)
	DEBFLAGS = -O -g -DSCULL_DEBUG
else
	DEBFLAGS = -O2
endif

ccflags-y := $(DEBFLAGS)

ifeq ($(KERNELRELEASE),)
	# Set KERNELDIR to point to the kernel object directory.
    KERNELDIR ?= $(HOME)/dev/linux_driver_dev/bin/obj
    # The current directory is passed to sub-makes as argument
    PWD := $(shell pwd)

modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

clean:
	rm -rf *.dwo *.ko *.mod *.mod.* *.o .*.cmd modules.order Module.symvers

.PHONY: modules modules_install clean

else
    # called from kernel build system: just declare what our modules are
    obj-m := scull.o
endif
