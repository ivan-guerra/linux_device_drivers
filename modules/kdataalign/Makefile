ifeq ($(KERNELRELEASE),)
	# Set KERNELDIR to point to the kernel object directory.
    KERNELDIR ?= $(KERNEL_OBJ_DIR)
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
	# Called from kernel build system: just declare what our modules are.
	obj-m	:= kdataalign.o
endif
