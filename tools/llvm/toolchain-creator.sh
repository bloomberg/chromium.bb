#!/bin/bash
# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
# This script must be run from the native_client/ directory.


readonly CS_TARBALL=arm-2007q3-51-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2
readonly LLVM_BUILD_SCRIPT=tools/llvm/build-install-linux.sh

CS_URL=http://www.codesourcery.com/sgpp/lite/arm/portal/package1787/public/arm-none-linux-gnueabi/${CS_TARBALL}


# TODO(robertm): temporarily use /usr/local/crosstool2 to not conflict
#                with the working toolchain in /usr/local/crosstool
export INSTALL_ROOT=/usr/local/crosstool2
export CODE_SOURCERY_PKG_PATH=${INSTALL_ROOT}
export LLVM_PKG_PATH=$(readlink -f ../third_party/llvm)
export LLVM_SVN_REV=88663
export LLVMGCC_SVN_REV=88663

######################################################################
# Helper
######################################################################

Banner() {
  echo "######################################################################"
  echo $*
  echo "######################################################################"
}


# Download the codesourcery tarball or use a local copy when available.
DownloadOrCopyCodeSourceryTarball() {
  if [[ -f "${CODE_SOURCERY_PKG_PATH}/${CS_TARBALL}" ]] ; then
     echo "code sourcery tarball already in place"
     exit 0
  fi
  Banner "downloading code sourcer tarball from ${CS_URL}"
  if [[ ${CS_TARBALL} ==  "http://*" ]] ; then
    wget ${CS_URL} -O ${CODE_SOURCERY_PKG_PATH}/${CS_TARBALL}
  else
    cp ${CS_URL} ${CODE_SOURCERY_PKG_PATH}/${CS_TARBALL}
  fi
}


# Extract the script to build llvm and llvm-gcc into *this* directory.
# We also patch it a little because we need to get rid of the sudo stuff
# to enable automatic builds.
ExtractLlvmBuildScript() {
  Banner "Extracting ${LLVM_BUILD_SCRIPT}"
  local in_archive_path=llvm/utils/crosstool/ARM/build-install-linux.sh
  tar jxf ${LLVM_PKG_PATH}/llvm-88663.tar.bz2 ${in_archive_path} -O \
      | sed -e 's/sudo//' > ${LLVM_BUILD_SCRIPT}
  chmod a+rx ${LLVM_BUILD_SCRIPT}
}


# Run the script extracted by ExtractLlvmBuildScript().
# The mygcc32 and myg++32 trickery ensures that all binaries
# are statically linked and 32-bit.
ConfigureAndBuildLlvm() {
  Banner "Running ${LLVM_BUILD_SCRIPT}"

  export MAKE_OPTS="-j6 VERBOSE=1"
  export CC=$(readlink -f tools/llvm/mygcc32)
  export CXX=$(readlink -f tools/llvm/myg++32)

  nice ${LLVM_BUILD_SCRIPT}
}

######################################################################
# Main
######################################################################

if [[ ! -d ${INSTALL_ROOT} ]]; then
  echo "ERROR: you must create the dir ${INSTALL_ROOT}"
  exit
fi

DownloadOrCopyCodeSourceryTarball
ExtractLlvmBuildScript
ConfigureAndBuildLlvm
