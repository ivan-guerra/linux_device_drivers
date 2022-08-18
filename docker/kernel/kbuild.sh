#!/bin/bash

# kbuild.sh runs the series of command needed to configure and build the kernel
# and any custom drivers in MODULE_SRC_DIR.

ConfigKernel()
{
    pushd $KERNEL_SRC_DIR
        make O=$KERNEL_OBJ_DIR x86_64_defconfig &&\
        make O=$KERNEL_OBJ_DIR kvm_guest.config &&\
        make O=$KERNEL_OBJ_DIR nconfig
    popd
}

BuildKernel()
{
    pushd $KERNEL_SRC_DIR
        make O=$KERNEL_OBJ_DIR -j$(nproc)
    popd
}

BuildModules()
{
    pushd $MODULE_SRC_DIR
        make O=$KERNEL_OBJ_DIR -j$(nproc) all
    popd
}

Main()
{
    read -p "Build kernel? [y/n] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]
    then
        if [ ! -f "${KERNEL_OBJ_DIR}/.config" ]
        then
            # Missing kernel config, create one.
            ConfigKernel
        else
            # A .config already exists. Prompt the User in case they want to
            # create a new config with this build.
            read -p "Do you want to generate a new kernel .config? [y/n] " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]
            then
                ConfigKernel
            fi
        fi
        BuildKernel
    fi

    read -p "Build modules (assumes existing kernel build)? [y/n] " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]
    then
        BuildModules
    fi
}

Main
