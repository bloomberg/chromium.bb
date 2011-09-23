#!/bin/bash
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
#@ This script builds and llvm/cs based cross toolchain for
#@ benchmarking puposes
#@ NOTE: It must be run from the native_client/ directory.

######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

# NOTE: gcc and llvm have to be synchronized
#       we have chosen toolchains which both are based on gcc-4.2.1

readonly INSTALL_ROOT=$(pwd)/toolchain/llvm-cs

readonly CROSS_TARGET=arm-none-linux-gnueabi
readonly LLVM_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}/llvm"
readonly LLVMGCC_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}/llvm-gcc-4.2"

readonly LLVMGCC_TARBALL=$(pwd)/../third_party/llvm/llvm-gcc-4.2-88663.tar.bz2
readonly LLVM_TARBALL=$(pwd)/../third_party/llvm/llvm-88663.tar.bz2

readonly MAKE_OPTS="-j8 VERBOSE=1"

readonly TMP=/tmp/llvm-cs

# These are simple compiler wrappers to force 32bit builds
readonly CC32=$(readlink -f tools/llvm/mygcc32)
readonly CXX32=$(readlink -f tools/llvm/myg++32)


readonly CODE_SOURCERY_ROOT=${INSTALL_ROOT}/codesourcery
readonly CROSS_TARGET_AS="${CODE_SOURCERY_ROOT}/arm-2007q3/bin/${CROSS_TARGET}-as"
readonly CROSS_TARGET_LD="${CODE_SOURCERY_ROOT}/arm-2007q3/bin/${CROSS_TARGET}-ld"

readonly SYSROOT="${CODE_SOURCERY_ROOT}/arm-2007q3/${CROSS_TARGET}/libc"

readonly CS_URL=http://www.codesourcery.com/sgpp/lite/arm/portal/package1787/public/arm-none-linux-gnueabi/arm-2007q3-51-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2

######################################################################
# Helper
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


Usage() {
  egrep "^#@" $0 | cut --bytes=3-
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


# Use this when not a lot of output is expected
Run() {
  local message=$1
  shift
  SubBanner "${message}"
  echo "COMMMAND: $@"
  "$@" || {
    echo
    Banner "ERROR: $@"
    exit -1
  }
}


RunWithLog() {
  local message=$1
  local log=$2
  shift 2
  SubBanner "${message}"
  echo "LOGFILE: ${log}"
  echo "PWD: $(pwd)"
  echo "COMMMAND: $@"
  "$@" > ${log} 2>&1 || {
    tail -1000 ${log}
    echo
    Banner "ERROR"
    echo "LOGFILE: ${log}"
    echo "PWD: $(pwd)"
    echo "COMMMAND: $@"
    exit -1
  }
}

######################################################################
#
######################################################################

# some sanity checks to make sure this script is run from the right place
PathSanityCheck() {
  if [[ $(basename $(pwd)) != "native_client" ]] ; then
    echo "ERROR: run this script from the native_client/ dir"
    exit -1
  fi

  if ! mkdir -p "${INSTALL_ROOT}" ; then
     echo "ERROR: ${INSTALL_ROOT} can't be created."
    exit -1
  fi
}


# TODO(robertm): consider wiping all of ${BASE_DIR}
ClearInstallDir() {
  Banner "clearing dirs in ${INSTALL_ROOT}"
  rm -rf ${INSTALL_ROOT}/*
}


DownloadOrCopyAndInstallSourcery() {
  local tarball="${TMP}/${CS_URL##*/}"
  DownloadOrCopy ${CS_URL} ${tarball}
  rm -rf ${CODE_SOURCERY_ROOT}
  mkdir -p ${CODE_SOURCERY_ROOT}
  tar jxf  ${tarball} -C  ${CODE_SOURCERY_ROOT}

  # Verify our CodeSourcery toolchain installation.
  if [[ ! -d "${SYSROOT}" ]]; then
    echo -n "Error: CodeSourcery does not contain libc for ${CROSS_TARGET}: "
    echo "${SYSROOT} not found."
    exit -1
  fi

  for tool in ${CROSS_TARGET_AS} ${CROSS_TARGET_LD}; do
    if [[ ! -e $tool ]]; then
      echo "${tool} not found; exiting."
      exit -1
    fi
  done
}



# Build basic llvm tools
ConfigureAndBuildLlvm() {
  local saved_dir=$(pwd)
  local tmpdir=${TMP}/llvm
  Banner "Building llvm in ${tmpdir}"
  rm -rf ${tmpdir}
  mkdir -p ${tmpdir}
  cd ${tmpdir}

  Run "Untaring" tar jxf ${LLVM_TARBALL}
  cd llvm

  RunWithLog "Configure" ${TMP}/llvm.configure.log\
      env -i PATH=/usr/bin/:/bin \
          CC=${CC32} \
          CXX=${CXX32} \
          ./configure \
          --disable-jit \
          --enable-optimized \
          --enable-targets=arm \
          --target=arm-none-linux-gnueabi \
          --prefix=${LLVM_INSTALL_DIR} \
          --with-llvmgccdir=${LLVMGCC_INSTALL_DIR}

  RunWithLog "Make" ${TMP}/llvm.make.log \
    env -i PATH=/usr/bin/:/bin \
           CC=${CC32} \
           CXX=${CXX32} \
           make ${MAKE_OPTS}

  RunWithLog "Installing LLVM" ${TMP}/llvm-install.log \
       make ${MAKE_OPTS} install

  cd ${saved_dir}
}


ConfigureAndBuildGcc() {
  local tmpdir=${TMP}/llvm-gcc
  local saved_dir=$(pwd)

  Banner "Building llvmgcc in ${tmpdir}"

  rm -rf ${tmpdir}
  mkdir -p ${tmpdir}
  cd ${tmpdir}

  Run "Untaring llvm-gcc" \
    tar jxf ${LLVMGCC_TARBALL}

  # NOTE: you cannot build llvm-gcc inside the source directory
  mkdir -p build
  cd build
  # TODO(robertm): do we really need CROSS_TARGET_*
  RunWithLog "Configure" ${TMP}/llvm-gcc.configure.log \
      env -i PATH=/usr/bin/:/bin \
          CC=${CC32} \
          CXX=${CXX32} \
          CFLAGS="-Dinhibit_libc" \
          CXXFLAGS="-Dinhibit_libc" \
          ../llvm-gcc-4.2/configure \
          --prefix=${LLVMGCC_INSTALL_DIR} \
          --enable-llvm=${LLVM_INSTALL_DIR} \
          --program-prefix=llvm- \
          --disable-libmudflap \
          --disable-decimal-float \
          --disable-libssp \
          --disable-libgomp \
          --enable-languages=c,c++ \
          --disable-threads \
          --disable-libstdcxx-pch \
          --disable-shared \
          --target=${CROSS_TARGET} \
          --with-arch=armv6 \
          --with-fpu=vfp \
          --with-as=${CROSS_TARGET_AS} \
          --with-ld=${CROSS_TARGET_LD} \
          --with-sysroot=${SYSROOT}

  RunWithLog "Make" ${TMP}/llvm-gcc.make.log \
      env -i PATH=/usr/bin/:/bin:${CODE_SOURCERY_ROOT}/arm-2007q3/bin \
          CC=${CC32} \
          CXX=${CXX32} \
          make \
          ${MAKE_OPTS}

  RunWithLog "Install" ${TMP}/llvm-gcc.install.log \
      env -i PATH=/usr/bin/:/bin:${CODE_SOURCERY_ROOT}/arm-2007q3/bin \
          make install

  cd ${saved_dir}
}



######################################################################
# Main
######################################################################
if [ $# -eq 0 ] ; then
  echo
  echo "ERROR: you must specify a mode on the commandline:"
  echo
  Usage
  exit -1
fi

MODE=$1
shift

#@
#@ help
#@
#@   print help for all modes
if [ $MODE} = 'help' ] ; then
  Usage
  exit 0
fi

#@
#@ toolchain
#@
#@    create an llvm/cs based toolchain
if [ ${MODE} = 'toolchain' ] ; then
  mkdir -p ${TMP}
  PathSanityCheck
  ClearInstallDir
  DownloadOrCopyAndInstallSourcery
  ConfigureAndBuildLlvm
  ConfigureAndBuildGcc
fi

echo "ERROR: unknown mode ${MODE}"
exit -1
