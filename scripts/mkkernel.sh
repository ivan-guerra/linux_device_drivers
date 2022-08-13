#!/bin/bash

# This script outputs the kernel bzImage produced by compiling the Linux kernel
# source at the project's root.

# Source global project configurations.
source config.sh

CreateKernelImage()
{
    # We create a temporary kernel 'builder' image that compiles the Linux
    # kernel. The kernel bzImage is extracted from the temporary image and
    # placed in $LINUX_KDEV_BIN_DIR.
    pushd $LINUX_KDEV_DOCKER_KERNEL_PATH
        docker build \
            --build-arg USER_ID=$(id -u) \
            --build-arg GROUP_ID=$(id -g) \
            -t kernel:latest .

        KDEV_SRC="/home/kdev/linux"
        KDEV_OBJ="/home/kdev/build/linux-x86-basic"
        BUILDER_NAME="kernel-builder"
        docker run -it --name $BUILDER_NAME \
            -v $LINUX_KDEV_KERNEL_SRC_PATH:$KDEV_SRC:rw \
            -v $LINUX_KDEV_KERNEL_OBJ_PATH:$KDEV_OBJ:rw \
            kernel:latest
        docker cp -L \
            $BUILDER_NAME:$KDEV_OBJ/arch/x86_64/boot/bzImage $LINUX_KDEV_BIN_DIR
        docker rm -f $BUILDER_NAME
    popd

    if [ -f "${LINUX_KDEV_BIN_DIR}/bzImage" ]
    then
        echo -e "${LGREEN}kernel bzImage successfully saved to " \
                "${LINUX_KDEV_BIN_DIR}/bzImage${NC}"
    else
        echo -e "${LRED}error failed to create kernel bzImage, see " \
                "the console log above for details${NC}"
        exit 1
    fi
}

Main()
{
    # Create the kernel binary output directory if it does not already exist.
    # Caching the build objects on the host allows us to pass the obj dir as a
    # volume to the builder container allowing the container to build just
    # the parts of the kernel source that changed.
    mkdir -pv $LINUX_KDEV_KERNEL_OBJ_PATH

    # Compile the kernel image.
    CreateKernelImage
}

Main
