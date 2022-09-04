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
    pushd $LDD3_DOCKER_COMMON_PATH
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

    # Add the module src/kernel objects to the initramfs Docker build context.
    cp -r $LDD3_MODULE_SRC_PATH $LDD3_DOCKER_INITRAMFS_PATH

    pushd $LDD3_DOCKER_INITRAMFS_PATH
        docker build -t kinitramfs:latest .

        BUILDER_NAME="initramfs-builder"
        docker create -it --name $BUILDER_NAME \
            -e CCACHE_DIR=/ccache \
            -v $LDD3_CCACHE_PATH:/ccache:rw \
            kinitramfs:latest bash
        docker cp $BUILDER_NAME:"/kdev/$INITRAMFS_AR" $LDD3_BIN_DIR
        docker rm -f $BUILDER_NAME
    popd

    # Remove module source from the initramfs Docker build context.
    rm -r $LDD3_DOCKER_INITRAMFS_PATH/$(basename $LDD3_MODULE_SRC_PATH)

    CheckInstall $LDD3_BIN_DIR/$INITRAMFS_AR
}

MakeKernelImage()
{
    # We create a temporary kernel 'builder' image that compiles the Linux
    # kernel. The kernel bzImage is extracted from the temporary image and
    # placed in $LDD3_BIN_DIR.
    pushd $LDD3_DOCKER_KERNEL_PATH
        docker build \
            --build-arg USER=$USER \
            --build-arg HOME=$HOME \
            --build-arg USER_ID=$(id -u) \
            --build-arg GROUP_ID=$(id -g) \
            -t kbuild:latest .

        BUILDER_NAME="kernel-builder"
        docker run -it --name $BUILDER_NAME \
            -e CCACHE_DIR=/ccache \
            -e KERNEL_SRC_DIR=$LDD3_KERNEL_SRC_PATH \
            -e KERNEL_OBJ_DIR=$LDD3_KERNEL_OBJ_PATH \
            -e MODULE_SRC_DIR=$LDD3_MODULE_SRC_PATH \
            -v $LDD3_KERNEL_SRC_PATH:$LDD3_KERNEL_SRC_PATH:rw \
            -v $LDD3_KERNEL_OBJ_PATH:$LDD3_KERNEL_OBJ_PATH:rw \
            -v $LDD3_MODULE_SRC_PATH:$LDD3_MODULE_SRC_PATH:rw \
            -v $LDD3_CCACHE_PATH:/ccache:rw \
            kbuild:latest
        docker cp -L \
            $BUILDER_NAME:$LDD3_KERNEL_OBJ_PATH/arch/x86_64/boot/bzImage $LDD3_BIN_DIR
        docker rm -f $BUILDER_NAME
    popd

    CheckInstall $LDD3_BIN_DIR/bzImage
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
    mkdir -pv $LDD3_KERNEL_OBJ_PATH
    mkdir -pv $LDD3_CCACHE_PATH

    # Generate the kernel bzImage.
    MakeKernelImage

    # Generate the initramfs.
    MakeInitramfs
}

Main
