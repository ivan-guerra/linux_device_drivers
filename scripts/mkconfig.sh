#!/bin/bash

# This script outputs a kernel config file. When the script runs a default
# x86_64 kernel config with kvm guest extras will be created. The User will
# be prompted via nconfig to add any additional configs they like to the
# kernel config.

# Source global project configurations.
source config.sh

CreateKernelConfig()
{
    pushd $LINUX_KDEV_DOCKER_KERNEL_CONFIG_PATH
        docker build \
            --build-arg USER_ID=$(id -u) \
            --build-arg GROUP_ID=$(id -g) \
            -t kernel:latest .

        KDEV_SRC="/home/kdev/linux"
        KDEV_OBJ="/home/kdev/build/linux-x86-basic"
        BUILDER_NAME="kernel-configurator"
        docker run -it --rm --name $BUILDER_NAME \
            -v $LINUX_KDEV_KERNEL_SRC_PATH:$KDEV_SRC:rw \
            -v $LINUX_KDEV_KERNEL_OBJ_PATH:$KDEV_OBJ:rw \
            kernel:latest
    popd

    if [ -f "${LINUX_KDEV_KERNEL_OBJ_PATH}/.config" ]
    then
        echo -e "${LGREEN}kernel config successfully saved to " \
                "${LINUX_KDEV_KERNEL_OBJ_PATH}/.config${NC}"
    else
        echo -e "${LRED}error failed to create kernel config, see " \
                "the console log above for details${NC}"
        exit 1
    fi
}

Main()
{
    # Create the kernel binary output directory if it does not already exist.
    mkdir -pv $LINUX_KDEV_KERNEL_OBJ_PATH

    # Create the kernel config.
    CreateKernelConfig
}

Main
