#!/bin/bash

# This script cleans up the source tree leaving it as if a fresh clone of
# the repository was made.

# Source the project configuration.
source config.sh

# Remove the binary directory.
if [ -d $LINUX_KDEV_BIN_DIR ]
then
    echo -e "${LGREEN}Removing '$LINUX_KDEV_BIN_DIR'${NC}"
    rm -r $LINUX_KDEV_BIN_DIR
fi

# Remove module build artefacts.
pushd $LINUX_KDEV_MODULE_SRC_PATH
    make clean
popd
