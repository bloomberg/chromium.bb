#!/bin/bash
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
#@ This script performs various helper tasks for pnacl.
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

readonly LLVM_DIS=$(readlink -f toolchain/linux_arm-untrusted/arm-none-linux-gnueabi/llvm/bin/llvm-dis)

readonly LLVM_AR=$(readlink -f toolchain/linux_arm-untrusted/arm-none-linux-gnueabi/llvm/bin/llvm-ar)
######################################################################
# Helpers
######################################################################

Banner() {
  echo "######################################################################"
  echo "$@"
  echo "######################################################################"
}

# Print the usage message to stdout.
Usage() {
  egrep "^#@" $0 | cut --bytes=3-
}

# Usage: VerifyObject <regexp> <filename>
# Ensure that the ELF "file format" string for <filename> matches <regexp>.
VerifyObject() {
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


# Usage: VerifyLlvmArchive <filename>
VerifyLlvmArchive() {
  echo "verify $1"
  saved=$(pwd)
  local archive=$1
  local tmp=/tmp/ar-verify
  rm -rf ${tmp}
  mkdir -p ${tmp}
  cp ${archive} ${tmp}
  cd ${tmp}
  ${LLVM_AR} x $(basename ${archive})
  for i in *.o ; do
    ${LLVM_DIS} $i -o $i.ll
  done

  if  grep asm *.ll ; then
    echo
    echo "ERROR"
    echo
    exit -1
  fi

  cd ${saved}
}

# Usage: VerifyLlvmObj <filename>
VerifyLlvmObj() {
  echo "verify $1"
  t=$(${LLVM_DIS} $1 -o -)

  if  grep asm <<<$t ; then
    echo
    echo "ERROR"
    echo
    exit -1
  fi
}
######################################################################
# Actions
######################################################################

#@
#@ help
#@
#@   Print help for all modes.
help() {
  Usage
}

#@
#@ download-toolchains
#@
#@   Download all the toolchains + SDKs needed by PNaCl.
download-toolchains() {
  ./scons platform=arm --download sdl=none
  ./scons platform=x86-64 --download sdl=none
}

#@
#@ organize-native-code
#@
#@   Saves the native code libraries (e.g. crt*.o) for each architecture
#@   into the toolchain/pnacl-untrusted/<arch> directories.
#@
#@   (You'll need to re-run this step after you modify the native code sources
#@   and re-run the "nacl_extra_sdk" build.)
organize-native-code() {
  rm -rf ${PNACL_TOOLCHAIN_ROOT}  # (a bit aggressive when making mods?)

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
  cp ${x86_src}/lib/gcc/nacl64/4.4.3/32/libgcc.a \
     ${x86_src}/nacl64/lib/32/crt*.o \
     ${x86_src}/nacl64/lib/32/libcrt*.a \
     ${x86_src}/nacl64/lib/32/intrinsics.o \
     ${PNACL_X8632_ROOT}
  ls -l ${PNACL_X8632_ROOT}

  Banner "x86-64 native code: ${PNACL_X8664_ROOT}"
  mkdir -p ${PNACL_X8664_ROOT}
  cp ${x86_src}/lib/gcc/nacl64/4.4.3/libgcc.a \
     ${x86_src}/nacl64/lib/crt*.o \
     ${x86_src}/nacl64/lib/libcrt*.a \
     ${PNACL_X8664_ROOT}
  # NOTE: we do not yet have a this for x86-64
  #cp ${x86_src}/nacl64/lib/intrinsics.o ${X8664_ROOT}
  ls -l ${PNACL_X8664_ROOT}
}

#@
#@ build-bitcode-cpp
#@
#@   Build bitcode libraries for C++.
build-bitcode-cpp() {
  Banner "C++"
  tools/llvm/untrusted-toolchain-creator.sh libstdcpp-libonly \
      ${PNACL_BITCODE_ROOT}

  Banner "Summary  ${PNACL_BITCODE_ROOT}"
  ls -l ${PNACL_BITCODE_ROOT}
}

#@
#@ build-bitcode-newlib
#@
#@   Build bitcode libraries for newlib
build-bitcode-newlib() {
  export TARGET_CODE=bc-arm
  Banner "Newlib"
  tools/llvm/untrusted-toolchain-creator.sh newlib-libonly \
       $(pwd)/${PNACL_BITCODE_ROOT}
}

#@
#@ build-bitcode-extra-sdk
#@
#@   Build bitcode libraries for extra sdk
build-bitcode-extra-sdk() {
  export TARGET_CODE=bc-arm

  Banner "Extra SDK"
  ./scons MODE=nacl_extra_sdk \
      platform=arm \
      sdl=none \
      naclsdk_validate=0 \
      disable_nosys_linker_warnings=1 \
      extra_sdk_clean \
      extra_sdk_update_header \
      install_libpthread \
      extra_sdk_update

  # NOTE: as collateral damage we also generate these as native code
  rm ${PNACL_BITCODE_ROOT}/crt*.o
  rm ${PNACL_BITCODE_ROOT}/intrinsics.o
  rm ${PNACL_BITCODE_ROOT}/libcrt_platform.a
}

#@
#@ build-bitcode
#@
#@   Builds bitcode libraries
build-bitcode() {
  rm -rf ${PNACL_BITCODE_ROOT}
  mkdir -p ${PNACL_BITCODE_ROOT}

  build-bitcode-newlib
  build-bitcode-extra-sdk
  build-bitcode-cpp
}

#@
#@ verify-llvm-archive <archive>
#@
#@   Verifies that a given archive is bitcode and free of ASMSs
verify-llvm-archive() {
  Banner "verify $1"
  VerifyLlvmArchive "$@"
}

#@
#@ verify-llvm-object <archive>
#@
#@   Verifies that a given .o file is bitcode and free of ASMSs
verify-llvm-object() {
  Banner "verify $1"
  VerifyLlvmObj "$@"
}

#@
#@ verify
#@
#@   Verifies that toolchain/pnacl-untrusted ELF files are of the correct
#@   architecture.
verify() {
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
  for i in ${PNACL_BITCODE_ROOT}/*.a ; do
    VerifyLlvmArchive $i
  done
  for i in ${PNACL_BITCODE_ROOT}/*.o ; do
    VerifyLlvmObj $i
  done
}

######################################################################
# Main

if [ $(basename $(pwd)) != "native_client" ]; then
  echo "You must run this script from the 'native_client' directory." >&2
  exit -1
fi

[ $# = 0 ] && set -- help  # Avoid reference to undefined $1.
if [ "$(type -t $1)" != "function" ]; then
  Usage
  echo "ERROR: unknown mode '$1'." >&2
  exit 1
fi

eval "$@"
