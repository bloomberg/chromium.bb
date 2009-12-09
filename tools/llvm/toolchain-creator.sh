#!/bin/bash
# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
#@ This script build the arm untrusted SDK.
#@ NOTE: It must be run from the native_client/ directory.
#@ NOTE: you should also source: set_arm_(un)trusted_toolchain.sh
######################################################################
# Config
######################################################################

set -o nounset
set -o errexit


# NOTE: gcc and llvm have to be synchronized
#       we have chosen toolchains which both are based on gcc-4.2.1

readonly CS_URL=http://www.codesourcery.com/sgpp/lite/arm/portal/package1787/public/arm-none-linux-gnueabi/arm-2007q3-51-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2

export INSTALL_ROOT=/usr/local/crosstool-untrusted
export LLVM_PKG_PATH=$(readlink -f ../third_party/llvm)
export LLVM_SVN_REV=88663
export LLVMGCC_SVN_REV=88663

export TMP=/tmp/crosstool-untrusted
export CODE_SOURCERY_PKG_PATH=${INSTALL_ROOT}/codesourcery

readonly LLVM_BUILD_SCRIPT=${TMP}/build-install-linux.sh

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
  tar jcf ${tarball} -C ${INSTALL_ROOT} .
}


# try to keep the tarball small
PruneDirs() {
  local CS_ROOT=${INSTALL_ROOT}/codesourcery/arm-2007q3
  Banner "pruning code sourcery tree"
  SubBanner "Size before: $(du -msc  ${CS_ROOT})"
  rm -rf ${CS_ROOT}/share
  rm -rf ${CS_ROOT}/arm-none-linux-gnueabi/libc
  SubBanner "Size after: $(du -msc  ${CS_ROOT})"
}


DownloadOrCopyCodeSourceryTarball() {
  local tarball="${TMP}/${CS_URL##*/}"
  DownloadOrCopy ${CS_URL} ${tarball}
  ln -s ${tarball} ${CODE_SOURCERY_PKG_PATH}
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
  # TODO(robertm): add this
  SubBanner "building extra sdk libs"
  ./scons MODE=nacl_extra_sdk platform=arm sdl_mode=none sdl=none naclsdk_mode=manual naclsdk_validate=0 extra_sdk_clean extra_sdk_update_header install_libpthread extra_sdk_update
  cp -r src/third_party/nacl_sdk/arm-newlib ${INSTALL_ROOT}
}


InstallExamples() {
  Banner "installing examples into ${INSTALL_ROOT}/examples"
  cp -r  tools/llvm/arm_examples ${INSTALL_ROOT}/examples
}


BuildNewLib() {
  echo
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
#@ untrusted_sdk <tarball>
#@
#@   create untrusted sdk tarball
if [ ${MODE} = 'untrusted_sdk' ] ; then
  mkdir -p ${TMP}
  PathSanityCheck
  ClearInstallDir
  DownloadOrCopyCodeSourceryTarball
  ExtractLlvmBuildScript
  ConfigureAndBuildLlvm
  UntarPatchConfigureAndBuildSfiLlc
  InstallNewlibAndNaClRuntime
  InstallUntrustedLinkerScript
  InstallMiscTools
  InstallDriver
  # TODO(cbiffle): sandboxed libgcc build
  # FIXME(robertm)
  InstallExamples
  PruneDirs
  CreateTarBall $1
  exit 0
fi

#@
#@ download-cs
#@
#@   download codesourcery toolchain
if [ ${MODE} = 'download-cs' ] ; then
  DownloadOrCopyCodeSourceryTarball
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

#@
#@ llvm-gcc
#@
#@   install llvm-gcc toolchain
if [ ${MODE} = 'llvm-gcc' ] ; then
  ExtractLlvmBuildScript
  ConfigureAndBuildLlvm
  exit 0
fi

#@
#@ newlib-etc
#@
#@   install newlib-etc
if [ ${MODE} = 'newlib-etc' ] ; then
  InstallNewlibAndNaClRuntime
  exit 0
fi


echo "ERROR: unknown mode ${MODE}"
exit -1
