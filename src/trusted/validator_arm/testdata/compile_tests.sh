#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -eu

# Get the path to the ARM cross-compiler.
dir=$(pwd)
cd ../../../..
topdir=$(pwd)
tools="$topdir/toolchain/linux_arm_newlib/bin"
cd $dir

readonly ARM_LD="$tools/arm-nacl-ld"
readonly ARM_AS="$tools/arm-nacl-as"

for test_file in *.S ; do
  object_file=${test_file%.*}.o
  nexe_file=${test_file%.*}.nexe
  pre_file=${test_file%.*}.s

  echo "compiling $test_file -> $nexe_file"
  cpp $test_file -o $pre_file
  ${ARM_AS} -march=armv7-a -mcpu=cortex-a8 -mfpu=neon -c $pre_file \
    -o $object_file
  ${ARM_LD} -static -nodefaultlibs -nostdlib \
      $object_file -o $nexe_file
  rm $pre_file $object_file
done
