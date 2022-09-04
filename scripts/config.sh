#!/bin/bash

# This script configures the default search paths for many of the binaries
# and configuration files used by the project. Other scripts may source this
# file to find the resources that they need.

LGREEN='\033[1;32m'
LRED='\033[1;31m'
NC='\033[0m'

# Root directory.
LDD3_PROJECT_PATH=$(dirname $(pwd))

# Scripts directory.
LDD3_SCRIPTS_PATH="${LDD3_PROJECT_PATH}/scripts"

# initramfs docker context path.
LDD3_DOCKER_INITRAMFS_PATH="${LDD3_PROJECT_PATH}/docker/initramfs"

# Kernel config docker context path.
LDD3_DOCKER_KERNEL_PATH="${LDD3_PROJECT_PATH}/docker/kernel"

# Path to a common base image used by the initramfs and kernel Dockerfiles.
LDD3_DOCKER_COMMON_PATH="${LDD3_PROJECT_PATH}/docker/common"

# Linux kernel source tree directory.
LDD3_KERNEL_SRC_PATH="${LDD3_PROJECT_PATH}/linux"

# Driver module source path.
LDD3_MODULE_SRC_PATH="${LDD3_PROJECT_PATH}/modules"

# Binary directory.
LDD3_BIN_DIR="${LDD3_PROJECT_PATH}/bin"

# Linux kernel build files.
LDD3_KERNEL_OBJ_PATH="${LDD3_BIN_DIR}/obj"

# ccache path. Allows for re-use of the cache between container builds.
LDD3_CCACHE_PATH="${LDD3_BIN_DIR}/ccache"
