#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -eu

# Get the path to the ARM cross-compiler.
dir=$(pwd)
cd ../../../..
topdir=$(pwd)
# TODO(pnacl-team): We have to not hard-code this.
tools="$topdir/toolchain/pnacl_linux_x86_64/host/bin"
cd $dir

ldscript=ld_script_arm_thumb2_untrusted
readonly ARM_LD="$tools/arm-pc-nacl-ld"
readonly ARM_AS="$tools/arm-pc-nacl-as"

for test_file in *.S ; do
  object_file=${test_file%.*}.o
  nexe_file=${test_file%.*}.nexe
  pre_file=${test_file%.*}.s

  echo "compiling $test_file -> $nexe_file"
  cpp $test_file -o $pre_file
  ${ARM_AS} -march=armv7-a -mcpu=cortex-a8 -mfpu=neon -mthumb -c $pre_file \
    -o $object_file
  ${ARM_LD} -static -nodefaultlibs -nostdlib -T $ldscript \
      $object_file -o $nexe_file
  rm $pre_file $object_file
done
