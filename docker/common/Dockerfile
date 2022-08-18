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
        ccache           \
        curl

# Use ccache (make it appear in path earlier than /usr/bin/gcc, etc.).
RUN for p in gcc g++ cc c++; do ln -vs /usr/bin/ccache /usr/local/bin/$p; done
