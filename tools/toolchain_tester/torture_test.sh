#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

######################################################################
######################################################################
#
#                               < CONFIG >
#
######################################################################
######################################################################


######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

readonly TEST_ROOT=/tmp/nacl_compiler_test
readonly TEST_TARBALL_URL="http://gcc.petsads.us/releases/gcc-4.6.1/gcc-testsuite-4.6.1.tar.bz2"

readonly TEST_PATH=${TEST_ROOT}/gcc-4.6.1/gcc/testsuite/gcc.c-torture/execute
######################################################################
######################################################################
#
#                               < HELPERS >
#
######################################################################
######################################################################


Banner() {
  echo "######################################################################"
  echo "$@"
  echo "######################################################################"
}


SubBanner() {
  echo "......................................................................"
  echo "$@"
  echo "......................................................................"
}


DownloadOrCopy() {
   if [[ -f "$2" ]] ; then
     echo "$2 already in place"
   elif [[ $1 =~  'http://' ]] ; then
     SubBanner "downloading from $1"
     wget $1 -O $2
   else
     SubBanner "copying from $1"
     cp $1 $2
   fi
 }

######################################################################
######################################################################
#
#                               < ACTIONS >
#
######################################################################
######################################################################


#@ help                  - Usage information.
help() {
  egrep "^#@" $0 | cut --bytes=3-
}


#@ install-tests         - Download test tarball
install-tests() {
  mkdir -p ${TEST_ROOT}
  DownloadOrCopy ${TEST_TARBALL_URL} ${TEST_ROOT}/test_tarball.tgz
  tar jxf ${TEST_ROOT}/test_tarball.tgz -C ${TEST_ROOT}
}

#@ clean                 - remove all tests
clean() {
  rm -rf  ${TEST_ROOT}
}

#@
#@ pnacl-x8632-O0-torture
#@
pnacl-x8632-O0-torture() {
  ./scons platform=x86-32 irt_core sel_ldr
  tools/toolchain_tester/toolchain_tester.py  \
      --exclude=tools/toolchain_tester/known_failures_pnacl.txt \
      --config=llvm_pnacl_x8632_O0 \
      "$@" \
      ${TEST_PATH}/*c ${TEST_PATH}/ieee/*c
}

#@
#@ pnacl-x8664-O0-torture
#@
pnacl-x8664-O0-torture() {
  ./scons platform=x86-64 irt_core sel_ldr
  tools/toolchain_tester/toolchain_tester.py  \
      --exclude=tools/toolchain_tester/known_failures_pnacl.txt \
      --config=llvm_pnacl_x8664_O0 \
      "$@" \
      ${TEST_PATH}/*c ${TEST_PATH}/ieee/*c
}

#@
#@ pnacl-arm-O0-torture
#@
pnacl-arm-O0-torture() {
  ./scons platform=arm irt_core sel_ldr
  tools/toolchain_tester/toolchain_tester.py  \
      --exclude=tools/toolchain_tester/known_failures_pnacl.txt \
      --config=llvm_pnacl_arm_O0 \
      "$@" \
      ${TEST_PATH}/*c ${TEST_PATH}/ieee/*c
}

#@
#@ naclgcc-x8632-O0-torture
#@
naclgcc-x8632-O0-torture() {
  # NOTE: we force the builing of crtX.o via run_intrinsics_test
  ./scons platform=x86-32 irt_core sel_ldr run_intrinsics_test
  tools/toolchain_tester/toolchain_tester.py  \
      --exclude=tools/toolchain_tester/known_failures_naclgcc.txt \
      --config=nacl_gcc_x8632_O0 \
      "$@" \
      ${TEST_PATH}/*c ${TEST_PATH}/ieee/*c
}

#@
#@ naclgcc-x8664-O0-torture
#@
naclgcc-x8664-O0-torture() {
  # NOTE: we force the builing of crtX.o via run_intrinsics_test
  ./scons platform=x86-64 irt_core sel_ldr run_intrinsics_test
  tools/toolchain_tester/toolchain_tester.py  \
      --exclude=tools/toolchain_tester/known_failures_naclgcc.txt \
      --config=nacl_gcc_x8664_O0 \
      "$@" \
      ${TEST_PATH}/*c ${TEST_PATH}/ieee/*c
}

#@
#@ localgcc-x8632-O0-torture
#@
localgcc-x8632-O0-torture() {
  tools/toolchain_tester/toolchain_tester.py  \
      --config=local_gcc_x8632_O0 \
      --exclude=tools/toolchain_tester/known_failures_localgcc.txt
      "$@" \
      ${TEST_PATH}/*c ${TEST_PATH}/ieee/*c
}

#@
#@ localgcc-x8664-O0-torture
#@
localgcc-x8664-O0-torture() {
  tools/toolchain_tester/toolchain_tester.py  \
      --config=local_gcc_x8664_O0 \
      --exclude=tools/toolchain_tester/known_failures_localgcc.txt
      "$@" \
      ${TEST_PATH}/*c ${TEST_PATH}/ieee/*c
}

######################################################################
######################################################################
#
#                               < MAIN >
#
######################################################################
######################################################################

if [ $# = 0 ]; then set -- help; fi  # Avoid reference to undefined $1.

if [ "$(type -t $1)" != "function" ]; then
  #Usage
  echo "ERROR: unknown function '$1'." >&2
  echo "For help, try:"
  echo "    $0 help"
  exit 1
fi

"$@"
