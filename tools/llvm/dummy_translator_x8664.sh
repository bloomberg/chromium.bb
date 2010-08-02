#!/bin/bash
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

set -o nounset
set -o errexit

readonly DEMO_ROOT=$(dirname $0)

# Executables
readonly LLC=${DEMO_ROOT}/llc
readonly AS=${DEMO_ROOT}/as
readonly LD=${DEMO_ROOT}/ld
readonly SEL_LDR=${DEMO_ROOT}/sel_ldr

# Flags
readonly LLC_X8664_FLAGS=(-march=x86-64
                          -mcpu=core2)

readonly AS_X8664_FLAGS=(--64 \
                         --nacl-align 5 \
                         -n \
                         -mtune=core2)

readonly LD_SCRIPT=${DEMO_ROOT}/ld_script

readonly LD_FLAGS=(--native-client \
                   -nostdlib \
                   -T ${LD_SCRIPT} \
                   -static)

readonly NATIVE_OBJS=(${DEMO_ROOT}/crt1.o \
                      ${DEMO_ROOT}/crti.o \
                      ${DEMO_ROOT}/intrinsics.o)

readonly NATIVE_LIBS=(${DEMO_ROOT}/libcrt_platform.a \
                      ${DEMO_ROOT}/crtn.o \
                      -L ${DEMO_ROOT} \
                      -lgcc)

# Main

# Avoid reference to undefined $1.
if [ $# != 1 ]; then
  echo "Usage: ./translator <bitcode file>"
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

echo "translating x8664 $1"

${LLC} ${LLC_X8664_FLAGS[@]} -f $1 -o asm_combined

${SEL_LDR} -d -- ${AS} ${AS_X8664_FLAGS[@]} asm_combined -o obj_combined

${LD} ${LD_FLAGS[@]} ${NATIVE_OBJS[@]} obj_combined ${NATIVE_LIBS[@]}

echo "translation complete"
