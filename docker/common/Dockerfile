# Create a docker image that can be used to build both an initramfs and the
# Linux kernel.

FROM debian:10.8-slim

# Install the minimum set of tools needed to build busybox.
RUN apt-get update     &&\
    apt-get install -y   \
        bc               \
        bison            \
        build-essential  \
        cpio             \
        flex             \
        libelf-dev       \
        libncurses-dev   \
        libssl-dev       \
        vim-tiny         \
        curl
