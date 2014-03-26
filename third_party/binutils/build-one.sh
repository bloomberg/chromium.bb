#!/bin/sh

# Script to build binutils found in /build/binutils-XXXX when inside a chroot.
# Don't call this script yourself, instead use the build-all.sh script.

set -e

if [ -z "$1" ]; then
 echo "Directory of binutils not given."
 exit 1
fi

cd "$1"
./configure --enable-gold=default --enable-threads --prefix=/build/output
make -j8 all
make install
