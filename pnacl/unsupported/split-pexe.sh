#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
set -o nounset
set -o errexit

# TODO(pnacl-team): We have to not hard-code this.
readonly EXTRACT=toolchain/pnacl_linux_x86/host_x86_32/bin/llvm-extract
readonly TRANSLATE=toolchain/pnacl_linux_x86/bin/pnacl-translate

split-pexe() {
  local out=$1
  local in=$2
  local rem=$3
  local div=$4

  ${EXTRACT} -remainder ${rem} -divisor ${div} ${in} -o ${out}
}


translate-split-pexe() {
  local arch=$1
  local out=$2
  local in=$3

  ${TRANSLATE} -c -arch ${arch} ${in} -o ${out} --pnacl-driver-verbose
}


link-split-objects() {
    local arch=$1
    local out=$2
    shift 2
    ${TRANSLATE} -arch ${arch} -o ${out} "$@" --pnacl-driver-verbose
}


split-translate-link() {
  local arch=$1
  local out=$2
  local in=$3
  local n=$4

  for i in $(seq 0 $(($n - 1))) ; do
    echo "splitting $i/$n"
    split-pexe  ${in}.$i ${in} $i $n
  done

  # TODO(robertm): these could be parallelized
  local objs=""
  for i in $(seq 0 $(($n - 1))) ; do
    echo "translating  $i/$n"
    objs+=" ${out}.$i"
    translate-split-pexe ${arch} ${out}.$i  ${in}.$i
  done

  echo "linking ${objs}"
  link-split-objects ${arch} ${out} ${objs}
}

"$@"
