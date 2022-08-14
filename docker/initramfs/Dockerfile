# Create a docker image whose output is a minimal userland that can be passed
# to QEMU as an initramfs. This image repackages many of the commands and
# advice given in this 2021 blog post:
# https://mgalgs.io/2021/03/23/how-to-build-a-custom-linux-kernel-for-qemu-using-docker.html

FROM kbase:latest

ENV ROOT_DIR="/kdev/"
ENV INITRAMFS_AR="initramfs-busybox-x86.cpio.gz"
ENV BUSYBOX_VER="1.32.1"
ENV OBJ_DIR=$ROOT_DIR/obj
ENV INITRAMFS_DIR=$ROOT_DIR/initramfs/x86-busybox

# We set the shell to bash. This is helpful because later we make use of bash
# features such as brace expansion.
SHELL ["/bin/bash", "-c"]

WORKDIR $ROOT_DIR

# Build and install a statically linked busybox.
RUN curl https://busybox.net/downloads/busybox-$BUSYBOX_VER.tar.bz2 |\
        tar xjf - &&\
    cd $ROOT_DIR/busybox-$BUSYBOX_VER &&\
    mkdir -pv $OBJ_DIR/busybox-x86 &&\
    make O=$OBJ_DIR/busybox-x86 defconfig &&\
    sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/g' \
        $OBJ_DIR/busybox-x86/.config &&\
    cd $OBJ_DIR/busybox-x86 &&\
    make -j$(nproc) &&\
    make install

# Create the initramfs skeleton.
RUN mkdir -pv $INITRAMFS_DIR &&\
    cd $INITRAMFS_DIR &&\
    mkdir -pv {bin,sbin,etc,proc,sys,usr/{bin,sbin}} &&\
    cp -av $OBJ_DIR/busybox-x86/_install/* .

# The actual init script comes from the build context.
COPY init $INITRAMFS_DIR

# Archive the initramfs.
RUN cd $INITRAMFS_DIR &&\
    find . -print0 |\
    cpio --null -ov --format=newc |\
    gzip -9 > "$ROOT_DIR/$INITRAMFS_AR"
