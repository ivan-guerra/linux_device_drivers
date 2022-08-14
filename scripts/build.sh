#!/bin/bash

# This script builds the initramfs and kernel bzImage with the added option to
# configure the kernel. QEMU can use the initramfs and bzImage archives to
# boot a VM for kernel debug and testing (see run.sh).

# Source global project configurations.
source config.sh

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

    if [ -f "${LINUX_KDEV_BIN_DIR}/${INITRAMFS_AR}" ]
    then
        echo -e "${LGREEN}${INITRAMFS_AR} successfully saved to " \
                "${LINUX_KDEV_BIN_DIR}/${INITRAMFS_AR}${NC}"
    else
        echo -e "${LRED}error failed to create initramfs archive, see " \
                "the console log above for details${NC}"
        exit 1
    fi
}

MakeKernelConfig()
{
    pushd $LINUX_KDEV_DOCKER_KERNEL_CONFIG_PATH
        docker build \
            --build-arg USER_ID=$(id -u) \
            --build-arg GROUP_ID=$(id -g) \
            -t kconfig:latest .

        KDEV_SRC="/home/kdev/linux"
        KDEV_OBJ="/home/kdev/build/linux-x86-basic"
        BUILDER_NAME="kernel-configurator"
        docker run -it --rm --name $BUILDER_NAME \
            -v $LINUX_KDEV_KERNEL_SRC_PATH:$KDEV_SRC:rw \
            -v $LINUX_KDEV_KERNEL_OBJ_PATH:$KDEV_OBJ:rw \
            kconfig:latest
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

MakeKernelImage()
{
    # We create a temporary kernel 'builder' image that compiles the Linux
    # kernel. The kernel bzImage is extracted from the temporary image and
    # placed in $LINUX_KDEV_BIN_DIR.
    pushd $LINUX_KDEV_DOCKER_KERNEL_BUILD_PATH
        docker build \
            --build-arg USER_ID=$(id -u) \
            --build-arg GROUP_ID=$(id -g) \
            -t kbuild:latest .

        KDEV_SRC="/home/kdev/linux"
        KDEV_OBJ="/home/kdev/build/linux-x86-basic"
        BUILDER_NAME="kernel-builder"
        docker run -it --name $BUILDER_NAME \
            -v $LINUX_KDEV_KERNEL_SRC_PATH:$KDEV_SRC:rw \
            -v $LINUX_KDEV_KERNEL_OBJ_PATH:$KDEV_OBJ:rw \
            kbuild:latest
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
    # Build a common base image used by both the initramfs and kernel builder
    # Docker images.
    MakeCommonBaseImage

    # Create the kernel binary output directory if it does not already exist.
    # Caching the build objects on the host allows us to pass the obj dir
    # as a volume to the docker containers. This is handy for speeding up
    # builds because we don't build from scratch every time.
    mkdir -pv $LINUX_KDEV_KERNEL_OBJ_PATH

    if [ ! -f "${LINUX_KDEV_BIN_DIR}/${INITRAMFS_AR}" ]
    then
        # We're missing an initramfs, create one.
        MakeInitramfs
    fi

    if [ ! -f "${LINUX_KDEV_KERNEL_OBJ_PATH}/.config" ]
    then
        # Missing kernel config, create one.
        MakeKernelConfig
    else
        # A .config already exists. Prompt the User in case they want to
        # create a new config with this build.
        read -p "Do you want to generate a new kernel .config? " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]
        then
            MakeKernelConfig
        fi
    fi

    # Generate the kernel bzImage.
    MakeKernelImage
}

Main
