#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script builds and runs the llvm test suite. It assumes that the source
# has been checked out using gclient (build.sh git-sync).
# Testsuite must be configured, then run, then report. Currently it
# requires clean in between runs of different arch. TODO(dschuff): fix that

set -o nounset
set -o errexit

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo 'ERROR: must be run in native_client/ directory!'
  echo "       (Current directory is $(pwd))"
  exit 1
fi

readonly NACL_ROOT="$(pwd)"
readonly LLVM_TESTSUITE_SRC=${NACL_ROOT}/pnacl/git/llvm-test-suite
readonly LLVM_TESTSUITE_BUILD=${NACL_ROOT}/pnacl/build/llvm-test-suite

readonly TC_SRC_LLVM=${NACL_ROOT}/pnacl/src/upstream/llvm
readonly TC_BUILD_LLVM=${NACL_ROOT}/pnacl/build/llvm
readonly PNACL_CONCURRENCY=${PNACL_CONCURRENCY:-6}
readonly PNACL_BIN=${NACL_ROOT}/toolchain/pnacl_linux_x86_64/newlib/bin
readonly PNACL_SCRIPTS=${NACL_ROOT}/pnacl/scripts
readonly PARSE_REPORT=${PNACL_SCRIPTS}/parse_llvm_testsuite_report.py

testsuite-prereq() {
  if [ $# -ne 1 ]; then
    echo "Please specify arch"
    exit 1
  fi
  ./scons platform=$1 irt_core sel_ldr
}

testsuite-run() {
  if [ $# -ne 1 ]; then
    echo "Please specify arch"
    exit 1
  fi
  local arch=$1
  mkdir -p ${LLVM_TESTSUITE_BUILD}
  pushd ${LLVM_TESTSUITE_BUILD}
  if [ ! -f Makefile ]; then
    testsuite-configure
  fi
  make -j${PNACL_CONCURRENCY} \
    PNACL_BIN=${PNACL_BIN} \
    PNACL_RUN=${NACL_ROOT}/run.py \
    PNACL_ARCH=${arch} \
    ENABLE_PARALLEL_REPORT=true \
    DISABLE_CBE=true \
    DISABLE_JIT=true \
    TEST=pnacl \
    report.csv
  mv report.pnacl.csv report.pnacl.${arch}.csv
  mv report.pnacl.raw.out report.pnacl.${arch}.raw.out
  popd
}

testsuite-configure() {
  mkdir -p ${LLVM_TESTSUITE_BUILD}
  pushd ${LLVM_TESTSUITE_BUILD}
  ${LLVM_TESTSUITE_SRC}/configure --with-llvmcc=clang \
    --with-clang=${PNACL_BIN}/pnacl-clang  --with-llvmsrc=${TC_SRC_LLVM} \
    --with-llvmobj=${TC_BUILD_LLVM}
  popd
}

testsuite-clean() {
  rm -rf ${LLVM_TESTSUITE_BUILD}
}

# parse the report output of the test, check against expected fails
testsuite-report() {
  if [ $# -lt 1 ]; then
    echo "Please specify arch"
    exit 1
  fi
  local arch=$1
  shift
  ${PARSE_REPORT} --exclude ${PNACL_SCRIPTS}/testsuite_known_failures_base.txt \
    --exclude ${PNACL_SCRIPTS}/testsuite_known_failures_pnacl.txt \
    --build-path ${LLVM_TESTSUITE_BUILD} \
    --attribute ${arch} \
    "$@" \
    ${LLVM_TESTSUITE_BUILD}/report.pnacl.${arch}.csv
}

all() {
  testsuite-prereq "$1"
  testsuite-clean
  testsuite-configure
  testsuite-run "$1"
  testsuite-report "$@"
}

help() {
  echo -n "Usage: $0 [testsuite-clean|testsuite-configure|testsuite-run <arch>"
  echo "|testsuite-report <arch>|all <arch>]"
  echo "Additional arguments to testsuite-report:"
  echo "-x|--exclude <file> (add an additional list of excluded tests)"
  echo "-c|--check-excludes (check for unnecessary excludes)"
  echo "-v|--verbose (print compilation/run logs of failing tests)"
}

[ $# = 0 ] && set -- help  # Avoid reference to undefined $1.
if [ "$(type -t $1)" != "function" ]; then
  echo "ERROR: unknown function '$1'." >&2
  help
  exit 1
fi

"$@"
