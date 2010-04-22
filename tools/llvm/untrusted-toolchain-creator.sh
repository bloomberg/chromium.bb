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

readonly CS_URL=http://www.codesourcery.com/sgpp/lite/arm/portal/package1787/\
public/arm-none-linux-gnueabi/\
arm-2007q3-51-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2


readonly INSTALL_ROOT=$(pwd)/toolchain/linux_arm-untrusted
readonly CROSS_TARGET=arm-none-linux-gnueabi
readonly LLVM_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}/llvm"
readonly LLVMGCC_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}/llvm-gcc-4.2"
# NOTE: NEWLIB_INSTALL_DIR also server as a SYSROOT
readonly NEWLIB_INSTALL_DIR="${INSTALL_ROOT}/arm-newlib"
readonly DRIVER_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}"
readonly CODE_SOURCERY_ROOT="${INSTALL_ROOT}/codesourcery/arm-2007q3"

# TODO(robertm): get the code from a repo rather than use tarball + patch
readonly LLVMGCC_TARBALL=$(pwd)/../third_party/llvm/llvm-gcc-4.2-88663.tar.bz2
# TODO(robertm): move these two into a proper patch when move to hg repo
readonly LLVMGCC_SFI_PATCH=$(pwd)/tools/patches/libgcc-arm-lib1funcs.patch
readonly LLVMGCC_SFI_PATCH2=$(pwd)/tools/patches/libgcc-arm-libunwind.patch

# TODO(robertm): get the code from a repo rather than use tarball + patch
readonly LLVM_TARBALL=$(pwd)/../third_party/llvm/llvm-88663.tar.bz2
readonly LLVM_SFI_PATCH=$(pwd)/tools/patches/llvm-r88663.patch

# TODO(robertm): get the code from a repo rather than use tarball + patch
readonly NEWLIB_TARBALL=$(pwd)/../third_party/newlib/newlib-1.17.0.tar.gz
readonly NEWLIB_PATCH=$(pwd)/tools/patches/newlib-1.17.0_arm.patch


readonly MAKE_OPTS="-j6 VERBOSE=1"

readonly TMP=/tmp/crosstool-untrusted


export CODE_SOURCERY_PKG_PATH=${INSTALL_ROOT}/codesourcery

# These are simple compiler wrappers to force 32bit builds
readonly  CC32=$(readlink -f tools/llvm/mygcc32)
readonly  CXX32=$(readlink -f tools/llvm/myg++32)

readonly CROSS_TARGET_AR=${CODE_SOURCERY_ROOT}/bin/${CROSS_TARGET}-ar
readonly CROSS_TARGET_AS=${CODE_SOURCERY_ROOT}/bin/${CROSS_TARGET}-as
readonly CROSS_TARGET_LD=${CODE_SOURCERY_ROOT}/bin/${CROSS_TARGET}-ld
readonly CROSS_TARGET_NM=${CODE_SOURCERY_ROOT}/bin/${CROSS_TARGET}-nm
readonly CROSS_TARGET_RANLIB=${CODE_SOURCERY_ROOT}/bin/${CROSS_TARGET}-ranlib
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


RecordRevisionInfo() {
  svn info >  ${INSTALL_ROOT}/REV
}


CreateTarBall() {
  local tarball=$1
  Banner "creating tar ball ${tarball}"
  tar zcf ${tarball} -C ${INSTALL_ROOT} .
}


# try to keep the tarball small
PruneDirs() {
  Banner "pruning code sourcery tree"
  local CS_ROOT=${INSTALL_ROOT}/codesourcery/arm-2007q3
  SubBanner "Size before: $(du -msc  ${CS_ROOT})"
  rm -rf ${CS_ROOT}/share
  rm -rf ${CS_ROOT}/libexec
  # only delete cc,c++,g++,gdb,gcov
  rm -f ${CS_ROOT}/bin/arm-none-linux-gnueabi-[cg]*
  rm -rf ${CS_ROOT}/lib
  rm -rf ${CS_ROOT}/include
  rm -rf ${CS_ROOT}/arm-none-linux-gnueabi/libc
  rm -rf ${CS_ROOT}/arm-none-linux-gnueabi/include
  rm -rf ${CS_ROOT}/arm-none-linux-gnueabi/lib

  SubBanner "Size after: $(du -msc  ${CS_ROOT})"

  Banner "pruning llvm sourcery tree"
  local LLVM_ROOT=${INSTALL_ROOT}/arm-none-linux-gnueabi
  SubBanner "Size before: $(du -msc  ${LLVM_ROOT})"
  rm  -f ${LLVM_ROOT}/llvm/lib/lib*.a
  SubBanner "Size after: $(du -msc  ${LLVM_ROOT})"
}

# we mostly need the cross binutils from this toolchain
DownloadOrCopyCodeSourceryTarballAndInstall() {
  Banner "Install codesourcery toolchain in ${CODE_SOURCERY_ROOT}"
  mkdir -p ${CODE_SOURCERY_ROOT}
  local tarball="${TMP}/${CS_URL##*/}"
  DownloadOrCopy ${CS_URL} ${tarball}
  Run "Untaring" tar jxf ${tarball} -C ${CODE_SOURCERY_ROOT}/..
  du -msc ${CODE_SOURCERY_ROOT}
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
             MAKE_OPTS=${MAKE_OPTS} \
             CC=${CC32} \
             CXX=${CXX32} \
             ./configure \
             --disable-jit \
             --enable-optimized \
             --enable-targets=x86,x86_64,arm \
             --target=arm-none-linux-gnueabi \
             --prefix=${LLVM_INSTALL_DIR} \
             --with-llvmgccdir=${LLVMGCC_INSTALL_DIR}

  RunWithLog "Make" ${TMP}/llvm.make.log \
    env -i PATH=/usr/bin/:/bin \
           MAKE_OPTS=${MAKE_OPTS} \
           CC=${CC32} \
           CXX=${CXX32} \
           make ${MAKE_OPTS} all

  RunWithLog "Installing LLVM" ${TMP}/llvm-install.log \
       make ${MAKE_OPTS} install

  cd ${saved_dir}
}


# UntarAndPatchNewLib
UntarAndPatchNewlib() {
  local saved_dir=$(pwd)
  local tmpdir=${TMP}/newlib
  Banner "Untaring and patching newlib in ${tmpdir}"
  rm -rf ${tmpdir}
  mkdir -p ${tmpdir}
  cd ${tmpdir}

  Run "Untaring newlib [${NEWLIB_TARBALL}]" tar zxf ${NEWLIB_TARBALL}
  cd newlib-1.17.0
  SubBanner "patching newlib"
  # remove -fshort-enums
  readonly patch1="newlib/libc/stdio/Makefile.am"
  mv ${patch1} ${patch1}.orig
  sed -e 's/-fshort-enums//' < ${patch1}.orig >  ${patch1}
  echo ${patch1}

  readonly patch2="newlib/libc/stdio/Makefile.in"
  mv ${patch2} ${patch2}.orig
  sed -e 's/-fshort-enums//' < ${patch2}.orig >  ${patch2}
  echo ${patch2}

  # replace setjmp assembler code with an empty file
  readonly patch3="newlib/libc/machine/arm/Makefile.in"
  mv ${patch3} ${patch3}.orig
  sed -e 's/setjmp.S/setjmp.c/g'  < ${patch3}.orig >  ${patch3}
  touch newlib/libc/machine/arm/setjmp.c
  echo ${patch3}

  SubBanner "more patching newlib"
  # patch some more files using the traditional patch tool
  patch -p1 < ${NEWLIB_PATCH}

  SubBanner "delete syscalls"
  # remove newlib functions - we have our own dummy
  for s in newlib/libc/syscalls/*.c ; do
    echo $s
    mv ${s} ${s}.orig
    touch ${s}
  done

  cd ${saved_dir}
}

# Note: depends on newlib being untared and patched
SetupSysRoot() {
  local sys_include=${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/include
  local sys_include2=${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/sys-include
  SubBanner "Setting up initial sysroot in ${sys_include}"

  rm -rf ${sys_include} ${sys_include2}
  mkdir -p ${sys_include}
  ln -sf ${sys_include} ${sys_include2}
  cp -r ${TMP}/newlib/newlib-1.17.0/newlib/libc/include/* ${sys_include}
  # TODO(robertm): is this really necessary at this time?
  src/trusted/service_runtime/export_header.py \
       src/trusted/service_runtime/include ${sys_include}
}


# Build "pregcc" which is a gcc that does not depend on having glibc/newlib
# already compiled. This also generates some important headers (confirm this).
#
# NOTE: depends on newlib being untared and patched so we can use it to setup a
#       sysroot
ConfigureAndBuildGccStage1() {
  local tmpdir=${TMP}/llvm-pregcc
  local saved_dir=$(pwd)

  Banner "Building llvmgcc-stage1 in ${tmpdir}"

  SetupSysRoot

  rm -rf ${tmpdir}
  mkdir -p ${tmpdir}
  cd ${tmpdir}

  Run "Untaring llvm-gcc" \
    tar jxf ${LLVMGCC_TARBALL}

  Run "Patching" \
    patch llvm-gcc-4.2/gcc/config/arm/lib1funcs.asm ${LLVMGCC_SFI_PATCH}

  Run "Patching2" \
    patch llvm-gcc-4.2/gcc/config/arm/libunwind.S ${LLVMGCC_SFI_PATCH2}

  # NOTE: you cannot build llvm-gcc inside the source directory
  mkdir -p build
  cd build
  # TODO(robertm): do we really need CROSS_TARGET_*
  RunWithLog "Configure" ${TMP}/llvm-pregcc.configure.log \
      env -i PATH=/usr/bin/:/bin \
             CC=${CC32} \
             CXX=${CXX32} \
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
               --with-as=${CROSS_TARGET_AS} \
               --with-ld=${CROSS_TARGET_LD}

 # NOTE: we add ${CODE_SOURCERY_ROOT}/bin to PATH
 RunWithLog "Make" ${TMP}/llvm-pregcc.make.log \
      env -i PATH=/usr/bin/:/bin:${CODE_SOURCERY_ROOT}/bin \
             CC=${CC32} \
             CXX=${CXX32} \
             CFLAGS="-Dinhibit_libc" \
             make ${MAKE_OPTS} all

 # NOTE: we add ${CODE_SOURCERY_ROOT}/bin to PATH
 RunWithLog "Install" ${TMP}/llvm-pregcc.install.log \
      env -i PATH=/usr/bin/:/bin:${CODE_SOURCERY_ROOT}/bin \
             CC=${CC32} \
             CXX=${CXX32} \
             CFLAGS="-Dinhibit_libc" \
             make ${MAKE_OPTS} install

  cd ${saved_dir}
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

  cd ${tmpdir}/gcc

  Run "Build libgcc clean"\
      env -i PATH=/usr/bin/:/bin:${CODE_SOURCERY_ROOT}/bin \
             make clean-target-libgcc

  RunWithLog "Build libgcc" ${TMP}/llvm-gcc.make_libgcc.log \
      env -i PATH=/usr/bin/:/bin:${CODE_SOURCERY_ROOT}/bin \
             make \
             "${STD_ENV_FOR_GCC_ETC[@]}" \
             ${MAKE_OPTS} libgcc.a
}


BuildLibiberty() {
  local tmpdir=$1

  cd ${tmpdir}
  # maybe clean libiberty first
  RunWithLog "Build libiberty" ${TMP}/llvm-gcc.make_libiberty.log \
      env -i PATH=/usr/bin/:/bin:${CODE_SOURCERY_ROOT}/bin \
             make \
             "${STD_ENV_FOR_GCC_ETC[@]}" \
             ${MAKE_OPTS} all-target-libiberty
}


BuildLibstdcpp() {
  local tmpdir=$1
  cd ${tmpdir}
  local src_dir=${tmpdir}/../llvm-gcc-4.2
  local cpp_build_dir=${tmpdir}/${CROSS_TARGET}/libstdc++-v3
  rm -rf ${cpp_build_dir}
  mkdir -p  ${cpp_build_dir}
  cd ${cpp_build_dir}

  RunWithLog "Configure libstdc++" ${TMP}/llvm-gcc.configure_libstdcpp.log \
      env -i PATH=/usr/bin/:/bin:${CODE_SOURCERY_ROOT}/bin \
        CC="${CC_FOR_SFI_TARGET}" \
        CXX="${CXX_FOR_SFI_TARGET}" \
        RAW_CXX_FOR_TARGET="${CXX_FOR_SFI_TARGET}" \
        LD="${LD_FOR_SFI_TARGET}" \
        CFLAGS="${CXXFLAGS_FOR_SFI_TARGET}" \
        CPPFLAGS="${CXXFLAGS_FOR_SFI_TARGET}" \
        CXXFLAGS="${CXXFLAGS_FOR_SFI_TARGET}" \
        CFLAGS_FOR_TARGET="${CFLAGS_FOR_SFI_TARGET}" \
        CPPFLAGS_FOR_TARGET="${CXXFLAGS_FOR_SFI_TARGET}" \
        CC_FOR_TARGET="${CC_FOR_SFI_TARGET}" \
        GCC_FOR_TARGET="${CC_FOR_SFI_TARGET}" \
        CXX_FOR_TARGET="${CXX_FOR_SFI_TARGET}" \
        AS_FOR_TARGET="${CROSS_TARGET_AS}" \
        LD_FOR_TARGET="${LD_FOR_SFI_TARGET}" \
        AR_FOR_TARGET="${CROSS_TARGET_AR}" \
        NM_FOR_TARGET="${CROSS_TARGET_NM}" \
        OBJDUMP_FOR_TARGET="${ILLEGAL_TOOL}" \
        RANLIB_FOR_TARGET="${CROSS_TARGET_RANLIB}" \
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
          --with-as=${CROSS_TARGET_AS} \
          --with-ld=${CROSS_TARGET_LD} \
          --srcdir=${src_dir}/libstdc++-v3

  RunWithLog "Make libstdc++" ${TMP}/llvm-gcc.make_libstdcpp.log \
    env -i PATH=/usr/bin/:/bin:${CODE_SOURCERY_ROOT}/bin \
        make  ${MAKE_OPTS}

#  RunWithLog "Install libstdc++" ${TMP}/llvm-gcc.install_libstdcpp.log \
#    env -i PATH=/usr/bin/:/bin:${CODE_SOURCERY_ROOT}/bin \
#      make  ${MAKE_OPTS} install
}

# Build gcc again in order to build libgcc and other essential libs
# Note: depends on
ConfigureAndBuildGccStage2() {
  local tmpdir=${TMP}/llvm-gcc
  local saved_dir=$(pwd)

  Banner "Building llvmgcc-stage2 in ${tmpdir}"

  rm -rf ${tmpdir}
  mkdir -p ${tmpdir}
  cd ${tmpdir}

  Run "Untaring llvm-gcc" \
    tar jxf ${LLVMGCC_TARBALL}

  Run "Patching" \
    patch  llvm-gcc-4.2/gcc/config/arm/lib1funcs.asm ${LLVMGCC_SFI_PATCH}

  Run "Patching2" \
    patch llvm-gcc-4.2/gcc/config/arm/libunwind.S ${LLVMGCC_SFI_PATCH2}

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
               --with-sysroot=${NEWLIB_INSTALL_DIR} \
               --with-as=${CROSS_TARGET_AS} \
               --with-ld=${CROSS_TARGET_LD}

 RunWithLog "Make" ${TMP}/llvm-gcc.make.log \
      env -i PATH=/usr/bin/:/bin:${CODE_SOURCERY_ROOT}/bin \
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
  cd ${saved_dir}
}


ConfigureAndBuildGccStage3() {
  local tmpdir=${TMP}/llvm-gcc
  local saved_dir=$(pwd)

  Banner "Installing final version of llvmgcc (stage3)"
  SubBanner "clean install dirs"
  for dir in bin include info lib lib32 libexec man share ; do
    echo "cleaning ${LLVMGCC_INSTALL_DIR}/${dir}"
    rm -rf ${LLVMGCC_INSTALL_DIR}/${dir}
  done

  cd ${tmpdir}/build
  RunWithLog "Make" ${TMP}/llvm-gcc.install.log \
      env -i PATH=/usr/bin/:/bin:${CODE_SOURCERY_ROOT}/bin \
             make \
             "${STD_ENV_FOR_GCC_ETC[@]}" \
             install


  SubBanner "Install libgcc.a library"
  mkdir -p  ${NEWLIB_INSTALL_DIR}/usr/lib
  cp gcc/libgcc.a ${NEWLIB_INSTALL_DIR}/usr/lib
  # NOTE: the "cp" will fail when we upgrade to a more recent compiler,
  #       simply fix this version when this happens
  cp gcc/libg*.a ${LLVMGCC_INSTALL_DIR}/lib/gcc/${CROSS_TARGET}/4.2.1/

  SubBanner "Install libiberty.a"
  mkdir -p  ${NEWLIB_INSTALL_DIR}/usr/lib
  cp libiberty/libiberty.a ${NEWLIB_INSTALL_DIR}/usr/lib
  cp libiberty/libiberty.a ${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/lib

  SubBanner "Install libstdc++, header massaging"
  echo "headers"
  cp -r ${LLVMGCC_INSTALL_DIR}/include/c++/ ${NEWLIB_INSTALL_DIR}/usr/include/
  # Don't ask
  rm -f ${LLVMGCC_INSTALL_DIR}/arm-none-linux-gnueabi/include/c++
  ln -s ../../include/c++ \
        ${LLVMGCC_INSTALL_DIR}/arm-none-linux-gnueabi/include/c++
  echo "libs"
  cp  ${LLVMGCC_INSTALL_DIR}/lib/lib*.a  ${NEWLIB_INSTALL_DIR}/usr/lib


  cd ${saved_dir}
}

# Build a sfi version of llvm's llc backend
# The mygcc32 and myg++32 trickery ensures that all binaries
# are statically linked and 32-bit.
UntarPatchConfigureAndBuildSfiLlc() {
  local saved_dir=$(pwd)
  local tmpdir=${TMP}/llvm.sfi
  Banner "Building sfi lcc in ${tmpdir}"
  rm -rf ${tmpdir}
  mkdir ${tmpdir}
  cd ${tmpdir}

  Run "Untaring" tar jxf  ${LLVM_TARBALL}
  cd llvm

  Run "Patching" patch -p2 < ${LLVM_SFI_PATCH}

  RunWithLog "Configure" ${TMP}/llvm.sfi.configure.log\
      env -i PATH=/usr/bin/:/bin \
             MAKE_OPTS=${MAKE_OPTS} \
             CC=${CC32} \
             CXX=${CXX32} \
             ./configure \
             --disable-jit \
             --enable-optimized \
             --enable-targets=x86,x86_64,arm \
             --target=arm-none-linux-gnueabi

  RunWithLog "Make" ${TMP}/llvm.sfi.make.log \
      env -i PATH=/usr/bin/:/bin \
             MAKE_OPTS=${MAKE_OPTS} \
             CC=${CC32} \
             CXX=${CXX32} \
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


# we copy some useful tools after building them first
InstallMiscTools() {
   Banner "building and installing misc tools"

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
   cp scons-out/opt-linux-x86-32-to-arm/obj/src/trusted/validator_arm/v2/\
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
           cppas-arm cppas-x86-32 \
           sfild bcld-arm bcld-x86-32 \
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

#
BuildAndInstallNewlib() {
  local saved_dir=$(pwd)
  local tmpdir=${TMP}/newlib
  rm -rf ${NEWLIB_INSTALL_DIR}
  Banner "building and installing newlib"

  cd ${tmpdir}/newlib-1.17.0

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
    cp ${tmpdir}//newlib-1.17.0/arm-none-linux-gnueabi/newlib/lib[cgm].a $1
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

  mkdir -p ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/usr
  cp -rf ${sys_lib}  ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/usr/
  cp -rf ${sys_include}  ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/usr/

  cp -r ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/lib/* ${sys_lib}

  # NOTE: we provide our own pthread.h via extra-sdk
  p1="${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/usr/include/pthread.h"
  p2="${sys_include}/pthread.h"
  for p in $p1 $p2  ; do
    echo "remove bad pthread: $p"
    rm -f $p
  done


  cd ${saved_dir}
}


BuildExtraSDK() {
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


InstallExamples() {
  Banner "installing examples into ${INSTALL_ROOT}/examples"
  rm -rf  ${INSTALL_ROOT}/examples/
  cp -r  tools/llvm/arm_examples ${INSTALL_ROOT}/examples/
}


# NOTE: Experiment x86-32 support
AddX86Basics32() {
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
  RecordRevisionInfo

  DownloadOrCopyCodeSourceryTarballAndInstall
  ConfigureAndBuildLlvm
  UntarPatchConfigureAndBuildSfiLlc

  UntarAndPatchNewlib
  ConfigureAndBuildGccStage1

  InstallUntrustedLinkerScript
  InstallDriver

  ConfigureAndBuildGccStage2
  ConfigureAndBuildGccStage3


  BuildAndInstallNewlib

  BuildExtraSDK

  InstallMiscTools

  InstallExamples

  PruneDirs

  CreateTarBall $1

  exit 0
fi

#@
#@ install-cs
#@
#@   download and install codesourcery toolchain
if [ ${MODE} = 'install-cs' ] ; then
  DownloadOrCopyCodeSourceryTarballAndInstall
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
#@ llvm
#@
#@   install llvm
if [ ${MODE} = 'llvm' ] ; then
  ConfigureAndBuildLlvm
  exit 0
fi

#@
#@ gcc-stage1
#@
#@   install pre-gcc
if [ ${MODE} = 'gcc-stage1' ] ; then
  UntarAndPatchNewlib
  ConfigureAndBuildGccStage1
  exit 0
fi

#@
#@ gcc-stage2
#@
#@   NOTE: depends on installation of gcc-stage1
#@   build libgcc, libiberty, libstc++
if [ ${MODE} = 'gcc-stage2' ] ; then
  ConfigureAndBuildGccStage2
  exit 0
fi

#@
#@ gcc-stage3
#@
#@   NOTE: depends on installation of gcc-stage2
if [ ${MODE} = 'gcc-stage3' ] ; then
  ConfigureAndBuildGccStage3
  exit 0
fi

#@
#@ newlib
#@
#@   build and install newlib
if [ ${MODE} = 'newlib' ] ; then
  UntarAndPatchNewlib
  BuildAndInstallNewlib
  exit 0
fi

#@
#@ newlib-libonly <target-dir>
#@
#@   build and install newlib
if [ ${MODE} = 'newlib-libonly' ] ; then
  UntarAndPatchNewlib
  BuildAndInstallNewlib $1
  exit 0
fi


#@
#@ extrasdk
#@
#@   build and install extra sdk libs and headers
if [ ${MODE} = 'extrasdk' ] ; then
  BuildExtraSDK
  exit 0
fi

#@
#@ misc-tools
#@
#@   install misc tools
if [ ${MODE} = 'misc-tools' ] ; then
  InstallMiscTools
  exit 0
fi

#@
#@ driver
#@
#@   install driver
if [ ${MODE} = 'driver' ] ; then
  InstallUntrustedLinkerScript
  InstallDriver
  exit 0
fi


#@
#@ driver-symlink
#@
#@   Install driver as a symlink into the client
#@   so that driver development is simplified.
#@   NOTE: This will make the INSTALL_ROOT unsuitable
#@         for being tar'ed up as a self contained toolchain.
if [ ${MODE} = 'driver-symlink' ] ; then
  abs_path=$(readlink -f tools/llvm/llvm-fake.py)
  driver=${INSTALL_ROOT}/arm-none-linux-gnueabi/llvm-fake.py
  ln -sf ${abs_path} ${driver}
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
#@ add-x86-basics-32
#@
#@   add x86 basic libs from
if [ ${MODE} = 'add-x86-basics-32' ] ; then
  AddX86Basics32
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

echo "ERROR: unknown mode ${MODE}"
exit -1
