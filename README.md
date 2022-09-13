# Linux Device Drivers 3rd Edition

This project contains the example modules introduced throughout the Linux
Device Drivers 3rd edition text. LDD3 examples are meant to be compatible with
Linux Kernel v2.6. This project uses kernel v5.19. Examples have been updated
to work with the newer kernel's updated interfaces.

Included with the project is a set of Docker containers that can be used to

* Build a custom initramfs
* Configure and build the Linux kernel
* Build out-of-tree kernel modules

The containers create the initramfs and kernel bzImage that can be handed off
to the QEMU emulator. From within the emulator, we can load/unload our custom
modules and experiment with LDD3's in memory "devices" from the safety of a
sandboxed environment.

### Checking Out the Project

This project includes the `linux-next` branch for kernel v5.19 as a submodule.
There's a lot of code and a lot of git history that comes along with the
`linux-next` submodule. To speed up the checkout process and save space on your
PC, you may want to follow these steps when cloning the repo:
```
$ git clone git@github.com:ivan-guerra/linux_device_drivers.git
$ cd linux_device_drivers
$ git submodule init
$ git submodule update --depth 1
```
You can change the argument to `--depth` to include however much history
you like or remove it completey for the full history.

### Building and Running the Examples

To build and run the examples, you will need Docker and QEMU installed on
your host PC. The steps that follow were run on a Fedora 36 PC with
Docker v20.10 and QEMU v6.2 installed.

All scripts referenced below can be found under `scripts/`.

To build the kernel, modules, and initramfs:
```
$ ./build.sh
```

To run the kernel with the init ram disk:
```
$ ./run.sh
```

Once the virtual machine boots, you can find the example modules under the
`/modules/` directory. Many of the modules include a `MODNAME_load` and
corresponding `MODNAME_unload` script. You can run these scripts to load/unload
the modules or you can do so directly yourself using the userland tools
provided (i.e., `insmod`, `rmmod`, `mknod`, etc.).

### GDB Support

It can sometimes be useful to run an interactive debugger against your module
to diagnose issues. This project includes support for GDB debug of kernel
code and modules.

You will first need to compile a kernel with debug info, kgdb debug support,
and more. You can find the details of which config parameters are required by
reading this
[Star Lab's article](https://www.starlab.io/blog/using-gdb-to-debug-the-linux-kernel).
When you're ready, run the `build.sh` script and configure/build the kernel
and all modules.

The `run.sh` script includes GDB debug support. To launch your kernel in debug
mode:
```
$ ./run.sh -g
```

From a seperate terminal, you can connect gdb to your live kernel and begin
stepping through code. Note, `$LINUX_OBJ_DIR` below refers to the directory
containing the Linux kernel objects produced by the build. These should always
be under `/path/to/linux_device_drivers/bin/obj`.
```
$ cd $LINUX_OBJ_DIR
$ gdb vmlinux
(gdb) target remote localhost:1234
```
At this point, you can set breakpoints, step through code, etc. Be wary, there
are quite a few differences between using GDB to debug the kernel versus a
userland app. It's worth taking the time to read some articles to understand
what those differences are.

### Source Material

The authors and publishers have made the text and source code for these
examples publicly avilable. Here are the links to the source material I used:

* [Linux Device Drivers 3rd Edition](https://lwn.net/Kernel/LDD3/)
* [Original Example Files](https://resources.oreilly.com/examples/9780596005900)
