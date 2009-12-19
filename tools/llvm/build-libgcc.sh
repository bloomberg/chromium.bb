#!/bin/bash
# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
#@ This script builds the arm untrusted SDK.
#@ NOTE: It must be run from the native_client/ directory.
#@ NOTE: you should source: set_arm_(un)trusted_toolchain.sh
#@       before running it
######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

NATIVE_CLIENT_DIR=$(pwd)
echo ${NATIVE_CLIENT_DIR} | grep native_client$
if [ $? -ne 0 ]; then
  echo "************************************************"
  echo " Script must be run from the native_client dir. "
  echo "************************************************"
  exit 1
fi

if [ ! -e /tmp/llvm.sfi ]; then
  echo "************************************************"
  echo " Toolchain creator byproduct /tmp/llvm.sfi does "
  echo " not exist (we need its contents).              "
  echo "************************************************"
  exit 1
fi

WORKDIR=$(mktemp -d /tmp/nacl-libgcc.XXXX)
echo "Working in ${WORKDIR}..."
cd ${WORKDIR}

echo "Untarring llvm-gcc..."
tar xfj ${NATIVE_CLIENT_DIR}/../third_party/llvm/llvm-gcc-4.2-88663.tar.bz2
ln -s /tmp/llvm.sfi/llvm

export GCC_FOR_TARGET="llvm-fake-sfigcc"
export CC_FOR_TARGET="llvm-fake-sfigcc"
export CXX_FOR_TARGET="llvm-fake-sfig++"

mkdir build
cd build

SRC=$(pwd)/../llvm-gcc-4.2
LLVM=$(pwd)/../llvm
INSTALL=$(pwd)/../install-not-used

${SRC}/configure --prefix=${INSTALL} \
                 --program-prefix=llvm- \
                 --enable-llvm=${LLVM} \
                 --enable-languages=c,c++ \
                 --target=arm-none-linux-gnueabi \
                 --with-cpu=cortex-a8 \
                 --disable-thumb \
                 --disable-interwork \
                 --disable-multilib \
                 --with-arch=armv7-a \
                 --with-mode=arm

# Make is expected to fail, trick our shell
make || true

if [ ! -s gcc/libgcc.a ]; then
  echo "******************************"
  echo "  libgcc.a was not produced   "
  echo "******************************"
  exit 1
fi
if [ ! -s gcc/libgcc_eh.a ]; then
  echo "******************************"
  echo " libgcc_eh.a was not produced "
  echo "******************************"
  exit 1
fi

${RANLIB} gcc/libgcc.a gcc/libgcc_eh.a

echo "Outputs:"
echo "  $(pwd)/gcc/libgcc.a"
echo "  $(pwd)/gcc/libgcc_eh.a"

TOOLCHAIN=/usr/local/crosstool-untrusted/arm-none-linux-gnueabi
INSTALLDIR=${TOOLCHAIN}/llvm-gcc-4.2/lib/gcc/arm-none-linux-gnueabi/4.2.1
cp gcc/libgcc.a gcc/libgcc_eh.a ${INSTALLDIR}

echo "Installed to ${INSTALLDIR}"
