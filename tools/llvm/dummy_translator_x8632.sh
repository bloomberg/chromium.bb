#!/bin/bash
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

set -o nounset
set -o errexit

readonly DEMO_ROOT=$(dirname $0)

readonly BIN_ROOT=${DEMO_ROOT}/bin
readonly LIB_ROOT=${DEMO_ROOT}/lib
readonly SCRIPT_ROOT=${DEMO_ROOT}/script

# Executables
readonly LLC=${BIN_ROOT}/llc
readonly AS=${BIN_ROOT}/as
readonly LD=${BIN_ROOT}/ld
readonly SEL_LDR=${BIN_ROOT}/sel_ldr

# Flags
readonly LLC_X8632_FLAGS=(-march=x86
                          -mcpu=pentium4)

readonly AS_X8632_FLAGS=(--32
                         --nacl-align 5
                         -n
                         -march=pentium4
                         -mtune=i386)

readonly LD_SCRIPT=${SCRIPT_ROOT}/ld_script

readonly LD_FLAGS=(--native-client \
                   -nostdlib \
                   -T ${LD_SCRIPT} \
                   -static)

readonly NATIVE_OBJS=(${LIB_ROOT}/crt1.o \
                      ${LIB_ROOT}/crti.o)

readonly NATIVE_LIBS=(${LIB_ROOT}/libcrt_platform.a \
                      ${LIB_ROOT}/crtn.o \
                      -L ${LIB_ROOT} \
                      -lgcc)

# Main

# Avoid reference to undefined $1.
if [ $# != 2 ]; then
  echo "Usage: ./translator <bitcode file> <nexe>"
  exit -1
fi

if [ ! -f $1 ] ; then
  echo "ERROR: $1 does not exist"
  exit -1
fi

if [ ! -f ${LLC} ] ; then
  echo "ERROR: there is no llc"
  exit -1
fi

if [ ! -f ${AS} ] ; then
  echo "ERROR: there is no as"
  exit -1
fi

if [ ! -f ${LD} ] ; then
  echo "ERROR: there is no ld"
  exit -1
fi

echo "translating x8632 $1"

${LLC} ${LLC_X8632_FLAGS[@]} $1 -o asm_combined

${SEL_LDR} -d -- ${AS} ${AS_X8632_FLAGS[@]} asm_combined -o obj_combined

${LD} ${LD_FLAGS[@]} ${NATIVE_OBJS[@]} obj_combined ${NATIVE_LIBS[@]} -o $2

echo "translation complete"
