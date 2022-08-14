#!/bin/bash

# kbuild.sh looks to see if BUILD_CONFIG is defined, if so the kernel config is
# built, otherwise, the kernel sources are built.

KERNEL_SRC_DIR=$HOME/dev/linux_driver_dev/linux
OBJ_DIR=$HOME/dev/linux_driver_dev/bin/obj

ConfigKernel()
{
    pushd $KERNEL_SRC_DIR
        make O=$OBJ_DIR x86_64_defconfig &&\
        make O=$OBJ_DIR kvm_guest.config &&\
        make O=$OBJ_DIR nconfig
    popd
}

BuildKernel()
{
    pushd $KERNEL_SRC_DIR
        make O=$OBJ_DIR -j$(nproc)
    popd
}

Main()
{
    mkdir -p KERNEL_SRC_DIR
    mkdir -p OBJ_DIR

    # Note, BUILD_CONFIG is sourced from the environment and must be specified
    # in the docker run command.
    if [ -z $BUILD_CONFIG ]
    then
        BuildKernel
    else
        ConfigKernel
    fi
}

Main
