#!/bin/bash

KERNEL_SRC_DIR="${HOME}/linux"
OBJ_DIR="${HOME}/build/linux-x86-basic"

mkdir -pv $OBJ_DIR
pushd $KERNEL_SRC_DIR
    make O=$OBJ_DIR x86_64_defconfig &&\
    make O=$OBJ_DIR kvm_guest.config &&\
    make O=$OBJ_DIR nconfig
popd
