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
# Directory Layout Description
######################################################################

# All directories are relative to BASE which is
# currently native_client/toolchain/linux_arm-untrusted
#
# TODO(robertm): arm layout needs to be described

# /x86-32sfi-lib   [experimental] x86 sandboxed libraries and object files
# /x86-32sfi-tools [experimental] x86-32 crosstool binaries for building
#                  and linking  x86-32 nexes
######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

# NOTE: gcc and llvm have to be synchronized
#       we have chosen toolchains which both are based on gcc-4.2.1

readonly INSTALL_ROOT=$(pwd)/toolchain/linux_arm-untrusted
readonly CROSS_TARGET=arm-none-linux-gnueabi
readonly ARM_ARCH=armv7-a
readonly ARM_FPU=vfp3
readonly LLVM_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}/llvm"
readonly LLVMGCC_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}/llvm-gcc-4.2"
# NOTE: NEWLIB_INSTALL_DIR also server as a SYSROOT
readonly NEWLIB_INSTALL_DIR="${INSTALL_ROOT}/arm-newlib"
readonly DRIVER_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}"
# installing binutils and gcc in the same dir so that gcc
# uses the correct as and ld even if we move the install dir.
readonly BINUTILS_INSTALL_DIR="${LLVMGCC_INSTALL_DIR}"
readonly SANDBOXED_BINUTILS_INSTALL_DIR="${LLVMGCC_INSTALL_DIR}/sandboxed"

# This toolchain currenlty builds only on linux.
# TODO(abetul): Remove the restriction on developer's OS choices.
readonly NACL_TOOLCHAIN=$(pwd)/toolchain/linux_x86/sdk/nacl-sdk

readonly PATCH_DIR=$(pwd)/tools/patches

readonly BFD_PLUGIN_DIR=${LLVMGCC_INSTALL_DIR}/lib/bfd-plugins

readonly MAKE_OPTS="-j8 VERBOSE=1"

# The directory in which we we keep src dirs (from hg repos)
# and objdirs

readonly DEFAULT_TOOLCHAIN_CLIENT=$(pwd)/toolchain/hg
readonly TOOLCHAIN_CLIENT=${TOOLCHAIN_CLIENT:-${DEFAULT_TOOLCHAIN_CLIENT}}

# Current milestones within each Hg repo:
readonly LLVM_REV=4a9de5361ab6
readonly LLVM_GCC_REV=5cf5cb1fa2b2
readonly NEWLIB_REV=5d64fed35b93
readonly BINUTILS_REV=80d1927e7089

# These are simple compiler wrappers to force 32bit builds
# They are unused now. Instead we make sure that the toolchains that we
# distribute are created on the oldest system we care to support. Currently
# that is a 32 bit hardy. The advantage of this is that we can build
# the toolchaing shared, reducing its size and allowing the use of
# plugins. You can still use them by setting the environment variables
# when running this script:
# CC=$(readlink -f tools/llvm/mygcc32) \
# CXX=$(readlink -f tools/llvm/myg++32) \
# tools/llvm/untrusted-toolchain-creator.sh

CC=${CC:-}
# TODO(espindola): This should be ${CXX:-}, but llvm-gcc's configure has a
# bug that brakes the build if we do that.
CXX=${CXX:-g++}

readonly CROSS_TARGET_AR=${BINUTILS_INSTALL_DIR}/bin/${CROSS_TARGET}-ar
readonly CROSS_TARGET_AS=${BINUTILS_INSTALL_DIR}/bin/${CROSS_TARGET}-as
readonly CROSS_TARGET_LD=${BINUTILS_INSTALL_DIR}/bin/${CROSS_TARGET}-ld
readonly CROSS_TARGET_NM=${BINUTILS_INSTALL_DIR}/bin/${CROSS_TARGET}-nm
readonly CROSS_TARGET_RANLIB=${BINUTILS_INSTALL_DIR}/bin/${CROSS_TARGET}-ranlib
readonly ILLEGAL_TOOL=${DRIVER_INSTALL_DIR}/llvm-fake-illegal

# NOTE: this tools.sh defines: LD_FOR_TARGET, CC_FOR_TARGET, CXX_FOR_TARGET, ...
source tools/llvm/tools.sh
readonly LD_FOR_SFI_TARGET=${LD_FOR_TARGET}
readonly CC_FOR_SFI_TARGET=${CC_FOR_TARGET}
readonly CXX_FOR_SFI_TARGET=${CXX_FOR_TARGET}
readonly AR_FOR_SFI_TARGET=${AR_FOR_TARGET}
readonly NM_FOR_SFI_TARGET=${NM_FOR_TARGET}
readonly RANLIB_FOR_SFI_TARGET=${RANLIB_FOR_TARGET}

readonly CXXFLAGS_FOR_SFI_TARGET="-D__native_client__=1"
readonly CFLAGS_FOR_SFI_TARGET="-march=${ARM_ARCH} \
                                -DMISSING_SYSCALL_NAMES=1 \
                                -ffixed-r9 \
                                -D__native_client__=1"

# The gold plugin that we use is documented at
# http://llvm.org/docs/GoldPlugin.html
# Despite its name it is actually used by both gold and bfd. The changes to
# this file to enable its use are:
# * Build shared
# * --enable-gold and --enable-plugin when building binutils
# * --with-binutils-include when building binutils
# * linking the plugin in bfd-plugins

######################################################################
# Helpers
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


checkout-hg-common() {
  SubBanner "Checking out local repo for $1 (label: $2)"
  local dir=${TOOLCHAIN_CLIENT}/$1
  if [ -d ${dir} ] ; then
    echo "ERROR: hg repo ${dir} already exists"
    exit -1
  fi
  pushd ${TOOLCHAIN_CLIENT}
  hg clone https://$1.googlecode.com/hg/ $1
  cd $1
  hg up -C $2
  popd
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


RecordRevisionInfo() {
  svn info >  ${INSTALL_ROOT}/REV
}


tarball() {
  local tarball=$1
  Banner "creating tar ball ${tarball}"
  tar zcf ${tarball} -C ${INSTALL_ROOT} .
}

# try to keep the tarball small
prune() {
  Banner "pruning llvm sourcery tree"
  local LLVM_ROOT=${INSTALL_ROOT}/arm-none-linux-gnueabi
  SubBanner "Size before: $(du -msc  ${LLVM_ROOT})"
  rm  -f ${LLVM_ROOT}/llvm/lib/lib*.a
  SubBanner "Size after: $(du -msc  ${LLVM_ROOT})"
}

# Build basic llvm tools
llvm() {
  local objdir="${TOOLCHAIN_CLIENT}/build.llvm"
  local srcdir=$(readlink -f ${TOOLCHAIN_CLIENT}/nacl-llvm-branches)
  Banner "Building LLVM in ${objdir}"

  rm -rf ${objdir}
  mkdir -p ${objdir}
  pushd ${objdir}


  # The --with-binutils-include is to allow llvm to build the gold plugin
  local binutils_include="${TOOLCHAIN_CLIENT}/binutils.nacl-llvm-branches/binutils-2.20/include"
  RunWithLog "Configure" ${TOOLCHAIN_CLIENT}/log.llvm.configure \
      env -i PATH=/usr/bin/:/bin \
             MAKE_OPTS=${MAKE_OPTS} \
             CC=${CC} \
             CXX=${CXX} \
             ${srcdir}/llvm-trunk/configure \
             --disable-jit \
             --enable-optimized \
             --with-binutils-include=${binutils_include} \
             --enable-targets=x86,x86_64,arm \
             --target=arm-none-linux-gnueabi \
             --prefix=${LLVM_INSTALL_DIR} \
             --with-llvmgccdir=${LLVMGCC_INSTALL_DIR}

  RunWithLog "Make" ${TOOLCHAIN_CLIENT}/log.llvm.make \
    env -i PATH=/usr/bin/:/bin \
           MAKE_OPTS=${MAKE_OPTS} \
           CC=${CC} \
           CXX=${CXX} \
           make ${MAKE_OPTS} all

  RunWithLog "Installing LLVM" ${TOOLCHAIN_CLIENT}/log.llvm-install \
       make ${MAKE_OPTS} install

  SubBanner "Linking the plugin"
  mkdir -p ${BFD_PLUGIN_DIR}
  ln -sf ../../../llvm/lib/LLVMgold.so ${BFD_PLUGIN_DIR}
  ln -sf ../../../llvm/lib/libLTO.so ${BFD_PLUGIN_DIR}

  popd
}


SetupSysRoot() {
  local sys_include=${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/include
  local sys_include2=${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/sys-include
  SubBanner "Setting up initial sysroot in ${sys_include}"

  rm -rf ${sys_include} ${sys_include2}
  mkdir -p ${sys_include}
  ln -sf ${sys_include} ${sys_include2}
  cp -r ${TOOLCHAIN_CLIENT}/newlib.nacl-llvm-branches/newlib-1.17.0/newlib/libc/include/* ${sys_include}
}

# Build "pregcc" which is a gcc that does not depend on having glibc/newlib
# already compiled. This also generates some important headers (confirm this).
#
# NOTE: depends on newlib source being set up so we can use it to set
#       up a sysroot.
ConfigureAndBuildGccStage1() {
  local objdir="${TOOLCHAIN_CLIENT}/build.llvm-pregcc"
  local srcdir=$(readlink -f ${TOOLCHAIN_CLIENT}/llvm-gcc.nacl-llvm-branches)
  Banner "Building llvmgcc-stage1 in ${objdir}"

  rm -rf ${objdir}
  mkdir -p ${objdir}
  pushd ${objdir}

  # TODO(robertm): do we really need CROSS_TARGET_*
  RunWithLog "Configure" ${TOOLCHAIN_CLIENT}/llvm-pregcc.configure.log \
      env -i PATH=/usr/bin/:/bin \
             CC=${CC} \
             CXX=${CXX} \
             CFLAGS="-Dinhibit_libc" \
             ${srcdir}/llvm-gcc-4.2/configure \
               --prefix=${LLVMGCC_INSTALL_DIR} \
               --enable-llvm=${LLVM_INSTALL_DIR} \
               --without-headers \
               --program-prefix=llvm- \
               --disable-libmudflap \
               --disable-decimal-float \
               --disable-libssp \
               --disable-libgomp \
               --enable-languages=c \
               --disable-threads \
               --disable-libstdcxx-pch \
               --disable-shared \
               --target=${CROSS_TARGET} \
               --with-arch=${ARM_ARCH} \
               --with-fpu=${ARM_FPU}

 # NOTE: we add ${BINUTILS_INSTALL_DIR}/bin to PATH
 RunWithLog "Make" ${TOOLCHAIN_CLIENT}/log.llvm-pregcc.make \
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
             CC=${CC} \
             CXX=${CXX} \
             CFLAGS="-Dinhibit_libc" \
             make ${MAKE_OPTS} all

 # NOTE: we add ${BINUTILS_INSTALL_DIR}/bin to PATH
 RunWithLog "Install" ${TOOLCHAIN_CLIENT}/log.llvm-pregcc.install \
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
             CC=${CC} \
             CXX=${CXX} \
             CFLAGS="-Dinhibit_libc" \
             make ${MAKE_OPTS} install

  popd
}



STD_ENV_FOR_GCC_ETC=(
    CFLAGS_FOR_TARGET="${CFLAGS_FOR_SFI_TARGET}"
    CPPFLAGS_FOR_TARGET="${CXXFLAGS_FOR_SFI_TARGET}"
    CC_FOR_TARGET="${CC_FOR_SFI_TARGET}"
    GCC_FOR_TARGET="${CC_FOR_SFI_TARGET}"
    CXX_FOR_TARGET="${CXX_FOR_SFI_TARGET}"
    AS_FOR_TARGET="${CROSS_TARGET_AS}"
    LD_FOR_TARGET="${ILLEGAL_TOOL}"
    AR_FOR_TARGET="${CROSS_TARGET_AR}"
    NM_FOR_TARGET="${CROSS_TARGET_NM}"
    OBJDUMP_FOR_TARGET="${ILLEGAL_TOOL}"
    RANLIB_FOR_TARGET="${CROSS_TARGET_RANLIB}"
    STRIP_FOR_TARGET="${ILLEGAL_TOOL}")


BuildLibgcc() {
  local tmpdir=$1

  pushd ${tmpdir}/gcc

  Run "Build libgcc clean"\
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
             make clean-target-libgcc

  RunWithLog "Build libgcc" ${TOOLCHAIN_CLIENT}/log.llvm-gcc.make_libgcc \
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
             make \
             "${STD_ENV_FOR_GCC_ETC[@]}" \
             ${MAKE_OPTS} libgcc.a

  popd
}


BuildLibiberty() {
  local tmpdir=$1

  pushd ${tmpdir}
  # maybe clean libiberty first
  RunWithLog "Build libiberty" ${TOOLCHAIN_CLIENT}/log.llvm-gcc.make_libiberty \
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
             make \
             "${STD_ENV_FOR_GCC_ETC[@]}" \
             ${MAKE_OPTS} all-target-libiberty
  popd
}

STD_ENV_FOR_LIBSTDCPP=(
  CC="${CC_FOR_SFI_TARGET}"
  CXX="${CXX_FOR_SFI_TARGET}"
  RAW_CXX_FOR_TARGET="${CXX_FOR_SFI_TARGET}"
  LD="${LD_FOR_SFI_TARGET}"
  CFLAGS="${CXXFLAGS_FOR_SFI_TARGET}"
  CPPFLAGS="${CXXFLAGS_FOR_SFI_TARGET}"
  CXXFLAGS="${CXXFLAGS_FOR_SFI_TARGET}"
  CFLAGS_FOR_TARGET="${CFLAGS_FOR_SFI_TARGET}"
  CPPFLAGS_FOR_TARGET="${CXXFLAGS_FOR_SFI_TARGET}"
  CC_FOR_TARGET="${CC_FOR_SFI_TARGET}"
  GCC_FOR_TARGET="${CC_FOR_SFI_TARGET}"
  CXX_FOR_TARGET="${CXX_FOR_SFI_TARGET}"
  AR_FOR_TARGET="${AR_FOR_SFI_TARGET}"
  NM_FOR_TARGET="${NM_FOR_SFI_TARGET}"
  RANLIB_FOR_TARGET="${RANLIB_FOR_SFI_TARGET}"
  AS_FOR_TARGET="${ILLEGAL_TOOL}"
  LD_FOR_TARGET="${ILLEGAL_TOOL}"
  OBJDUMP_FOR_TARGET="${ILLEGAL_TOOL}" )




BuildLibstdcpp() {
  local srcdir=$(readlink -f ${TOOLCHAIN_CLIENT}/llvm-gcc.nacl-llvm-branches)
  # Not sure whetehr this really has to be within the enclosing build-dir
  local objdir=$1/${CROSS_TARGET}/libstdc++-v3

  rm -rf ${objdir}
  mkdir -p ${objdir}
  pushd ${objdir}

  RunWithLog "Configure libstdc++" ${TOOLCHAIN_CLIENT}/log.llvm-gcc.configure_libstdcpp \
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
        "${STD_ENV_FOR_LIBSTDCPP[@]}" \
        ${srcdir}/llvm-gcc-4.2/libstdc++-v3/configure \
          --host=arm-none-linux-gnueabi \
          --prefix=${LLVMGCC_INSTALL_DIR} \
          --enable-llvm=${LLVM_INSTALL_DIR} \
          --program-prefix=llvm- \
          --with-newlib \
          --disable-libstdcxx-pch \
          --disable-shared \
          --enable-languages=c,c++ \
          --target=${CROSS_TARGET} \
          --with-sysroot=${NEWLIB_INSTALL_DIR} \
          --with-arch=${ARM_ARCH} \
          --srcdir=${srcdir}/llvm-gcc-4.2/libstdc++-v3

  RunWithLog "Make libstdc++" ${TOOLCHAIN_CLIENT}/log.llvm-gcc.make_libstdcpp \
    env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
        make \
        "${STD_ENV_FOR_LIBSTDCPP[@]}" \
        ${MAKE_OPTS}

#  RunWithLog "Install libstdc++" ${TMP}/llvm-gcc.install_libstdcpp.log \
#    env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
#      make  ${MAKE_OPTS} install
  popd
}

# Build gcc again in order to build libgcc and other essential libs
# Note: depends on
gcc-stage2() {
  local objdir="${TOOLCHAIN_CLIENT}/build.llvm-gcc"
  local srcdir=$(readlink -f ${TOOLCHAIN_CLIENT}/llvm-gcc.nacl-llvm-branches)
  Banner "Building llvmgcc-stage2 in ${objdir}"

  rm -rf ${objdir}
  mkdir -p ${objdir}
  pushd ${objdir}

  # TODO(robertm): do we really need CROSS_TARGET_*
  RunWithLog "Configure" ${TOOLCHAIN_CLIENT}/log.llvm-gcc.configure \
      env -i PATH=/usr/bin/:/bin \
             CC=${CC} \
             CXX=${CXX} \
             CFLAGS="-Dinhibit_libc" \
             CXXFLAGS="-Dinhibit_libc" \
             "${STD_ENV_FOR_GCC_ETC[@]}" \
             ${srcdir}/llvm-gcc-4.2/configure \
               --prefix=${LLVMGCC_INSTALL_DIR} \
               --enable-llvm=${LLVM_INSTALL_DIR} \
               --program-prefix=llvm- \
               --with-newlib \
               --disable-libmudflap \
               --disable-decimal-float \
               --disable-libssp \
               --disable-libgomp \
               --enable-languages=c,c++ \
               --disable-threads \
               --disable-libstdcxx-pch \
               --disable-shared \
               --target=${CROSS_TARGET} \
               --with-arch=${ARM_ARCH} \
               --with-fpu=${ARM_FPU} \
               --with-sysroot=${NEWLIB_INSTALL_DIR}

 RunWithLog "Make" ${TOOLCHAIN_CLIENT}/log.llvm-gcc.make \
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
             make \
             "${STD_ENV_FOR_GCC_ETC[@]}" \
             ${MAKE_OPTS} all-gcc


  BuildLibgcc ${objdir}
  BuildLibiberty ${objdir}

  SubBanner "Partial compiler install for gcc"
  cd gcc
  cp gcc-cross ${LLVMGCC_INSTALL_DIR}/bin/llvm-gcc
  cp g++-cross ${LLVMGCC_INSTALL_DIR}/bin/llvm-g++
  # NOTE: the "cp" will fail when we upgrade to a more recent compiler,
  #       simply fix this version when this happens
  cp cc1 ${LLVMGCC_INSTALL_DIR}/libexec/gcc/${CROSS_TARGET}/4.2.1
  cp cc1plus ${LLVMGCC_INSTALL_DIR}/libexec/gcc/${CROSS_TARGET}/4.2.1

  BuildLibstdcpp ${objdir}
  popd
}


gcc-stage3() {
  # NOTE: this is the build dir from stage2
  local objdir="${TOOLCHAIN_CLIENT}/build.llvm-gcc"

  Banner "Installing final version of llvmgcc (stage3)"

  pushd ${objdir}
  RunWithLog "Make" ${TOOLCHAIN_CLIENT}/log.llvm-gcc.install \
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
             make \
             "${STD_ENV_FOR_GCC_ETC[@]}" \
             install

  SubBanner "Install libgcc.a library"
  # NOTE: the "cp" will fail when we upgrade to a more recent compiler,
  #       simply fix this version when this happens
  cp gcc/libg*.a ${LLVMGCC_INSTALL_DIR}/lib/gcc/${CROSS_TARGET}/4.2.1/

  SubBanner "Install libiberty.a"
  cp libiberty/libiberty.a ${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/lib

  SubBanner "Install libstdc++, header massaging"
  echo "headers"

  # Don't ask
  rm -f ${LLVMGCC_INSTALL_DIR}/arm-none-linux-gnueabi/include/c++
  ln -s ../../include/c++ \
        ${LLVMGCC_INSTALL_DIR}/arm-none-linux-gnueabi/include/c++

  popd
}

# We need to adjust the start address and aligment of nacl arm modules
InstallUntrustedLinkerScript() {
   Banner "installing untrusted ld script"
   cp tools/llvm/ld_script_arm_untrusted ${INSTALL_ROOT}/arm-none-linux-gnueabi/
}


# we copy some useful tools after building them first
misc-tools() {
   Banner "building and installing misc tools"

   if [ ! -f toolchain/linux_arm-trusted/ld_script_arm_trusted ]; then
     cat <<EOF
The trusted toolchain doesn't appear to be installed, so this step
will fail due to missing libraries.  Run:

  % tools/llvm/trusted-toolchain-creator.sh trusted_sdk

This takes a while.  Alternatively, you could run:

  % tools/llvm/pnacl-helper.sh download-toolchains

or even:

  % ./scons platform=arm --download sdl=none

but be warned that both of these will clobber your untrusted
toolchains tree, negating the work done so far, so you may want to
manually save/restore the untrusted toolchains.
EOF
     exit -1
   fi

   # TODO(robertm): revisit some of these options
   Run "sel loader" \
          ./scons MODE=nacl,opt-linux \
          platform=arm \
          sdl=none \
          naclsdk_validate=0 \
          sysinfo= \
          sel_ldr
   rm -rf  ${INSTALL_ROOT}/tools-arm
   mkdir ${INSTALL_ROOT}/tools-arm
   cp scons-out/opt-linux-arm/obj/src/trusted/service_runtime/sel_ldr\
     ${INSTALL_ROOT}/tools-arm

   Run "validator" \
           ./scons MODE=opt-linux \
           targetplatform=arm \
           sysinfo= \
           arm-ncval-core
   rm -rf  ${INSTALL_ROOT}/tools-x86
   mkdir ${INSTALL_ROOT}/tools-x86
   cp scons-out/opt-linux-x86-32-to-arm/obj/src/trusted/validator_arm/\
arm-ncval-core ${INSTALL_ROOT}/tools-x86
}


# the driver is a simple python script which changes its behavior
# depending under the name it is invoked as
InstallDriver() {
  Banner "installing driver adaptors to ${DRIVER_INSTALL_DIR}"
  # TODO(robertm): move the driver to their own dir
  #rm -rf  ${DRIVER_INSTALL_DIR}
  #mkdir -p ${DRIVER_INSTALL_DIR}
  cp tools/llvm/llvm-fake.py ${DRIVER_INSTALL_DIR}
  for s in gcc g++ \
           sfigcc bcgcc \
           sfig++ bcg++ \
           cppas-arm cppas-x86-32 cppas-x86-64 \
           sfild bcld-arm bcld-x86-32 bcld-x86-64 \
           illegal nop ; do
    local t="llvm-fake-$s"
    echo "$t"
    ln -fs llvm-fake.py ${DRIVER_INSTALL_DIR}/$t
  done
}

# NOTE: we do not expect the assembler or linker to be used to build newlib.a
STD_ENV_FOR_NEWLIB=(
    CFLAGS_FOR_TARGET="${CFLAGS_FOR_SFI_TARGET}"
    CPPFLAGS_FOR_TARGET="${CXXFLAGS_FOR_SFI_TARGET}"
    CC_FOR_TARGET="${CC_FOR_SFI_TARGET}"
    GCC_FOR_TARGET="${CC_FOR_SFI_TARGET}"
    CXX_FOR_TARGET="${CXX_FOR_SFI_TARGET}"
    AR_FOR_TARGET="${AR_FOR_SFI_TARGET}"
    NM_FOR_TARGET="${NM_FOR_SFI_TARGET}"
    RANLIB_FOR_TARGET="${RANLIB_FOR_SFI_TARGET}"
    OBJDUMP_FOR_TARGET="${ILLEGAL_TOOL}"
    AS_FOR_TARGET="${ILLEGAL_TOOL}"
    LD_FOR_TARGET="${ILLEGAL_TOOL}"
    STRIP_FOR_TARGET="${ILLEGAL_TOOL}")

BuildAndInstallBinutils() {
  local objdir="${TOOLCHAIN_CLIENT}/build.binutils.nacl"
  local srcdir=$(readlink -f ${TOOLCHAIN_CLIENT}/binutils.nacl-llvm-branches)
  Banner "Building binutils"

  rm -rf ${objdir}
  mkdir -p ${objdir}
  pushd ${objdir}

  # --enable-checking is to avoid a build failure:
  #   tc-arm.c:2489: warning: empty body in an if-statement
  # The --enable-gold and --enable-plugins options are on so that we
  # can use gold's support for plugin to link PNaCl modules.
  RunWithLog "Configuring binutils" ${TOOLCHAIN_CLIENT}/log.binutils.configure \
    env -i \
    PATH="/usr/bin:/bin" \
    CC=${CC} \
    CXX=${CXX} \
    ${srcdir}/binutils-2.20/configure --prefix=${BINUTILS_INSTALL_DIR} \
                                      --target=${CROSS_TARGET} \
                                      --enable-checking \
                                      --enable-gold \
                                      --enable-plugins \
                                      --with-sysroot=${NEWLIB_INSTALL_DIR}

  RunWithLog "Make binutils" ${TOOLCHAIN_CLIENT}/log.binutils.make \
    env -i PATH="/usr/bin:/bin" \
    make ${MAKE_OPTS}

  RunWithLog "Install binutils"  ${TOOLCHAIN_CLIENT}/log.binutils.install \
    env -i PATH="/usr/bin:/bin" \
    make \
      install ${MAKE_OPTS}

  popd
}

BuildAndInstallSandboxedBinutils() {
  local objdir="${TOOLCHAIN_CLIENT}/build.binutils.nacl.sandboxed"
  local srcdir=$(readlink -f ${TOOLCHAIN_CLIENT}/binutils.nacl-llvm-branches)

  if [ ! -d ${NACL_TOOLCHAIN} ] ; then
    echo "ERROR: install Native Client toolchain"
    exit -1
  fi

  Banner "Building sandboxed binutils"

  rm -rf ${objdir}
  mkdir -p ${objdir}
  pushd ${objdir}
  # --enable-checking is to avoid a build failure:
  #   tc-arm.c:2489: warning: empty body in an if-statement
  # The --enable-gold and --enable-plugins options are on so that we
  # can use gold's support for plugin to link PNaCl modules.
  RunWithLog "Configuring sandboxed binutils" \
    ${TOOLCHAIN_CLIENT}/log.binutils.sandboxed.configure \
    env -i \
    PATH="/usr/bin:/bin" \
    AR="${NACL_TOOLCHAIN}/bin/nacl-ar" \
    AS="${NACL_TOOLCHAIN}/bin/nacl-as" \
    CC="${NACL_TOOLCHAIN}/bin/nacl-gcc" \
    CXX="${NACL_TOOLCHAIN}/bin/nacl-g++" \
    EMULATOR_FOR_BUILD="$(pwd)/scons-out/dbg-linux-x86-32/staging/sel_ldr -d" \
    LD="${NACL_TOOLCHAIN}/bin/nacl-ld" \
    RANLIB="${NACL_TOOLCHAIN}/bin/nacl-ranlib" \
    CFLAGS="-O2 -DPNACL_TOOLCHAIN_SANDBOX -I${NACL_TOOLCHAIN}/nacl/include" \
    LDFLAGS="-s" \
    ${srcdir}/binutils-2.20/configure --prefix=${SANDBOXED_BINUTILS_INSTALL_DIR} \
                                   --host=nacl \
                                   --target=${CROSS_TARGET} \
                                   --disable-nls \
                                   --enable-checking \
                                   --enable-static \
                                   --enable-shared=no \
                                   --with-sysroot=${NEWLIB_INSTALL_DIR}

  RunWithLog "Make binutils" ${TOOLCHAIN_CLIENT}/log.binutils.sandboxed.make \
    env -i PATH="/usr/bin:/bin" \
    make ${MAKE_OPTS} all-gas all-ld

  RunWithLog "Install binutils" ${TOOLCHAIN_CLIENT}/log.binutils.sandboxed.install \
    env -i PATH="/usr/bin:/bin" \
    make install-gas install-ld

  popd
}

BuildAndInstallNewlib() {
  local objdir="${TOOLCHAIN_CLIENT}/build.newlib"
  local srcdir=$(readlink -f ${TOOLCHAIN_CLIENT}/newlib.nacl-llvm-branches)

  # NOTE: When an argument is given to this function we abuse it to generate
  #       a bitcode version of the library. In the scenario we just want to
  #       build libc.a and do install or uninstall anything.
  if [ $# == 0 ] ; then
    rm -rf ${NEWLIB_INSTALL_DIR}
  elif [ ! -d "$1" ]; then
    echo "Error: no such directory '$1'." >&2
    exit 1
  fi
  Banner "building and installing newlib in ${objdir}"

  rm -rf ${objdir}
  mkdir -p ${objdir}
  pushd ${objdir}

  RunWithLog "Configuring newlib"  ${TOOLCHAIN_CLIENT}/log.newlib.configure \
    env -i \
    PATH="/usr/bin:/bin" \
    "${STD_ENV_FOR_NEWLIB[@]}" \
    ${srcdir}/newlib-1.17.0/configure \
        --disable-libgloss \
        --disable-multilib \
        --enable-newlib-reent-small \
        --prefix="${NEWLIB_INSTALL_DIR}" \
        --disable-newlib-supplied-syscalls \
        --disable-texinfo \
        --target="${CROSS_TARGET}"


  RunWithLog "Make newlib"  ${TOOLCHAIN_CLIENT}/log.newlib.make \
    env -i PATH="/usr/bin:/bin" \
    make \
      "${STD_ENV_FOR_NEWLIB[@]}" \
      ${MAKE_OPTS}


  if [ $# == 1 ] ; then
    SubBanner "installing lib[cgm].a to $1"
    cp ${objdir}/arm-none-linux-gnueabi/newlib/lib[cgm].a $1
    return
  fi

  RunWithLog "Install newlib"  ${TOOLCHAIN_CLIENT}/log.newlib.install \
    env -i PATH="/usr/bin:/bin" \
      make \
      "${STD_ENV_FOR_NEWLIB[@]}" \
      install ${MAKE_OPTS}

  SubBanner "extra install newlib"
  local sys_include=${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/include
  local sys_lib=${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/lib
  # NOTE: we provide a new one via extra-sdk
  rm ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/include/pthread.h

  cp ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/include/machine/endian.h \
    ${sys_include}
  cp ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/include/sys/param.h \
    ${sys_include}
  cp ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/include/newlib.h \
    ${sys_include}

  cp -r ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/lib/* ${sys_lib}


  # NOTE: we provide our own pthread.h via extra-sdk
  p1="${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/usr/include/pthread.h"
  p2="${sys_include}/pthread.h"
  for p in $p1 $p2  ; do
    echo "remove bad pthread: $p"
    rm -f $p
  done

  popd
}


extrasdk() {
  Banner "building extra sdk"
  ./scons MODE=nacl_extra_sdk \
          platform=arm \
          sdl=none \
          naclsdk_validate=0 \
          extra_sdk_clean \
          extra_sdk_update_header \
          install_libpthread \
          extra_sdk_update
}


examples() {
  Banner "installing examples into ${INSTALL_ROOT}/examples"
  rm -rf  ${INSTALL_ROOT}/examples/
  cp -r  tools/llvm/arm_examples ${INSTALL_ROOT}/examples/
}


# NOTE: Experiment x86-32 support
add-x86-basics-32() {
  Banner "installing experimental x86-32 support"
  local libdir="${INSTALL_ROOT}/x86-32sfi-lib"
  mkdir -p ${libdir}

  SubBanner "rebuilding stubs for x86"
  rm -f scons-out/nacl_extra_sdk-x86-32/obj/src/untrusted/stubs/*.o
  # NOTE: this does way too much - we only want the stubs
  ./scons MODE=nacl_extra_sdk platform=x86-32 \
      extra_sdk_clean extra_sdk_update_header extra_sdk_update
  cp scons-out/nacl_extra_sdk-x86-32/obj/src/untrusted/stubs/*.o ${libdir}

  SubBanner "installing x86 libgcc libs into ${libdir}"
  cp -r toolchain/linux_x86-32/sdk/nacl-sdk/lib/gcc/nacl/4.2.2/libgcc.a \
    ${libdir}

  local toolsdir="${INSTALL_ROOT}/x86-32sfi-tools"
  mkdir -p ${toolsdir}
  SubBanner "installing x86 linker script and tools into ${toolsdir}"
  cp tools/llvm/ld_script_x86_untrusted ${toolsdir}
  cp toolchain/linux_x86-32/sdk/nacl-sdk/bin/nacl-as ${toolsdir}
  cp toolchain/linux_x86-32/sdk/nacl-sdk/bin/nacl-ld ${toolsdir}
}


######################################################################
# Actions

#@
#@ help
#@
#@   Print help for all modes.
help() {
  Usage
}

#@
#@ add-nacl-headers-to-newlib-src
#@
#@   Modify newlib with latest nacl headers
add-nacl-headers-to-newlib-src() {
  Banner "Adding nacl headers to newlib"
  here=$(pwd)
  pushd ${TOOLCHAIN_CLIENT}/newlib.nacl-llvm-branches/newlib-1.17.0
  ${here}/src/trusted/service_runtime/export_header.py \
      ${here}/src/trusted/service_runtime/include \
      newlib/libc/include
  popd
}

#@
#@ del-nacl-headers-to-newlib-src
#@
#@   Cleanse newlib src from all changes
del-nacl-headers-in-newlib-src() {
  Banner "Deleting nacl headers in newlib"
  here=$(pwd)
  pushd ${TOOLCHAIN_CLIENT}/newlib.nacl-llvm-branches/newlib-1.17.0
  hg revert newlib/libc/include/
  hg clean newlib/libc/include/
  popd
}

#@
#@ checkout-toolchain-client
#@
#@   check out all srcdirs/repos needed to build TC
checkout-toolchain-client() {
  Banner "Creating client in ${TOOLCHAIN_CLIENT}"
  rm -rf ${TOOLCHAIN_CLIENT}
  mkdir -p ${TOOLCHAIN_CLIENT}

  checkout-hg-common llvm-gcc.nacl-llvm-branches ${LLVM_GCC_REV}
  checkout-hg-common nacl-llvm-branches ${LLVM_REV}
  checkout-hg-common newlib.nacl-llvm-branches ${NEWLIB_REV}
  checkout-hg-common binutils.nacl-llvm-branches ${BINUTILS_REV}

  add-nacl-headers-to-newlib-src
}

#@
#@ untrusted_sdk_experimental_build
#@
#@   rebuild the sdk from existing repos
untrusted_sdk_experimental_build() {
  PathSanityCheck
  ClearInstallDir
  RecordRevisionInfo
  BuildAndInstallBinutils
  llvm
  gcc-stage1
  driver
  gcc-stage2
  gcc-stage3

  # NOTE: this builds native libs NOT needed for pnacl and currently
  #       also does some header file shuffling which IS needed
  BuildAndInstallNewlib
  extrasdk
}

#@
#@ untrusted_sdk <tarball>
#@
#@   Create untrusted SDK tarball.
#@   This is the primary function of this script.
untrusted_sdk() {
  if [ ! -n "${1:-}" ]; then
    echo "Error: untrusted_sdk needs a tarball name." >&2
    exit 1
  fi
  checkout-toolchain-client

  untrusted_sdk_experimental_build

  misc-tools
  examples
  prune
  tarball $1
}


#@
#@ untrusted_sdk_from_llvm_checkout <dir>  <tarball>
#@
#@   Create untrusted SDK tarball given a dir wirh checked out llvm branch
untrusted_sdk_from_llvm_checkout() {
  if [ ! -d $1/.hg ] ; then
    echo "ERRROR: there is no .hg file in $1"
    exit -1
  fi
  export LLVM_DEV=$1
  untrusted_sdk $2
}

#@
#@ llvm
#@
#@   Configure, build and install LLVM.

#@
#@ sandboxed-binutils-only
#@
#@   install sandboxed-binutils
sandboxed-binutils-only() {
  BuildAndInstallSandboxedBinutils
}

#@
#@ gcc-stage1
#@
#@   install pre-gcc
gcc-stage1() {
  SetupSysRoot
  ConfigureAndBuildGccStage1
}

#@
#@ gcc-stage2
#@
#@   NOTE: depends on installation of gcc-stage1
#@   build libgcc, libiberty, libstc++

#@
#@ gcc-stage3
#@
#@   NOTE: depends on installation of gcc-stage2

#@
#@ newlib
#@
#@   Build and install newlib.
newlib() {
  BuildAndInstallNewlib
}

#@
#@ newlib-libonly <target-dir>
#@
#@   Build and install newlib.
newlib-libonly() {
  BuildAndInstallNewlib $1
}

#@
#@ libstdcpp-libonly <target-dir>
#@
#@   Build and install libstc++.
libstdcpp-libonly() {
  if [ ! -d "$1" ]; then
    echo "Error: no such directory '$1'." >&2
    exit 1
  fi
  dest=$(readlink -f $1)
  objdir=${TOOLCHAIN_CLIENT}/build.libstdcpp.only

  rm -rf ${objdir}
  mkdir -p ${objdir}
  pushd ${objdir}

  BuildLibstdcpp ${objdir}
  cp ${objdir}/${CROSS_TARGET}/libstdc++-v3/src/.libs/libstdc++.a ${dest}
  popd
}

#@
#@ extrasdk
#@
#@   Build and install extra sdk libs and headers.

#@
#@ misc-tools
#@
#@   Build and install misc tools such as sel_ldr, validator.

#@
#@ driver
#@
#@   Install driver.
driver() {
  InstallUntrustedLinkerScript
  InstallDriver
}

#@
#@ driver-symlink
#@
#@   Install driver as a symlink into the client
#@   so that driver development is simplified.
#@   NOTE: This will make the INSTALL_ROOT unsuitable
#@         for being tar'ed up as a self contained toolchain.
driver-symlink() {
  abs_path=$(readlink -f tools/llvm/llvm-fake.py)
  driver=${INSTALL_ROOT}/arm-none-linux-gnueabi/llvm-fake.py
  ln -sf ${abs_path} ${driver}
}

#@
#@ prune
#@
#@   Prune tree to make tarball smaller.

#@
#@ examples
#@
#@   Install examples.

#@
#@ add-x86-basics-32
#@
#@   Install x86 stubs, libraries, linker scripts and tools.

#@
#@ tarball <tarball>
#@
#@   Tar everything up.

######################################################################
# Main

if [ $(basename $(pwd)) != "native_client" ]; then
  echo "You must run this script from the 'native_client' directory." >&2
  exit -1
fi

[ $# = 0 ] && set -- help  # Avoid reference to undefined $1.
if [ "$(type -t $1)" != "function" ]; then
  Usage
  echo "ERROR: unknown mode '$1'." >&2
  exit 1
fi

eval "$@"
