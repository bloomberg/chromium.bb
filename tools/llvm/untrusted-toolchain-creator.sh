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
readonly LLVM_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}/llvm"
readonly LLVMGCC_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}/llvm-gcc-4.2"
# NOTE: NEWLIB_INSTALL_DIR also server as a SYSROOT
readonly NEWLIB_INSTALL_DIR="${INSTALL_ROOT}/arm-newlib"
readonly DRIVER_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}"
# installing binutils and gcc in the same dir so that gcc
# uses the correct as and ld even if we move the install dir.
readonly BINUTILS_INSTALL_DIR="${LLVMGCC_INSTALL_DIR}"

readonly PATCH_DIR=$(pwd)/tools/patches

readonly BFD_PLUGIN_DIR=${LLVMGCC_INSTALL_DIR}/lib/bfd-plugins

readonly MAKE_OPTS="-j8 VERBOSE=1"

# The directory in which we perform all our builds.
# TODO(adonovan): make this include a hash (or segments) of $(pwd) so
# we don't clobber builds done from other clients.  (The Hg repos could
# be shared, though.)
readonly TMP=/tmp/crosstool-untrusted

# Environmental options: each optional *_DEV variable specifies a
# directory to use instead of the Mercurial repository.  Use these
# options to test pending changes.
readonly LLVM_GCC_DEV=${LLVM_GCC_DEV:-}   # llvm-gcc
readonly LLVM_DEV=${LLVM_DEV:-}           # llvm
readonly NEWLIB_DEV=${NEWLIB_DEV:-}       # newlib
readonly BINUTILS_DEV=${BINUTILS_DEV:-}   # binutils

# Current milestones within each Hg repo:
readonly LLVM_REV=8f933e976274 # (on the arm-sfi branch)
readonly LLVM_GCC_REV=1e60ad236357
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
readonly CFLAGS_FOR_SFI_TARGET="-march=armv6 \
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

# Usage: HgMirror <repo>
#
# Clones the specified Mercurial repository from
# nacl-llvm-branches.googlecode.com into the local "hg mirrors"
# directory, to be used by subsequent steps.
HgMirror() {
  local repo=$1
  local repo_mirror=${TMP}/hg-mirrors
  local url=https://${repo}.googlecode.com/hg/
  mkdir -p ${repo_mirror}
  echo "HgMirror: creating local mirror of ${url} Mercurial repository..."
  trap "rm -fr ${repo_mirror}/${repo}" EXIT
  if [ -d ${repo_mirror}/${repo} ]; then
    # Already exists: just bring it up-to-date.
    (cd ${repo_mirror}/${repo} && hg pull)
  else
    # Absent: create anew.
    hg clone ${url} ${repo_mirror}/${repo}
  fi
  trap - EXIT
  echo "HgMirror: done."
}

# Usage: SetupSourceTree <dest> <repo-name> <label> <dev-tree>
#
# Sets up a source tree <dest> by cloning the Mercurial repository
# <repo-name> at label <label> (e.g. an Hg branch name or revision
# specification), or if <dev-tree> is non-empty, by copying the tree
# <dev-tree>.
SetupSourceTree() {
  local dest=$1
  local repo_name=$2
  local label=$3
  local dev_tree=${4:-}

  rm -rf ${dest}  # TODO(adonovan): optimise around this when safe

  mkdir -p $(dirname ${dest})

  if [ -n "${dev_tree}" ]; then
    # User has requested that we use their development tree instead.
    echo "Copying user's development tree ${dev_tree}..."
    cp -ru ${dev_tree} ${dest}
  else
    HgMirror ${repo_name}
    echo "Making local checkout of repo ${repo_name} at label ${label}..."
    hg clone ${TMP}/hg-mirrors/${repo_name} ${dest}
    (cd ${dest} && hg up -C ${label})
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
  local tmpdir=${TMP}/llvm

  Banner "Building LLVM in ${tmpdir}"

  SetupSourceTree ${tmpdir} \
                  nacl-llvm-branches \
                  ${LLVM_REV} \
                  ${LLVM_DEV}
  pushd ${tmpdir}/llvm-trunk

  # TODO(adonovan): this is a hack which Cliff will revert ASAP (Jun
  # 14 2010)
  Run "Patching hack" patch -p1 < ${PATCH_DIR}/llvm-hack-sfi.patch

  # The --with-binutils-include is to allow llvm to build the gold plugin
  local binutils_include="${TMP}/binutils.nacl/src/binutils-2.20/include"
  RunWithLog "Configure" ${TMP}/llvm.configure.log\
      env -i PATH=/usr/bin/:/bin \
             MAKE_OPTS=${MAKE_OPTS} \
             CC=${CC} \
             CXX=${CXX} \
             ./configure \
             --disable-jit \
             --enable-optimized \
             --with-binutils-include=${binutils_include} \
             --enable-targets=x86,x86_64,arm \
             --target=arm-none-linux-gnueabi \
             --prefix=${LLVM_INSTALL_DIR} \
             --with-llvmgccdir=${LLVMGCC_INSTALL_DIR}

  RunWithLog "Make" ${TMP}/llvm.make.log \
    env -i PATH=/usr/bin/:/bin \
           MAKE_OPTS=${MAKE_OPTS} \
           CC=${CC} \
           CXX=${CXX} \
           make ${MAKE_OPTS} all

  RunWithLog "Installing LLVM" ${TMP}/llvm-install.log \
       make ${MAKE_OPTS} install

  Run "Linking llc-sfi" ln -s \
           llc ${INSTALL_ROOT}/arm-none-linux-gnueabi/llvm/bin/llc-sfi

  SubBanner "Linking the plugin"
  mkdir -p ${BFD_PLUGIN_DIR}
  ln -s ../../../llvm/lib/LLVMgold.so ${BFD_PLUGIN_DIR}
  ln -s ../../../llvm/lib/libLTO.so ${BFD_PLUGIN_DIR}

  popd
}

SetupNewlibSource() {
  local tmpdir=${TMP}/newlib

  Banner "Setting up newlib source in ${tmpdir}"

  SetupSourceTree ${tmpdir} \
                  newlib.nacl-llvm-branches \
                  ${NEWLIB_REV} \
                  ${NEWLIB_DEV}

  SubBanner "add nacl headers"

  here=$(pwd)
  (cd ${tmpdir}/newlib-1.17.0 &&
    ${here}/src/trusted/service_runtime/export_header.py \
      ${here}/src/trusted/service_runtime/include \
      newlib/libc/include)
}

# Note: depends on newlib source being set up.
SetupSysRoot() {
  local sys_include=${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/include
  local sys_include2=${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/sys-include
  SubBanner "Setting up initial sysroot in ${sys_include}"

  rm -rf ${sys_include} ${sys_include2}
  mkdir -p ${sys_include}
  ln -sf ${sys_include} ${sys_include2}
  cp -r ${TMP}/newlib/newlib-1.17.0/newlib/libc/include/* ${sys_include}
}

# Build "pregcc" which is a gcc that does not depend on having glibc/newlib
# already compiled. This also generates some important headers (confirm this).
#
# NOTE: depends on newlib source being set up so we can use it to set
#       up a sysroot.
ConfigureAndBuildGccStage1() {
  local tmpdir=${TMP}/llvm-pregcc

  Banner "Building llvmgcc-stage1 in ${tmpdir}"

  SetupSourceTree ${tmpdir} \
                  llvm-gcc.nacl-llvm-branches \
                  ${LLVM_GCC_REV} \
                  ${LLVM_GCC_DEV}
  mkdir -p ${tmpdir}/build
  pushd ${tmpdir}/build

  # NOTE: you cannot build llvm-gcc inside the source directory

  # TODO(robertm): do we really need CROSS_TARGET_*
  RunWithLog "Configure" ${TMP}/llvm-pregcc.configure.log \
      env -i PATH=/usr/bin/:/bin \
             CC=${CC} \
             CXX=${CXX} \
             CFLAGS="-Dinhibit_libc" \
             ../llvm-gcc-4.2/configure \
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
               --with-arch=armv6 \
               --with-fpu=vfp

 # NOTE: we add ${BINUTILS_INSTALL_DIR}/bin to PATH
 RunWithLog "Make" ${TMP}/llvm-pregcc.make.log \
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
             CC=${CC} \
             CXX=${CXX} \
             CFLAGS="-Dinhibit_libc" \
             make ${MAKE_OPTS} all

 # NOTE: we add ${BINUTILS_INSTALL_DIR}/bin to PATH
 RunWithLog "Install" ${TMP}/llvm-pregcc.install.log \
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

  RunWithLog "Build libgcc" ${TMP}/llvm-gcc.make_libgcc.log \
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
  RunWithLog "Build libiberty" ${TMP}/llvm-gcc.make_libiberty.log \
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
  local tmpdir=$1
  pushd ${tmpdir}
  local src_dir=${tmpdir}/../llvm-gcc-4.2
  local cpp_build_dir=${tmpdir}/${CROSS_TARGET}/libstdc++-v3
  rm -rf ${cpp_build_dir}
  mkdir -p  ${cpp_build_dir}
  cd ${cpp_build_dir}

  RunWithLog "Configure libstdc++" ${TMP}/llvm-gcc.configure_libstdcpp.log \
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
        "${STD_ENV_FOR_LIBSTDCPP[@]}" \
        ${src_dir}/libstdc++-v3/configure \
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
          --with-arch=armv6 \
          --srcdir=${src_dir}/libstdc++-v3

  RunWithLog "Make libstdc++" ${TMP}/llvm-gcc.make_libstdcpp.log \
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
  local tmpdir=${TMP}/llvm-gcc

  Banner "Building llvmgcc-stage2 in ${tmpdir}"

  SetupSourceTree ${tmpdir} \
                  llvm-gcc.nacl-llvm-branches \
                  ${LLVM_GCC_REV} \
                  ${LLVM_GCC_DEV}
  pushd ${tmpdir}

  # NOTE: you cannot build llvm-gcc inside the source directory
  mkdir -p build
  cd build
  # TODO(robertm): do we really need CROSS_TARGET_*
  RunWithLog "Configure" ${TMP}/llvm-gcc.configure.log \
      env -i PATH=/usr/bin/:/bin \
             CC=${CC} \
             CXX=${CXX} \
             CFLAGS="-Dinhibit_libc" \
             CXXFLAGS="-Dinhibit_libc" \
             "${STD_ENV_FOR_GCC_ETC[@]}" \
             ../llvm-gcc-4.2/configure \
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
               --with-arch=armv6 \
               --with-fpu=vfp \
               --with-sysroot=${NEWLIB_INSTALL_DIR}

 RunWithLog "Make" ${TMP}/llvm-gcc.make.log \
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
             make \
             "${STD_ENV_FOR_GCC_ETC[@]}" \
             ${MAKE_OPTS} all-gcc


  BuildLibgcc ${tmpdir}/build
  BuildLibiberty ${tmpdir}/build

  SubBanner "Partial compiler install for gcc"
  cd gcc
  cp gcc-cross ${LLVMGCC_INSTALL_DIR}/bin/llvm-gcc
  cp g++-cross ${LLVMGCC_INSTALL_DIR}/bin/llvm-g++
  # NOTE: the "cp" will fail when we upgrade to a more recent compiler,
  #       simply fix this version when this happens
  cp cc1 ${LLVMGCC_INSTALL_DIR}/libexec/gcc/${CROSS_TARGET}/4.2.1
  cp cc1plus ${LLVMGCC_INSTALL_DIR}/libexec/gcc/${CROSS_TARGET}/4.2.1

  BuildLibstdcpp ${tmpdir}/build
  popd
}


gcc-stage3() {
  local tmpdir=${TMP}/llvm-gcc

  Banner "Installing final version of llvmgcc (stage3)"

  pushd ${tmpdir}/build
  RunWithLog "Make" ${TMP}/llvm-gcc.install.log \
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
  local tmpdir="${TMP}/binutils.nacl"
  local tarball=$(readlink -f ../third_party/binutils/binutils-2.20.tar.bz2)

  Banner "Building binutils"

  SetupSourceTree ${tmpdir}/src \
                  binutils.nacl-llvm-branches \
                  ${BINUTILS_REV} \
                  ${BINUTILS_DEV}
  mkdir -p ${tmpdir}/build
  pushd ${tmpdir}/build

  # --enable-checking is to avoid a build failure:
  #   tc-arm.c:2489: warning: empty body in an if-statement
  # The --enable-gold and --enable-plugins options are on so that we
  # can use gold's support for plugin to link PNaCl modules.
  RunWithLog "Configuring binutils"  ${TMP}/binutils.configure.log \
    env -i \
    PATH="/usr/bin:/bin" \
    CC=${CC} \
    CXX=${CXX} \
    ../src/binutils-2.20/configure --prefix=${BINUTILS_INSTALL_DIR} \
                                   --target=${CROSS_TARGET} \
                                   --enable-checking \
                                   --enable-gold \
                                   --enable-plugins \
                                   --with-sysroot=${NEWLIB_INSTALL_DIR}

  RunWithLog "Make binutils" ${TMP}/binutils.make.log \
    env -i PATH="/usr/bin:/bin" \
    make ${MAKE_OPTS}

  RunWithLog "Install binutils"  ${TMP}/binutils.install.log \
    env -i PATH="/usr/bin:/bin" \
      make \
      install ${MAKE_OPTS}

  popd
}

BuildAndInstallNewlib() {
  local tmpdir=${TMP}/newlib
  # NOTE: When an argument is given to this function we abuse it to generate
  #       a bitcode version of the library. In the scenario we just want to
  #       build libc.a and do install or uninstall anything.
  if [ $# == 0 ] ; then
    rm -rf ${NEWLIB_INSTALL_DIR}
  elif [ ! -d "$1" ]; then
    echo "Error: no such directory '$1'." >&2
    exit 1
  fi
  Banner "building and installing newlib"

  pushd ${tmpdir}/newlib-1.17.0

  RunWithLog "Configuring newlib"  ${TMP}/newlib.configure.log \
    env -i \
    PATH="/usr/bin:/bin" \
    "${STD_ENV_FOR_NEWLIB[@]}" \
    ./configure \
        --disable-libgloss \
        --disable-multilib \
        --enable-newlib-reent-small \
        --prefix="${NEWLIB_INSTALL_DIR}" \
        --disable-newlib-supplied-syscalls \
        --disable-texinfo \
        --target="${CROSS_TARGET}"


  RunWithLog "Make newlib"  ${TMP}/newlib.make.log \
    env -i PATH="/usr/bin:/bin" \
    make \
      "${STD_ENV_FOR_NEWLIB[@]}" \
      ${MAKE_OPTS}


  if [ $# == 1 ] ; then
    SubBanner "installing lib[cgm].a to $1"
    cp ${tmpdir}/newlib-1.17.0/arm-none-linux-gnueabi/newlib/lib[cgm].a $1
    return
  fi

  RunWithLog "Install newlib"  ${TMP}/newlib.install.log \
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
#@ untrusted_sdk <tarball>
#@
#@   Create untrusted SDK tarball.
#@   This is the primary function of this script.
untrusted_sdk() {
  if [ ! -n "${1:-}" ]; then
    echo "Error: untrusted_sdk needs a tarball name." >&2
    exit 1
  fi
  mkdir -p ${TMP}
  PathSanityCheck
  ClearInstallDir
  RecordRevisionInfo
  BuildAndInstallBinutils
  llvm
  gcc-stage1
  driver
  gcc-stage2
  gcc-stage3
  BuildAndInstallNewlib
  extrasdk
  misc-tools
  examples
  prune
  tarball $1
}

#@
#@ llc-sfi
#@
#@   Build and install SFI llc.

#@
#@ llvm
#@
#@   Configure, build and install LLVM.

#@
#@ gcc-stage1
#@
#@   install pre-gcc
gcc-stage1() {
  SetupNewlibSource
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
  SetupNewlibSource
  BuildAndInstallNewlib
}

#@
#@ newlib-libonly <target-dir>
#@
#@   Build and install newlib.
newlib-libonly() {
  SetupNewlibSource
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
  tmpdir=${TMP}/libstdcpp

  # Extract llvm-gcc headers:
  SetupSourceTree ${tmpdir} \
                  llvm-gcc.nacl-llvm-branches \
                  ${LLVM_GCC_REV} \
                  ${LLVM_GCC_DEV}
  pushd ${tmpdir}
  mkdir -p build
  BuildLibstdcpp ${tmpdir}/build
  cp ${tmpdir}/build/${CROSS_TARGET}/libstdc++-v3/src/.libs/libstdc++.a ${dest}
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
