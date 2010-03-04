#!/bin/bash
# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
#@ This script builds the arm untrusted SDK.
#@ NOTE: It must be run from the native_client/ directory.
#@ NOTE: you should source: set_arm_(un)trusted_toolchain.sh
#@       before running it
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
  echo "COMMMAND: $@"
  "$@" > ${log} 2>&1 || {
    cat ${log}
    echo
    Banner "ERROR"
    echo "LOGFILE: ${log}"
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
  Banner "pruning code sourcery tree"
  local CS_ROOT=${INSTALL_ROOT}/codesourcery/arm-2007q3
  SubBanner "Size before: $(du -msc  ${CS_ROOT})"
  rm -rf ${CS_ROOT}/share
  rm -rf ${CS_ROOT}/arm-none-linux-gnueabi/libc
  SubBanner "Size after: $(du -msc  ${CS_ROOT})"

  Banner "pruning llvm sourcery tree"
  local LLVM_ROOT=${INSTALL_ROOT}/arm-none-linux-gnueabi
  SubBanner "Size before: $(du -msc  ${LLVM_ROOT})"
  rm  ${LLVM_ROOT}/llvm/lib/lib*.a
  SubBanner "Size after: $(du -msc  ${LLVM_ROOT})"
}


DownloadOrCopyCodeSourceryTarball() {
  local tarball="${TMP}/${CS_URL##*/}"
  DownloadOrCopy ${CS_URL} ${tarball}
  mkdir -p ${CODE_SOURCERY_PKG_PATH}
  ln -s ${tarball} ${CODE_SOURCERY_PKG_PATH}
}


# Run the script extracted by ExtractLlvmBuildScript().
# The mygcc32 and myg++32 trickery ensures that all binaries
# are statically linked and 32-bit.
ConfigureAndBuildLlvm() {
  Banner "Untar,Confiure,Build llvm/llvm-gcc ${LLVM_BUILD_SCRIPT}"
  export MAKE_OPTS="-j6 VERBOSE=1"
  export CC=$(readlink -f tools/llvm/mygcc32)
  export CXX=$(readlink -f tools/llvm/myg++32)

  tools/llvm/build-phase1-llvmgcc.sh
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

  Run "Untaring" tar jxf  ${LLVM_PKG_PATH}/llvm-${LLVM_SVN_REV}.tar.bz2
  cd llvm

  Run "Patching" patch -p0 < ${patch}

  RunWithLog "Configure" /tmp/llvm.sfi/llvm.sfi.configure.log\
      ./configure\
      --disable-jit\
      --enable-optimized\
      --enable-targets=arm\
      --target=arm-none-linux-gnueabi

  RunWithLog "Make" /tmp/llvm.sfi/llvm.sfi.make.log\
      make ${MAKE_OPTS} tools-only

  SubBanner "Install"
  cp Release/bin/llc ${INSTALL_ROOT}/arm-none-linux-gnueabi/llvm/bin/llc-sfi
  cd ${saved_dir}
}


# We need to adjust the start address and aligment of nacl arm modules
InstallUntrustedLinkerScript() {
   Banner "installing untrusted ld script"
   cp tools/llvm/ld_script_arm_untrusted ${INSTALL_ROOT}/arm-none-linux-gnueabi/
}


# Run a modified version of the script used by ConfigureAndBuildLlvm to build
# it all a second time, with some patches, using the newly-installed driver
# script to produce SFI libs.
InstallSecondPhaseLlvmGccLibs() {
  Banner "Untar,Configure,Build llvm/llvm-gcc phase2"
  export MAKE_OPTS="-j6 VERBOSE=1"
  export CC=$(readlink -f tools/llvm/mygcc32)
  export CXX=$(readlink -f tools/llvm/myg++32)

  tools/llvm/build-phase2-llvmgcc.sh
}


# we copy some useful tools after building them first
InstallMiscTools() {
   Banner "building and installing misc tools"

   # TODO(robertm): revisit some of these options
   Run "sel loader" \
          ./scons MODE=nacl,opt-linux \
          platform=arm \
          sdl_mode=none \
          sdl=none \
          naclsdk_mode=manual \
          naclsdk_validate=0 \
          sysinfo= \
          sel_ldr
   rm -rf  ${INSTALL_ROOT}/tools-arm
   mkdir ${INSTALL_ROOT}/tools-arm
   cp scons-out/opt-linux-arm/obj/src/trusted/service_runtime/sel_ldr\
     ${INSTALL_ROOT}/tools-arm

   Run "validator" \
           ./scons MODE=opt-linux \
           naclsdk_mode=manual \
           targetplatform=arm \
           sysinfo= \
           arm-ncval-core
   rm -rf  ${INSTALL_ROOT}/tools-x86
   mkdir ${INSTALL_ROOT}/tools-x86
   cp scons-out/opt-linux-x86-32-to-arm/obj/src/trusted/validator_arm/v2/arm-ncval-core\
       ${INSTALL_ROOT}/tools-x86
}


# the driver is a simple python script which changes its behavior
# depending under the name it is invoked as
InstallDriver() {
  Banner "installing driver adaptors"
  cp tools/llvm/llvm-fake.py ${INSTALL_ROOT}/arm-none-linux-gnueabi/
  for s in gcc g++ as \
           sfigcc bcgcc \
           sfig++ bcg++ \
           sfild bcld \
           illegal nop ; do
    local t="llvm-fake-$s"
    echo "$t"
    ln -fs llvm-fake.py ${INSTALL_ROOT}/arm-none-linux-gnueabi/$t
  done
}


#
InstallNewlibAndNaClRuntime() {
  Banner "building and installing nacl runtime"

  SubBanner "building newib"
  rm -rf src/third_party/nacl_sdk/arm-newlib/
  tools/llvm/setup_arm_newlib.sh

  SubBanner "building extra sdk libs"
  rm -rf scons-out/nacl_extra_sdk-arm/
  ./scons MODE=nacl_extra_sdk \
          platform=arm \
          sdl_mode=none \
          sdl=none \
          naclsdk_mode=manual \
          naclsdk_validate=0 \
          extra_sdk_clean \
          extra_sdk_update_header \
          install_libpthread \
          extra_sdk_update
  cp -r src/third_party/nacl_sdk/arm-newlib ${INSTALL_ROOT}
}


InstallExamples() {
  Banner "installing examples into ${INSTALL_ROOT}/examples"
  rm -rf  ${INSTALL_ROOT}/examples/
  cp -r  tools/llvm/arm_examples ${INSTALL_ROOT}/examples/
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
#@ untrusted_sdk <tarball>
#@
#@   create untrusted sdk tarball
#@   This is the primary function of this script.
if [ ${MODE} = 'untrusted_sdk' ] ; then
  mkdir -p ${TMP}
  PathSanityCheck
  ClearInstallDir
  DownloadOrCopyCodeSourceryTarball
  ConfigureAndBuildLlvm
  UntarPatchConfigureAndBuildSfiLlc
  InstallUntrustedLinkerScript
  InstallDriver
  InstallSecondPhaseLlvmGccLibs
  # TODO(cbiffle): sandboxed libgcc build
  source tools/llvm/setup_arm_untrusted_toolchain.sh
  InstallNewlibAndNaClRuntime

  source tools/llvm/setup_arm_trusted_toolchain.sh
  InstallMiscTools
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
#@ phase2
#@
#@   build libgcc and libstdc++
if [ ${MODE} = 'phase2' ] ; then
  InstallSecondPhaseLlvmGccLibs
  exit 0
fi

#@
#@ newlib-etc
#@
#@   install newlib-etc
if [ ${MODE} = 'newlib-etc' ] ; then
  source tools/llvm/setup_arm_untrusted_toolchain.sh
  InstallNewlibAndNaClRuntime
  exit 0
fi

#@
#@ misc-tools
#@
#@   install misc tools
if [ ${MODE} = 'misc-tools' ] ; then
  source tools/llvm/setup_arm_untrusted_toolchain.sh
  source tools/llvm/setup_arm_trusted_toolchain.sh
  InstallMiscTools
  exit 0
fi

#@
#@ driver
#@
#@   install driver
if [ ${MODE} = 'driver' ] ; then
  InstallDriver
  exit 0
fi

#@
#@ prune
#@
#@   prune tree to make tarball smaller
if [ ${MODE} = 'prune' ] ; then
  PruneDirs
  exit 0
fi

#@
#@ examples
#@
#@   add examples
if [ ${MODE} = 'examples' ] ; then
  InstallExamples
  exit 0
fi


#@
#@ tar <tarball>
#@
#@   tar everything up
if [ ${MODE} = 'tar' ] ; then
  CreateTarBall $1
  exit 0
fi

ExtractLlvmBuildScript

echo "ERROR: unknown mode ${MODE}"
exit -1
