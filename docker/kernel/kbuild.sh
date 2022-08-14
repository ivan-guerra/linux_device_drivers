#!/bin/bash

# kbuild.sh looks to see if BUILD_CONFIG is defined, if so the kernel config is
# built otherwise the kernel sources are built.

# Note, the following variables are sourced from the environment and must be
# specified in the docker run command:
#   KERNEL_SRC_DIR
#   OBJ_DIR
#   BUILD_CONFIG

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
    mkdir -p $OBJ_DIR

    if [ -z $BUILD_CONFIG ]
    then
        BuildKernel
    else
        ConfigKernel
    fi
}

Main
