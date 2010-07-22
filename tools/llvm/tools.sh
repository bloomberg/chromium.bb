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
# BINUTILS_ROOT - binutils installation.
# {CC, CXX, AR, NM, RANLIB, LD, ASCOM}_FOR_TARGET - locations of tools
# LLVM_BIN_PATH - LLVM installation.
# ILLEGAL_TOOL - A tool that should never be invoked.  Used for assertions.

BINUTILS_ROOT="$(pwd)/toolchain/linux_arm-untrusted/arm-none-linux-gnueabi/llvm-gcc-4.2"
LLVM_BIN_PATH="$(pwd)/toolchain/linux_arm-untrusted/arm-none-linux-gnueabi"
LLVM_DRIVER_PATH="$(pwd)/toolchain/linux_arm-untrusted/arm-none-linux-gnueabi"

# Define TARGET_CODE=<value> in the calling environment to override.
case ${TARGET_CODE:=bc-arm} in
  sfi)  # => Libraries with Native Client SFI sandboxing.
    CC_FOR_TARGET="${LLVM_DRIVER_PATH}/llvm-fake-sfigcc"
    CXX_FOR_TARGET="${LLVM_DRIVER_PATH}/llvm-fake-sfig++"
    AR_FOR_TARGET="${BINUTILS_ROOT}/bin/arm-none-linux-gnueabi-ar"
    NM_FOR_TARGET="${BINUTILS_ROOT}/bin/arm-none-linux-gnueabi-nm"
    RANLIB_FOR_TARGET="${BINUTILS_ROOT}/bin/arm-none-linux-gnueabi-ranlib"
    CC_NATIVE_FOR_TARGET="${LLVM_DRIVER_PATH}/llvm-fake-sfigcc"
    LD_FOR_TARGET="${LLVM_DRIVER_PATH}/llvm-fake-sfild"
    ;;
  bc-arm)  # => Bitcode libraries => arm
    CC_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-sfigcc"
    CXX_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-sfig++"
    AR_FOR_TARGET="${BINUTILS_ROOT}/bin/arm-none-linux-gnueabi-ar"
    NM_FOR_TARGET="${BINUTILS_ROOT}/bin/arm-none-linux-gnueabi-nm"
    RANLIB_FOR_TARGET="${BINUTILS_ROOT}/bin/arm-none-linux-gnueabi-ranlib"
    CC_NATIVE_FOR_TARGET="${LLVM_DRIVER_PATH}/llvm-fake-sfigcc"
    LD_FOR_TARGET="${LLVM_DRIVER_PATH}/llvm-fake-bcld -arch arm"
    ;;
  bc-x86-32)  # => Bitcode libraries => x86-32
    CC_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-sfigcc"
    CXX_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-sfig++"
    AR_FOR_TARGET="${BINUTILS_ROOT}/bin/arm-none-linux-gnueabi-ar"
    NM_FOR_TARGET="${BINUTILS_ROOT}/bin/arm-none-linux-gnueabi-nm"
    RANLIB_FOR_TARGET="${BINUTILS_ROOT}/bin/arm-none-linux-gnueabi-ranlib"
    CC_NATIVE_FOR_TARGET="${LLVM_DRIVER_PATH}/llvm-fake-sfigcc"
    LD_FOR_TARGET="${LLVM_DRIVER_PATH}/llvm-fake-bcld -arch x86-32"
    ;;
  bc-x86-64)  # => Bitcode libraries => x86-64
    CC_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-sfigcc"
    CXX_FOR_TARGET="${LLVM_BIN_PATH}/llvm-fake-sfig++"
    AR_FOR_TARGET="${BINUTILS_ROOT}/bin/arm-none-linux-gnueabi-ar"
    NM_FOR_TARGET="${BINUTILS_ROOT}/bin/arm-none-linux-gnueabi-nm"
    RANLIB_FOR_TARGET="${BINUTILS_ROOT}/bin/arm-none-linux-gnueabi-ranlib"
    CC_NATIVE_FOR_TARGET="${LLVM_DRIVER_PATH}/llvm-fake-sfigcc"
    LD_FOR_TARGET="${LLVM_DRIVER_PATH}/llvm-fake-bcld -arch x86-64"
    ;;
  *)
    echo "Unknown TARGET_CODE value '${TARGET_CODE}';" \
         "(expected one of: sfi, regular, bc-arm, bcx86-32)." >&2
    return 1
    ;;
esac
