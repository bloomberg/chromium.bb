# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
# This file is sourced from setup_arm_untrusted_toolchain.sh and
# setup_arm_newlib.sh and defines the minimal set of needed constants:
#
# Uses LLVM toolchain in both "bitcode" and "sfi" modes of
# TARGET_CODE.
#
# CS_ROOT - CodeSourcery installation.
# {CC, CXX, AR, NM, RANLIB, LD, ASCOM}_FOR_TARGET - locations of tools
# LLVM_BIN_PATH - LLVM installation.
# ILLEGAL_TOOL - A tool that should never be invoked.  Used for assertions.

CS_ROOT="$(pwd)/compiler/linux_arm-untrusted/codesourcery/arm-2007q3"
LLVM_BIN_PATH="$(pwd)/compiler/linux_arm-untrusted/arm-none-linux-gnueabi"
ILLEGAL_TOOL="${LLVM_BIN_PATH}/llvm-fake-illegal"

# Define TARGET_CODE=<value> in the calling environment to override.
case ${TARGET_CODE:=sfi} in
  sfi)  # => Libraries with Native Client SFI sandboxing.
    CC_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-sfigcc"
    CXX_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-sfig++"
    AR_FOR_TARGET="${CS_ROOT}/arm-none-linux-gnueabi/bin/ar"
    NM_FOR_TARGET="${CS_ROOT}/arm-none-linux-gnueabi/bin/nm"
    RANLIB_FOR_TARGET="${CS_ROOT}/arm-none-linux-gnueabi/bin/ranlib"
    CCAS_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-cppas-arm"
    LD_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-sfild"
    ;;
  regular)  # => Libraries without sandboxing.
    CC_FOR_TARGET="${CS_ROOT}/arm-none-linux-gnueabi/bin/gcc"
    CXX_FOR_TARGET="${CS_ROOT}/arm-none-linux-gnueabi/bin/g++"
    AR_FOR_TARGET="${CS_ROOT}/arm-none-linux-gnueabi/bin/ar"
    NM_FOR_TARGET="${CS_ROOT}/arm-none-linux-gnueabi/bin/nm"
    RANLIB_FOR_TARGET="${CS_ROOT}/arm-none-linux-gnueabi/bin/ranlib"
    CCAS_FOR_TARGET="${CS_ROOT}/arm-none-linux-gnueabi/bin/gcc"
    LD_FOR_TARGET="${CS_ROOT}/arm-none-linux-gnueabi/bin/ld"
    ;;
  bc-arm)  # => Bitcode libraries => arm
    CC_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-bcgcc"
    CXX_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-bcg++"
    AR_FOR_TARGET="${LLVM_BIN_PATH}/llvm/bin/llvm-ar"
    NM_FOR_TARGET="${LLVM_BIN_PATH}/llvm/bin/llvm-nm"
    RANLIB_FOR_TARGET="${LLVM_BIN_PATH}/llvm/bin/llvm-ranlib"
    CCAS_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-cppas-arm"
    LD_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-bcld-arm"
    ;;
  bc-x86-32)  # => Bitcode libraries => x8632
    CC_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-bcgcc"
    CXX_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-bcg++"
    AR_FOR_TARGET="${LLVM_BIN_PATH}/llvm/bin/llvm-ar"
    NM_FOR_TARGET="${LLVM_BIN_PATH}/llvm/bin/llvm-nm"
    RANLIB_FOR_TARGET="${LLVM_BIN_PATH}/llvm/bin/llvm-ranlib"
    CCAS_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-cppas-x86-32"
    LD_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-bcld-x86-32"
    ;;
  *)
    echo "Unknown TARGET_CODE value '${TARGET_CODE}';" \
         "(expected one of: sfi, regular, bc-arm, bcx86-32)." >&2
    return 1
    ;;
esac
