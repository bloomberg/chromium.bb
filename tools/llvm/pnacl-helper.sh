#!/bin/bash
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
#@ This script performs various helper task for pnacl.
#@ NOTE: It must be run from the native_client/ directory.
######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

readonly PNACL_TOOLCHAIN_ROOT=toolchain/pnacl-untrusted

readonly PNACL_ARM_ROOT=${PNACL_TOOLCHAIN_ROOT}/arm
readonly PNACL_X8632_ROOT=${PNACL_TOOLCHAIN_ROOT}/x8632
readonly PNACL_X8664_ROOT=${PNACL_TOOLCHAIN_ROOT}/x8664
readonly PNACL_BITCODE_ROOT=${PNACL_TOOLCHAIN_ROOT}/bitcode


######################################################################
# Helper
######################################################################

Banner() {
  echo "######################################################################"
  echo "$@"
  echo "######################################################################"
}

Usage() {
  egrep "^#@" $0 | cut --bytes=3-
}

######################################################################
#
######################################################################

# Usage: VerifyObject <regexp> <filename>
# Ensure that the ELF "file format" string for <filename> matches <regexp>.
VerifyObject () {
  local pattern=$1 filename=$2
  echo -n "verify $filename [$pattern]: "
  local format=$(objdump -a "$filename" |
                 sed -ne 's/^.* file format //p' |
                 head -1)
  if echo "$format" | grep -q -e "^$pattern$"; then
    echo "PASS"
  else
    echo "FAIL (got '$format')"
    exit -1
  fi
}

VerifyLLVM () {
  echo -n "verify $1: "
  local n=$(objdump -a $1 2>&1 | grep -c "format")
  local good=$(objdump -a $1 2>&1 | grep -c "not recognized")
  echo "${good}"
  if [ "$n" != "${good}" ] ; then
    echo
    echo "ERROR: unexpected obj file format"
    objdump -a $1
    exit -1
  fi
}

######################################################################
# Main
######################################################################
if [ $(basename $(pwd)) != "native_client" ]; then
  echo "You must run this script from the 'native_client' directory." >&2
  exit -1
fi
if [ $# -eq 0 ] ; then
  echo
  echo "ERROR: you must specify a mode on the command line:"
  echo
  Usage
  exit -1
fi

MODE=$1
shift

#@
#@ help
#@
#@   print help for all modes
if [ $MODE} = 'help' ] ; then
  Usage
  exit 0
fi

#@
#@ download-toolchains
#@
#@   down load all the toolchains needed by pnacl
if [ ${MODE} = 'download-toolchains' ] ; then
  ./scons platform=arm --download sdl=none
  ./scons platform=x86-64 --download sdl=none
  exit 0
fi

#@
#@ organize-native-code
#@
#@   save and organize native code
if [ ${MODE} = 'organize-native-code' ] ; then
  rm -rf ${PNACL_TOOLCHAIN_ROOT}

  readonly arm_src=toolchain/linux_arm-untrusted
  readonly x86_src=toolchain/linux_x86/sdk/nacl-sdk

  Banner "arm native code: ${PNACL_ARM_ROOT}"
  mkdir -p ${PNACL_ARM_ROOT}
  cp ${arm_src}/arm-none-linux-gnueabi/llvm-gcc-4.2/lib/gcc/arm-none-linux-gnueabi/4.2.1/libgcc.a ${PNACL_ARM_ROOT}
  cp ${arm_src}/arm-newlib/arm-none-linux-gnueabi/lib/crt*.o \
     ${arm_src}/arm-newlib/arm-none-linux-gnueabi/lib/libcrt*.a \
     ${arm_src}/arm-newlib/arm-none-linux-gnueabi/lib/intrinsics.o \
     ${PNACL_ARM_ROOT}
  ls -l ${PNACL_ARM_ROOT}

  Banner "x86-32 native code: ${PNACL_X8632_ROOT}"
  mkdir -p ${PNACL_X8632_ROOT}
  cp ${x86_src}/lib/gcc/nacl64/4.4.3/32/libgcc.a ${PNACL_X8632_ROOT}
  cp ${x86_src}/nacl64/lib/32/crt*.o ${PNACL_X8632_ROOT}
  cp ${x86_src}/nacl64/lib/32/libcrt*.a ${PNACL_X8632_ROOT}
  cp ${x86_src}/nacl64/lib/32/intrinsics.o ${PNACL_X8632_ROOT}
  ls -l ${PNACL_X8632_ROOT}

  Banner "x86-64 native code: ${PNACL_X8664_ROOT}"
  mkdir -p ${PNACL_X8664_ROOT}
  cp ${x86_src}/lib/gcc/nacl64/4.4.3/libgcc.a ${PNACL_X8664_ROOT}
  cp ${x86_src}/nacl64/lib/crt*.o ${PNACL_X8664_ROOT}
  cp ${x86_src}/nacl64/lib/libcrt*.a ${PNACL_X8664_ROOT}
  # NOTE: we do not yet have a this for x86-64
  #cp ${x86_src}/nacl64/lib/intrinsics.o ${X8664_ROOT}
  ls -l ${PNACL_X8664_ROOT}

  exit 0
fi

#@
#@ verify
#@
#@   verify files in platform directories

if [ ${MODE} = 'verify' ] ; then
  Banner "Verify ${PNACL_ARM_ROOT}"
  for i in ${PNACL_ARM_ROOT}/*.[oa] ; do
    VerifyObject 'elf32-little\(arm\)\?' $i  # objdumps vary in their output.
  done

  Banner "Verify ${PNACL_X8632_ROOT}"
  for i in ${PNACL_X8632_ROOT}/*.[oa] ; do
    VerifyObject elf32-i386  $i
  done

  Banner "Verify ${PNACL_X8664_ROOT}"
  for i in ${PNACL_X8664_ROOT}/*.[oa] ; do
    VerifyObject elf64-x86-64 $i
  done

  Banner "Verify ${PNACL_BITCODE_ROOT}"
  for i in ${PNACL_BITCODE_ROOT}/*.[oa] ; do
    VerifyLLVM $i
  done

  exit 0
fi

#@
#@ build-bitcode
#@
#@   build bitcode libs (newlib and extra sdk)
if [ ${MODE} = 'build-bitcode' ] ; then
  rm -rf ${PNACL_BITCODE_ROOT}
  mkdir -p ${PNACL_BITCODE_ROOT}

  export TARGET_CODE=bc-arm
  Banner "Newlib"
  tools/llvm/untrusted-toolchain-creator.sh newlib-libonly \
       $(pwd)/${PNACL_BITCODE_ROOT}

  Banner "Extra SDK"
  ./scons MODE=nacl_extra_sdk platform=arm sdl=none naclsdk_validate=0 \
      extra_sdk_clean \
      extra_sdk_update_header \
      install_libpthread \
      extra_sdk_update

  # NOTE: as collateral damage we also generate these as native code
  rm ${PNACL_BITCODE_ROOT}/crt*.o
  rm ${PNACL_BITCODE_ROOT}/intrinsics.o
  rm ${PNACL_BITCODE_ROOT}/libcrt_platform.a

  # TODO: cpp suppport is still broken so we fake the library
  exit 0
fi

#@
#@ build-bitcode-cpp
#@
#@   build bitcode for cpp - currently broken
if [ ${MODE} = 'build-bitcode-cpp' ] ; then
  Banner "C++"
  tools/llvm/untrusted-toolchain-creator.sh libstdcpp-libonly \
      ${PNACL_BITCODE_ROOT}

  Banner "Summary  ${PNACL_BITCODE_ROOT}"
  ls -l ${PNACL_BITCODE_ROOT}
  exit 0
fi

echo "ERROR: unknown mode ${MODE}"
exit -1
