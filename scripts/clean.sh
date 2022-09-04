#!/bin/bash

# This script cleans up the source tree leaving it as if a fresh clone of
# the repository was made.

# Source the project configuration.
source config.sh

# Remove the binary directory.
if [ -d $LDD3_BIN_DIR ]
then
    echo -e "${LGREEN}Removing '$LDD3_BIN_DIR'${NC}"
    rm -r $LDD3_BIN_DIR
fi

# Remove module build artefacts.
pushd $LDD3_MODULE_SRC_PATH
    make clean
popd
