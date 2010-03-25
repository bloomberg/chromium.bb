#!/bin/bash
#
# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
# Copyright 2009, Google Inc.

set -o nounset
set -o errexit

#@ various commands to emulate arm code using qemu

# From a qemu build based on qemu-0.10.1.tar.gz
readonly SDK_ROOT=$(pwd)/compiler/linux_arm-trusted
readonly QEMU=${SDK_ROOT}/qemu-arm
readonly QEMU_JAIL=${SDK_ROOT}/arm-2009q3/arm-none-linux-gnueabi/libc
# NOTE: some useful debugging options for qemu:
#       env vars:
#                  QEMU_STRACE=1
#       args:
#                  -strace
#                  -d out_asm,in_asm,op,int,exec,cpu
#       c.f.  cpu_log_items in qemu-XXX/exec.c
readonly QEMU_ARGS="-cpu cortex-a8"
readonly QEMU_ARGS_DEBUG="-d in_asm,op,int,exec,cpu"

######################################################################
# Helpers
######################################################################

Banner() {
  echo "######################################################################"
  echo $*
  echo "######################################################################"
}

Usage() {
  egrep "^#@" $0 | cut --bytes=3-
}


CheckPrerequisites () {
  if [[ ! -d ${QEMU_JAIL} ]] ; then
    echo "ERROR:  no proper root-jail directory found"
    exit -1
  fi

  local MMAP_LIMIT=$(cat /proc/sys/vm/mmap_min_addr)
  if [ ${MMAP_LIMIT} -ge 32000 ] ; then
    # c.f. http://www.crashcourse.ca/wiki/index.php/QEMU_on_Fedora_11
    echo "ERROR:"
    Banner "You need to run 'echo 16384 > /proc/sys/vm/mmap_min_addr' as root"
    exit -1
  fi
}

######################################################################
if [[ $# -eq 0 ]] ; then
     echo "you must specify a mode on the commandline:"
     exit -1
fi

MODE=$1
shift

#@
#@ help
#@
#@   print help for all modes
if [[ ${MODE} = 'help' ]] ; then
  Usage
  exit 0
fi


#@
#@ run
#@
#@   run stuff
if [[ ${MODE} = 'run' ]] ; then
  CheckPrerequisites
  exec ${QEMU} -L ${QEMU_JAIL} ${QEMU_ARGS} $*
fi

#@
#@ run_debug
#@
#@   run stuff but also generate trace in /tmp
if [[ ${MODE} = 'run' ]] ; then
  CheckPrerequisites
  exec ${QEMU} -L ${QEMU_JAIL} ${QEMU_ARGS} ${QEMU_ARGS_DEBUG} $*
fi

######################################################################
echo "unknown mode: ${MODE}"
exit -1

