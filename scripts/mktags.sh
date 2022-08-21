#!/bin/bash

# This script generates the cscope and tags files for the kernel source tree's
# x86 sources. See this SO post for more details/source material:
# https://stackoverflow.com/questions/33676829/vim-configuration-for-linux-kernel-development

source config.sh

MakeCScopeFiles()
{
    echo "creating kernel cscope.files for x86, this may take a minute..."

    # Create cscope.files for generic kernel sources and sources found under
    # arch/x86.
    find $LINUX_KDEV_KERNEL_SRC_PATH                                       \
         -path "$LINUX_KDEV_KERNEL_SRC_PATH/arch*"            -prune -o    \
         -path "$LINUX_KDEV_KERNEL_SRC_PATH/tmp*"             -prune -o    \
         -path "$LINUX_KDEV_KERNEL_SRC_PATH/Documentation*"   -prune -o    \
         -path "$LINUX_KDEV_KERNEL_SRC_PATH/scripts*"         -prune -o    \
         -path "$LINUX_KDEV_KERNEL_SRC_PATH/tools*"           -prune -o    \
         -path "$LINUX_KDEV_KERNEL_SRC_PATH/include/config*"  -prune -o    \
         -path "$LINUX_KDEV_KERNEL_SRC_PATH/usr/include*"     -prune -o    \
         -type f                                       \
         -not -name '*.mod.c'                          \
         -name "*.[chsS]" -print > cscope.files
    find $LINUX_KDEV_KERNEL_SRC_PATH/arch/x86                              \
         -path "$LINUX_KDEV_KERNEL_SRC_PATH/arch/x86/configs" -prune -o    \
         -path "$LINUX_KDEV_KERNEL_SRC_PATH/arch/x86/kvm"     -prune -o    \
         -path "$LINUX_KDEV_KERNEL_SRC_PATH/arch/x86/lguest"  -prune -o    \
         -path "$LINUX_KDEV_KERNEL_SRC_PATH/arch/x86/xen"     -prune -o    \
         -type f                                       \
         -not -name '*.mod.c'                          \
         -name "*.[chsS]" -print >> cscope.files
}

MakeCScopeIndex()
{
    # -b Build cross-reference only.
    # -q Enable fast symbol lookup via an inverted index.
    # -k Don't index the C standard library.
    cscope -b -q -k
}

MakeCTags()
{
    # Speeds things up to use the existing cscope.files.
    ctags -L cscope.files
}

InstallIndices()
{
    # Move indices to the root of the project.
    mv cscope.in.out cscope.out cscope.po.out $LINUX_KDEV_PROJECT_PATH
    mv tags $LINUX_KDEV_PROJECT_PATH
}

Main()
{
    # Verify cscope is installed.
    if ! command -v cscope --help &> /dev/null
    then
        echo "${LRED}error cscope is not installed${NC}"
        exit 1
    fi

    # Verify ctags is installed.
    if ! command -v ctags --help &> /dev/null
    then
        echo "${LRED}error 'ctags' is not installed${NC}"
        exit 1
    fi

    MakeCScopeFiles
    MakeCScopeIndex
    MakeCTags
    InstallIndices

    # Cleanup, we don't need to keep the cscope.files around.
    rm cscope.files
}

Main
