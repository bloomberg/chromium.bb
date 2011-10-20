#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -eu

# Get the path to the ARM cross-compiler.
# We use the trusted compiler because llvm-fake.py (used in the
# untrusted compiler) doesn't support -nodefaultlibs.
dir=$(pwd)
cd ../../../..
eval "$(tools/llvm/setup_arm_trusted_toolchain.py)"
topdir=$(pwd)
cd $dir

ldscript=$topdir/toolchain/pnacl_linux_x86_64/ldscripts/ld_script_arm_untrusted

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
