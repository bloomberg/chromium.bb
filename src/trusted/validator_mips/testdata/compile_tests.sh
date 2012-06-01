#!/bin/bash
# Copyright 2012 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

set -eu

# Get the path to the Mips cross-compiler.

dir=$(pwd)
cd ../../../..
topdir=$(pwd)
tools="$topdir/toolchain/pnacl_linux_x86_64/pkg/binutils/mipsel-pc-nacl/bin"
cd $dir

readonly MIPSEL_LD="$tools/ld"
readonly MIPSEL_AS="$tools/as"


for test_file in *.S ; do
  object_file=${test_file%.*}.o
  mipsel_nexe_file=${test_file%.*}.nexe
  special_link_options="--section-start .text=0xFFFA000"

  echo "compiling (MIPS32) $test_file -> $mipsel_nexe_file"
  ${MIPSEL_AS} -mips32r2 -EL -mdsp\
      $test_file -o $object_file
  if [ $test_file == "test_invalid_dest.S" ]
  then
     ${MIPSEL_LD} $special_link_options -static -nodefaultlibs -nostdlib \
        -m elf32ltsmip_nacl $object_file -o $mipsel_nexe_file
  else
     ${MIPSEL_LD} -static -nodefaultlibs -nostdlib \
        -m elf32ltsmip_nacl $object_file -o $mipsel_nexe_file
  fi
  rm $object_file
done

