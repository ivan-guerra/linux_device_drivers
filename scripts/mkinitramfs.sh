#!/bin/bash

# This script outputs a minimal initramfs archive that can be passed as the
# argument to the -initrd option of QEMU. There are a number of options,
# you can view all options and their description by executing
# './mkinitramfs.sh --help'.

# Source global project configurations.
source config.sh

# Arguments passed to 'docker build' when constructing the initramfs.
# See CreateInitramfs().
ROOT_DIR="/kdev/"
INITRAMFS_AR="initramfs-busybox-x86.cpio.gz"
OUT_DIR=$LINUX_KDEV_BIN_DIR
BUSYBOX_VER="1.31.1"

Help()
{
    echo "usage: mkinitramfs.sh [OPTION]..."
    echo "options:"
    echo -e "-r ROOT_DIR, --root-dir ROOT_DIR"
    echo -e "\tdocker container root directory"
    echo -e "-i INITRAMFS_AR, --initramfs-name INITRAMFS_AR"
    echo -e "\tname of the output initramfs archive file (including extension)"
    echo -e "-o OUT_DIR, --out-dir OUT_DIR"
    echo -e "\tinitramfs archive output directory"
    echo -e "-b BUSYBOX_VER, --busybox-version BUSYBOX_VER"
    echo -e "\tBusybox version in x.y.z format"
    echo -e "-h, --help"
    echo -e "\tprint this help message"
}

PrintConfig()
{
    echo "mkinitramfs.sh - configuration"
    echo -e "\tROOT_DIR     = ${ROOT_DIR}"
    echo -e "\tINITRAMFS_AR = ${INITRAMFS_AR}"
    echo -e "\tOUT_DIR      = ${OUT_DIR}"
    echo -e "\tBUSYBOX_VER  = ${BUSYBOX_VER}"
}

CreateInitramfs()
{
    # We create a temporary initramfs 'builder' image that compiles a crude
    # initramfs and archives it. That archive file is extracted via 'docker cp'
    # from the image.
    pushd $LINUX_KDEV_DOCKER_INITRAMFS_PATH
        docker build \
            --build-arg ROOT_DIR=$ROOT_DIR \
            --build-arg INITRAMFS_AR=$INITRAMFS_AR \
            --build-arg BUSYBOX_VER=$BUSYBOX_VER \
            -t initramfs:latest .

        BUILDER_NAME="initramfs-builder"
        docker create -it --name $BUILDER_NAME initramfs:latest bash
        docker cp $BUILDER_NAME:$ROOT_DIR/$INITRAMFS_AR $OUT_DIR
        docker rm -f $BUILDER_NAME
    popd

    if [ -f "${OUT_DIR}/${INITRAMFS_AR}" ]
    then
        echo -e "${LGREEN}${INITRAMFS_AR} successfully saved to " \
                "${OUT_DIR}/${INITRAMFS_AR}${NC}"
    else
        echo -e "${LRED}error failed to create initramfs archive, see " \
                "the console log above for details${NC}"
        exit 1
    fi
}

Main()
{
    # Show the script configuration post option parsing.
    PrintConfig

    # Create the output directory if it does not already exist.
    mkdir -pv $OUT_DIR

    # Compile the initramfs archive file.
    CreateInitramfs
}

VALID_ARGS=$(getopt -o r:i:o:b:h --long \
             root-dir:,initramfs-name:,out-dir:,busybox-version:,help -- "$@")
if [[ $? -ne 0 ]]; then
    Help
    exit 1;
fi

eval set -- "$VALID_ARGS"
while [ : ]; do
  case "$1" in
    -r | --root-dir)
        ROOT_DIR=$2
        shift 2
        ;;
    -i | --initramfs-name)
        INITRAMFS_AR=$2
        shift 2
        ;;
    -o | --out-dir)
        OUT_DIR=$2
        shift 2
        ;;
    -b | --busybox-version)
        BUSYBOX_VER=$2
        shift 2
        ;;
    -h | --help)
        Help
        exit 0
        ;;
    --) shift;
        break
        ;;
  esac
done

Main
