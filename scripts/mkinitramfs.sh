#!/bin/bash

# This script outputs a minimal initramfs archive that can be passed as the
# argument to the -initrd option of QEMU.

# Source global project configurations.
source config.sh

CreateInitramfs()
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

Main()
{
    # Create the output directory if it does not already exist.
    mkdir -pv $LINUX_KDEV_BIN_DIR

    # Compile the initramfs archive file.
    CreateInitramfs
}

Main
