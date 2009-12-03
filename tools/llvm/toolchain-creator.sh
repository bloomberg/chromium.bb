#!/bin/bash
# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
#@ This script build the arm SDK.
#@ It must be run from the native_client/ directory.

######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

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


SubBanner() {
  echo "......................................................................"
  echo $*
  echo "......................................................................"
}


Usage() {
  egrep "^#@" $0 | cut --bytes=3-
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

  if [[ ! -d ${INSTALL_ROOT} ]] ; then
     echo "ERROR: ${INSTALL_ROOT} does not exist"
    exit -1
  fi
}


# TODO(robertm): consider wiping all of ${BASE_DIR}
ClearInstallDir() {
  Banner "clearing dirs in ${INSTALL_ROOT}"
  rm -rf ${INSTALL_ROOT}/*
}


CreateTarBall() {
  local tarball=$1
  Banner "creating tar ball ${tarball}"
  tar cfz ${tarball} -C ${INSTALL_ROOT} .
}


# try to keep the tarball small
PruneDirs() {
  local CS=${INSTALL_ROOT}/codesourcery
  Banner "pruning tree"

  rm     ${INSTALL_ROOT}/arm-none-linux-gnueabi/llvm/lib/lib*.a
  rm -rf ${CS}/arm-2007q3/share
  # TODO(robertm): investigate these
  #  rm -rf ${CS}/arm-2007q3/arm-none-linux-gnueabi/lib
  #  rm -rf ${CS}/arm-2007q3/arm-none-linux-gnueabi/libc/armv4t
  rm -rf ${CS}/arm-2007q3/arm-none-linux-gnueabi/libc/thumb2
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
  Banner "Untar,Confiure,Build llvm/llvm-gcc ${LLVM_BUILD_SCRIPT}"
  export MAKE_OPTS="-j6 VERBOSE=1"
  export CC=$(readlink -f tools/llvm/mygcc32)
  export CXX=$(readlink -f tools/llvm/myg++32)

  nice ${LLVM_BUILD_SCRIPT}
}


# Build a sfi version of llvm's llc backend
# The mygcc32 and myg++32 trickery ensures that all binaries
# are statically linked and 32-bit.
UntarPatchConfigureAndBuildSfiLlc() {
  export MAKE_OPTS="-j6 VERBOSE=1"
  export CC=$(readlink -f tools/llvm/mygcc32)
  export CXX=$(readlink -f tools/llvm/myg++32)
  local patch=$(readlink -f tools/patches/llvm-r${LLVM_SVN_REV}.patch)
  local saved_dir=$(pwd)
  local tmpdir=/tmp/llvm.sfi
  Banner "Building sfi lcc in ${tmpdir}"
  rm -rf ${tmpdir}
  mkdir ${tmpdir}
  cd ${tmpdir}
  SubBanner "Untaring"
  tar jxf  ${LLVM_PKG_PATH}/llvm-${LLVM_SVN_REV}.tar.bz2
  cd llvm
  SubBanner "Patching"
  patch -p0 < ${patch}
  SubBanner "Configure"
   ./configure --disable-jit --enable-optimized --target=arm-none-linux-gnueabi
  SubBanner "Make"
  nice make ${MAKE_OPTS} tools-only
  SubBanner "Install"
  cp Release/bin/llc ${INSTALL_ROOT}/arm-none-linux-gnueabi/llvm/bin/llc-sfi
  cd ${saved_dir}
}


InstallTrustedLinkerScript() {
  local trusted_ld_script=${INSTALL_ROOT}/codesourcery/ld_script_arm_trusted
  local ld=${INSTALL_ROOT}/codesourcery/arm-2007q3/bin/arm-none-linux-gnueabi-ld
  # We are using the output of "ld --verbose" which contains
  # the linker script delimited by "=========".
  # We are changing the image start address to 70000000
  # to move the sel_ldr and other images "out of the way"
  Banner "installing trusted linker script to ${trusted_ld_script}"

  ${ld} --verbose |\
      grep -A 10000 "=======" |\
      grep -v "=======" |\
      sed -e 's/00008000/70000000/g' > ${trusted_ld_script}
}


# We need to adjust the start address and aligment of nacl arm modules
InstallUntrustedLinkerScript() {
   Banner "installing untrusted ld script"
   cp tools/llvm/ld_script_arm_untrusted ${INSTALL_ROOT}/arm-none-linux-gnueabi/
 }


# we copy some useful tools after building them first
InstallMiscTools() {
   Banner "building and installing misc tools"
   SubBanner "sel loader"
   # TODO(robertm): revisit some of these options
  ./scons MODE=nacl,opt-linux platform=arm dangerous_debug_disable_inner_sandbox=1 sdl_mode=none sdl=none naclsdk_mode=manual naclsdk_validate=0
   mkdir ${INSTALL_ROOT}/tools-arm
   cp scons-out/opt-linux-arm/obj/src/trusted/service_runtime/sel_ldr\
     ${INSTALL_ROOT}/tools-arm

   SubBanner "validator"
   ./scons MODE=opt-linux targetplatform=arm arm-ncval-core
   mkdir ${INSTALL_ROOT}/tools-x86
   cp scons-out/opt-linux-x86-32-to-arm/obj/src/trusted/validator_arm/v2/arm-ncval-core\
       ${INSTALL_ROOT}/tools-x86
}


# the driver is a simple python script which changes its behavior
# depending under the name it is invoked as
InstallDriver() {
  Banner "installing driver adaptors"
  cp tools/llvm/llvm-fake.py ${INSTALL_ROOT}/arm-none-linux-gnueabi/
  for s in gcc sfigcc g++ sfig++ ; do
    local t="llvm-fake-$s"
    echo "$t"
    ln -s llvm-fake.py ${INSTALL_ROOT}/arm-none-linux-gnueabi/$t
  done
}


#
InstallNewlibAndNaClRuntime() {
  Banner "building and installing nacl runtime"
  SubBanner "building newib"
   # TODO(robertm)
  SubBanner "building extra sdk libs"
  ./scons MODE=nacl_extra_sdk platform=arm sdl_mode=none sdl=none naclsdk_mode=manual naclsdk_validate=0 extra_sdk_clean extra_sdk_update_header install_libpthread extra_sdk_update
  cp -r src/third_party/nacl_sdk/arm-newlib ${INSTALL_ROOT}
}


InstallExamples() {
  Banner "installing examples into ${INSTALL_ROOT}/examples"
  cp -r  tools/llvm/arm_examples ${INSTALL_ROOT}/examples
}

######################################################################
# Main
######################################################################
if [ $# -eq 0 ] ; then
  echo "you must specify a mode on the commandline:"
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
#@ make_tarball <tarball>
#@
#@   create sdk tarball
if [ ${MODE} = 'make_tarball' ] ; then
  PathSanityCheck
  ClearInstallDir
  DownloadOrCopyCodeSourceryTarball
  ExtractLlvmBuildScript
  ConfigureAndBuildLlvm
  UntarPatchConfigureAndBuildSfiLlc
  # TODO(robertm): install missing libs + headers
  InstallTrustedLinkerScript
  InstallUntrustedLinkerScript
  InstallNewlibAndNaClRuntime
  InstallMiscTools
  InstallDriver
  InstallExamples
  PruneDirs
  # TODO(robertm): install qemu
  CreateTarBall $1
  exit 0
fi


#@
#@ llc-sfi
#@
#@   reinstall llc-sfi
if [ ${MODE} = 'llc-sfi' ] ; then
  UntarPatchConfigureAndBuildSfiLlc
  exit 0
fi
