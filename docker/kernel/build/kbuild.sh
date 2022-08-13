#!/bin/bash

# This script will build the Linux kernel source under KERNEL_SRC_DIR with the
# assumption that the kernel configuration was generated in a previous step.

KERNEL_SRC_DIR="${HOME}/linux"
OBJ_DIR="${HOME}/build/linux-x86-basic"

mkdir -p $OBJ_DIR
pushd $KERNEL_SRC_DIR
    make O=$OBJ_DIR -j$(nproc)
popd
