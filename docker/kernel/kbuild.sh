#!/bin/bash

# This script will build the Linux kernel source under KERNEL_SRC_DIR using
# the default x86_64 configuration that ships with the kernel tree. Config
# options that improve performance/functionality of kvm guests are also
# included.

KERNEL_SRC_DIR="${HOME}/linux"
OBJ_DIR="${HOME}/build/linux-x86-basic"

mkdir -p $OBJ_DIR
pushd $KERNEL_SRC_DIR
    make O=$OBJ_DIR x86_64_defconfig &&\
    make O=$OBJ_DIR kvm_guest.config &&\
    make O=$OBJ_DIR -j$(nproc)
popd
