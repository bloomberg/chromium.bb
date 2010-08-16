#!/bin/bash
# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
# Copyright 2009 Google Inc.

set -eu

# Get the path to the ARM cross-compiler.
# We use the trusted compiler because llvm-wrapper.py (used in the
# untrusted compiler) doesn't support -nodefaultlibs.
dir=$(pwd)
cd ../../../..
source tools/llvm/setup_arm_trusted_toolchain.sh
topdir=$(pwd)
cd $dir

ldscript=$topdir/toolchain/linux_arm-untrusted/arm-none-linux-gnueabi/ld_script_arm_untrusted

readonly ARM_CROSS_COMPILER="${ARM_CC}"
for test_file in *.S ; do
  object_file=${test_file%.*}.o
  nexe_file=${test_file%.*}.nexe

  echo "compiling $test_file -> $nexe_file"
  ${ARM_CROSS_COMPILER} \
      -march=armv7-a -mcpu=cortex-a8 -mfpu=neon -c $test_file -o $object_file
  ${ARM_LD} -static -nodefaultlibs -nostdlib -T $ldscript \
      $object_file -o $nexe_file
done
