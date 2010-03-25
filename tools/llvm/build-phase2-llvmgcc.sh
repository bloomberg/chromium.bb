#!/bin/bash
#
# Compiles and installs a Linux/x86_64 -> Linux/ARM crosstool based on LLVM and
# LLVM-GCC-4.2 using SVN snapshots in provided tarballs.

# This script is derived from build-install-linux.sh from the LLVM project.
# The script was originally written by Google employee Misha Brukman, and
# copyright was subsequently transferred to LLVM.

set -o nounset
set -o errexit

echo -n "Welcome to LLVM Linux/X86_64 -> Linux/ARM crosstool "
echo "builder/installer; some steps will require sudo privileges."

readonly PATCH_ROOT="$(pwd)/tools/patches"

readonly INSTALL_ROOT="${INSTALL_ROOT}"
# Both $USER and root *must* have read/write access to this dir.
readonly SCRATCH_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/llvm-phase2.XXXXXX")
# NOTE: use this instead for a more deterministic behavior
#readonly SCRATCH_ROOT=/tmp/llvm-phase2.XXXXXX
readonly SRC_ROOT="${SCRATCH_ROOT}/src"
readonly OBJ_ROOT="${SCRATCH_ROOT}/obj"

readonly CROSS_HOST="x86_64-unknown-linux-gnu"
readonly CROSS_TARGET="arm-none-linux-gnueabi"
readonly CROSS_MARCH="${CROSS_MARCH:-armv6}"

readonly CODE_SOURCERY="${INSTALL_ROOT}/codesourcery"
readonly CODE_SOURCERY_PKG_PATH="${CODE_SOURCERY_PKG_PATH:-/tmp/codesourcery}"
readonly CODE_SOURCERY_REPO="http://www.codesourcery.com/sgpp/lite/arm/portal"
readonly CODE_SOURCERY_HTTP="${CODE_SOURCERY_REPO}/package1787/public"
readonly CODE_SOURCERY_PLATFORM="arm-none-linux-gnueabi-i686-pc-linux-gnu"
readonly CODE_SOURCERY_PKG="arm-2007q3-51-${CODE_SOURCERY_PLATFORM}.tar.bz2"
readonly CODE_SOURCERY_ROOT="${CODE_SOURCERY}/arm-2007q3"
readonly CODE_SOURCERY_BIN="${CODE_SOURCERY_ROOT}/bin"
# Make sure ${CROSS_TARGET}-* binutils are in command path
export PATH="${CODE_SOURCERY_BIN}:${INSTALL_ROOT}/${CROSS_TARGET}:${PATH}"

readonly CROSS_TARGET_AS="${CODE_SOURCERY_BIN}/${CROSS_TARGET}-as"
readonly CROSS_TARGET_LD="${CODE_SOURCERY_BIN}/${CROSS_TARGET}-ld"

readonly SYSROOT="${CODE_SOURCERY_ROOT}/${CROSS_TARGET}/libc"

readonly LLVM_PKG_PATH="${LLVM_PKG_PATH:-${HOME}/llvm-project/snapshots}"

# Latest SVN revisions known to be working in this configuration.
readonly LLVM_DEFAULT_REV="74530"
readonly LLVMGCC_DEFAULT_REV="74535"

readonly LLVM_PKG="llvm-${LLVM_SVN_REV:-${LLVM_DEFAULT_REV}}.tar.bz2"
readonly LLVM_SRC_DIR="${SRC_ROOT}/llvm"
readonly LLVM_OBJ_DIR="${OBJ_ROOT}/llvm"
readonly LLVM_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}/llvm"

readonly LLVMGCC_PKG="llvm-gcc-4.2-${LLVMGCC_SVN_REV:-${LLVMGCC_DEFAULT_REV}}.tar.bz2"
readonly LLVMGCC_SRC_DIR="${SRC_ROOT}/llvm-gcc-4.2"
readonly LLVMGCC_OBJ_DIR="${OBJ_ROOT}/llvm-gcc-4.2"
readonly LLVMGCC_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}/llvm-gcc-4.2"

readonly MAKE_OPTS="${MAKE_OPTS:--j2}"

# Params:
#   $1: directory to be created
#   $2: optional mkdir command prefix, e.g. "sudo"
createDir() {
  if [[ ! -e $1 ]]; then
    ${2:-} mkdir -p $1
  elif [[ -e $1 && ! -d $1 ]]; then
    echo "$1 exists but is not a directory; exiting."
    exit 3
  fi
}

# NOTE: these and other functions were copied from toolchain-creator.sh
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

buildLLVM() {
  Banner "buildLLVM"
  # Unpack LLVM tarball; should create the directory "llvm".
  cd ${SRC_ROOT}
  Run "Unpacking LLVM" tar jxf ${LLVM_PKG_PATH}/${LLVM_PKG}

  # Configure, build, and install LLVM.
  createDir ${LLVM_OBJ_DIR}
  cd ${LLVM_OBJ_DIR}
  RunWithLog "Configuring LLVM" ${LLVM_OBJ_DIR}/llvm-configure.log \
      ${LLVM_SRC_DIR}/configure \
      --disable-jit \
      --enable-targets=x86,x86_64,arm \
      --enable-optimized \
      --prefix=${LLVM_INSTALL_DIR} \
      --target=${CROSS_TARGET} \
      --with-llvmgccdir=${LLVMGCC_INSTALL_DIR}

  RunWithLog "Building LLVM" ${LLVM_OBJ_DIR}/llvm-build.log \
      make ${MAKE_OPTS}
}

installLLVMGCC() {
  Banner "installLLVMGCC"
  # Unpack LLVM-GCC tarball; should create the directory "llvm-gcc-4.2".
  cd ${SRC_ROOT}

  Run "Unpacking LLVM-GCC" \
    tar jxf ${LLVM_PKG_PATH}/${LLVMGCC_PKG}

  Run "Patching libgcc sources" \
    patch ${LLVMGCC_SRC_DIR}/gcc/config/arm/lib1funcs.asm \
    ${PATCH_ROOT}/libgcc-arm-lib1funcs.patch

  # Configure, build, and install LLVM-GCC.
  createDir ${LLVMGCC_OBJ_DIR}
  cd ${LLVMGCC_OBJ_DIR}
  RunWithLog "Configuring LLVM-GCC" ${LLVMGCC_OBJ_DIR}/llvmgcc-configure.log \
      ${LLVMGCC_SRC_DIR}/configure \
      --disable-decimal-float \
      --disable-libssp \
      --disable-libgomp \
      --disable-libstdcxx-pch \
      --disable-shared \
      --enable-languages=c,c++ \
      --enable-llvm=${LLVM_INSTALL_DIR} \
      --prefix=${LLVMGCC_INSTALL_DIR} \
      --program-prefix=llvm- \
      --target=${CROSS_TARGET} \
      --with-arch=${CROSS_MARCH} \
      --with-as=${CROSS_TARGET_AS} \
      --with-ld=${CROSS_TARGET_LD} \
      --with-sysroot=${SYSROOT}

      # TODO(robertm) try these options:
      # --without-headers
      # --with-newlib

  # LOCALMOD: this env var dance forces use of our compiler driver.
  RunWithLog "Building LLVM-GCC" ${LLVMGCC_OBJ_DIR}/llvmgcc-build.log \
      make CC_FOR_TARGET=llvm-fake-sfigcc \
           CXX_FOR_TARGET=llvm-fake-sfig++ \
           GCC_FOR_TARGET=llvm-fake-sfigcc \
           CFLAGS=-O2 \
           all-gcc

  # NOTE: there is no separate libgcc_eh.a as we build with  --disable-share
  #       libgcc.a DOES contain all the relevant symbols, though
  SubBanner "Installing libgcc"
  mkdir -p  ${INSTALL_ROOT}/armsfi-lib
  cp ${SCRATCH_ROOT}/obj/llvm-gcc-4.2/gcc/libgcc.a  ${INSTALL_ROOT}/armsfi-lib


#    TODO(robertm): reconsider this when switching to recent version of gcc
#   RunWithLog "Building LLVM-GCC-LIBGCC" ${LLVMGCC_OBJ_DIR}/llvmgcc_libgcc-build.log \
#       make CC_FOR_TARGET=llvm-fake-sfigcc \
#            CXX_FOR_TARGET=llvm-fake-sfig++ \
#            GCC_FOR_TARGET=llvm-fake-sfigcc \
#            CFLAGS=-O2 \
#            all-target-libgcc

  # NOTE: TODO(robertm): needs more work
  # the following make invocation seems promising:
  #     make all-target-libstdc++-v3
  # Also: look at tools/Makefile for what we do on x86
  Banner "Skipping libstdc++"
  return
  # LOCALMOD: the full set of C/C++ compiler envvars isn't propagated into the
  # libstdc++ build, so we get to do it all over again.
  cd arm-none-linux-gnueabi/libstdc++-v3
  RunWithLog "Configuring libstdc++" ${LLVMGCC_OBJ_DIR}/libstdc++-configure.log \
      ${LLVMGCC_SRC_DIR}/libstdc++-v3/configure \
      --host=arm-none-linux-gnueabi \
      --target=arm-none-linux-gnueabi \
      --with-cross-host=x86_64-unknown-linux-gnu \
      --enable-llvm=${LLVM_OBJ_DIR} \
      --prefix=${LLVMGCC_INSTALL_DIR} \
      --with-sysroot=${SYSROOT} \
      --with-cpu=cortex-a8 \
      --disable-thumb \
      --disable-interwork \
      --disable-multilib \
      --with-mode=arm \
      --enable-languages=c,c++ \
      --program-transform-name='s,^,llvm-,' \
      --with-target-subdir=arm-none-linux-gnueabi \
      --srcdir=${LLVMGCC_SRC_DIR}/libstdc++-v3

  RunWithLog "Building libstdc++" ${LLVMGCC_OBJ_DIR}/libstdc++-build.log \
      make CC=llvm-fake-sfigcc CXX=llvm-fake-sfig++ CFLAGS=-O2

  SubBanner "Remove any previous versions to ensure we install"
  cd ${INSTALL_ROOT}-untrusted/
  local LIBDIR="arm-none-linux-gnueabi/llvm-gcc-4.2/"
  rm ${LIBDIR}/arm-none-linux-gnueabi/lib/libstdc++{.a,.so,.la,.so.6.0.9,.so.6}
  rm ${LIBDIR}/arm-none-linux-gnueabi/lib/{libgcc_s.so.1,libgcc_s.so}
  rm ${LIBDIR}/lib/gcc/arm-none-linux-gnueabi/4.2.1/libgcc{_eh,}.a
  cd -
  cd ../..
  RunWithLog "Installing phase2 gcc output" ${LLVMGCC_OBJ_DIR}/install.log \
      make install
}

echo "Building in ${SCRATCH_ROOT}; installing in ${INSTALL_ROOT}"

createDir ${SRC_ROOT}
createDir ${OBJ_ROOT}

buildLLVM
installLLVMGCC

# NOTE: for now we leave the dir around when a failure occurs
SubBanner "Cleaning up"
rm -rf ${SCRATCH_ROOT}

echo "Done."
