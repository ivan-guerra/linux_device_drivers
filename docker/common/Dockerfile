# Create a docker image that can be used to build both an initramfs and the
# Linux kernel.

FROM debian:10.8-slim

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
