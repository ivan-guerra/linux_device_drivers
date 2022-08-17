#!/bin/bash

# This script builds the initramfs and kernel bzImage.
# QEMU can use the initramfs and bzImage archives to boot a VM for kernel
# debug and testing (see run.sh).

# Source global project configurations.
source config.sh

CheckInstall()
{
    if [ -f $1 ]
    then
        echo -e "${LGREEN}successfully created $1${NC}"
    else
        echo -e "${LRED}error failed to create $1, see the console log " \
                "above for details${NC}"
        exit 1
    fi
}

MakeCommonBaseImage()
{
    pushd $LINUX_KDEV_DOCKER_COMMON_PATH
        docker build -t kbase:latest .
    popd
}

MakeInitramfs()
{
    # We create a temporary initramfs 'builder' image that compiles a crude
    # initramfs and archives it. That archive file is extracted via 'docker cp'
    # from the image.

    # initramfs archive file produced by the initramfs builder container.
    # See docker/initramfs/Dockerfile for details.
    INITRAMFS_AR="initramfs-busybox-x86.cpio.gz"

    pushd $LINUX_KDEV_DOCKER_INITRAMFS_PATH
        docker build -t initramfs:latest .

        BUILDER_NAME="initramfs-builder"
        docker create -it --name $BUILDER_NAME initramfs:latest bash
        docker cp $BUILDER_NAME:"/kdev/$INITRAMFS_AR" $LINUX_KDEV_BIN_DIR
        docker rm -f $BUILDER_NAME
    popd

    CheckInstall $LINUX_KDEV_BIN_DIR/$INITRAMFS_AR
}

MakeKernelImage()
{
    # We create a temporary kernel 'builder' image that compiles the Linux
    # kernel. The kernel bzImage is extracted from the temporary image and
    # placed in $LINUX_KDEV_BIN_DIR.
    pushd $LINUX_KDEV_DOCKER_KERNEL_PATH
        docker build \
            --build-arg USER=$USER \
            --build-arg HOME=$HOME \
            --build-arg USER_ID=$(id -u) \
            --build-arg GROUP_ID=$(id -g) \
            -t kbuild:latest .

        BUILDER_NAME="kernel-builder"
        docker run -it --name $BUILDER_NAME \
            -e KERNEL_SRC_DIR=$LINUX_KDEV_KERNEL_SRC_PATH \
            -e KERNEL_OBJ_DIR=$LINUX_KDEV_KERNEL_OBJ_PATH \
            -e MODULE_SRC_DIR=$LINUX_KDEV_MODULE_SRC_PATH \
            -v $LINUX_KDEV_KERNEL_SRC_PATH:$LINUX_KDEV_KERNEL_SRC_PATH:rw \
            -v $LINUX_KDEV_KERNEL_OBJ_PATH:$LINUX_KDEV_KERNEL_OBJ_PATH:rw \
            -v $LINUX_KDEV_MODULE_SRC_PATH:$LINUX_KDEV_MODULE_SRC_PATH:rw \
            kbuild:latest
        docker cp -L \
            $BUILDER_NAME:$LINUX_KDEV_KERNEL_OBJ_PATH/arch/x86_64/boot/bzImage $LINUX_KDEV_BIN_DIR
        docker rm -f $BUILDER_NAME
    popd

    CheckInstall $LINUX_KDEV_BIN_DIR/bzImage
}

Main()
{
    # Build a common base image used by both the initramfs and kernel builder
    # Docker images.
    MakeCommonBaseImage

    # Create the kernel binary output directory if it does not already exist.
    # Caching the build objects on the host allows us to pass the obj dir
    # as a volume to the docker containers. This is handy for speeding up
    # builds because we don't build from scratch every time.
    mkdir -pv $LINUX_KDEV_KERNEL_OBJ_PATH

    # Generate the kernel bzImage.
    MakeKernelImage

    # Generate the initramfs.
    MakeInitramfs
}

Main
