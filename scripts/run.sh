#!/bin/bash

# This script launches the kernel under QEMU. run.sh includes support for a
# number of commandline options. See 'run.sh -h' for details.

source config.sh

DEBUG="OFF"

Help()
{
    echo "usage: run.sh [-g]"
    echo "options:"
    echo "g    run the kernel in debug mode"
    echo "h    print this help message"
}

RunDebugSession()
{
    echo -e "Open a new terminal and attach a GDB session to begin debugging:"
    echo -e "\t\$ cd \$LINUX_OBJ_DIR"
    echo -e "\t\$ gdb vmlinux"
    echo -e "\t(gdb) target remote localhost:1234"

    qemu-system-x86_64 \
        -kernel "${LINUX_KDEV_BIN_DIR}/bzImage" \
        -initrd "${LINUX_KDEV_BIN_DIR}/initramfs-busybox-x86.cpio.gz" \
        -nographic \
        -append "console=ttyS0,115200 nokaslr" \
        -s \
        -S \
        -enable-kvm
}

RunNormalSession()
{
    qemu-system-x86_64 \
        -kernel "${LINUX_KDEV_BIN_DIR}/bzImage" \
        -initrd "${LINUX_KDEV_BIN_DIR}/initramfs-busybox-x86.cpio.gz" \
        -nographic \
        -append "console=ttyS0,115200" \
        -enable-kvm
}

Main()
{
    if [ "$DEBUG" = "ON" ]
    then
        RunDebugSession
    else
        RunNormalSession
    fi
}

while getopts ":hg" flag
do
    case "${flag}" in
        g) DEBUG="ON";;
        h) Help
           exit;;
       \?) echo "error: invalid option"
           Help
           exit 1
    esac
done

Main
