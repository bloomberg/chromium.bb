#!/bin/bash
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

set -o nounset
set -o errexit

# 32-bit sandboxed translators root
readonly SB_ROOT=$(dirname $0)

readonly BIN_ROOT=${SB_ROOT}/bin
readonly LIB_ROOT=${SB_ROOT}/../../libs-x8632
readonly SCRIPT_ROOT=${SB_ROOT}/script

# Executables
readonly LLC=${BIN_ROOT}/llc
readonly AS=${BIN_ROOT}/as
readonly LD=${BIN_ROOT}/ld
readonly SEL_LDR=${BIN_ROOT}/sel_ldr

# Flags
readonly LLC_FLAGS=(-march=x86
                    -mcpu=pentium4)

readonly AS_FLAGS=(--32
                   --nacl-align 5
                   -n
                   -march=pentium4
                   -mtune=i386)

readonly LD_SCRIPT=${SCRIPT_ROOT}/ld_script

readonly LD_FLAGS=(-nostdlib \
                   -T ${LD_SCRIPT} \
                   -static)

readonly NATIVE_OBJS=(${LIB_ROOT}/crt1.o \
                      ${LIB_ROOT}/crti.o \
                      ${LIB_ROOT}/crtbegin.o)

readonly NATIVE_LIBS=(${LIB_ROOT}/libcrt_platform.a \
                      ${LIB_ROOT}/crtend.o \
                      ${LIB_ROOT}/crtn.o \
                      -L ${LIB_ROOT} \
                      -lgcc_eh \
                      -lgcc)

# Main

# Avoid reference to undefined $1.
if [ $# != 2 ]; then
  echo "Usage: ./translator <bitcode file> <nexe>"
  exit -1
fi

for x in "$1" "${LLC}" "${AS}" "${LD}"; do
  if [ ! -f $x ] ; then
    echo "ERROR: $x does not exist"
    exit -1
  fi
done

echo "translating x8632 $1"

${SEL_LDR} -a -- ${LLC} ${LLC_FLAGS[@]} $1 -o asm_combined

${SEL_LDR} -a -- ${AS} ${AS_FLAGS[@]} asm_combined -o obj_combined

${SEL_LDR} -a -- ${LD} ${LD_FLAGS[@]} ${NATIVE_OBJS[@]} obj_combined -o $2 \
     ${NATIVE_LIBS[@]}

echo "translation complete"
