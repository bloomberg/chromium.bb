#!/bin/bash
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

set -o nounset
set -o errexit

ROOT=$(pwd)
while [[ ${ROOT} != */native_client ]] ; do
  ROOT=$(dirname ${ROOT})
  if [[ ${ROOT} == "/" ]] ; then
    echo "ERROR: run script from with native_client/ or a subdir"
    exit -1
  fi
done

readonly ARCH=x86-64

echo "translating [${ARCH}] $1 -> $2"

# TODO(abetul): replace this with real translator step as they mature
# TODO(abetul): do not depend on any file executables outside of
#               toolchain/sandboxed_translators
readonly LIB_PATH="${ROOT}/toolchain/linux_arm-untrusted/libs-bitcode"
readonly LIBS="-lc -lm -lnacl -lpthread -lnosys"
readonly TRANS="${ROOT}/toolchain/linux_arm-untrusted/arm-none-linux-gnueabi/llvm-fake-bcld -arch ${ARCH}"

# NOTE: use this for debugging
#readonly FLAGS=--pnacl-driver-verbose
readonly FLAGS=

${TRANS} ${FLAGS} $1 -L"${LIB_PATH}" ${LIBS} -o "$2"

echo "translation complete"
