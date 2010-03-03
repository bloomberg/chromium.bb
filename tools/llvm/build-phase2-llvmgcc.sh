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

readonly INSTALL_ROOT="${INSTALL_ROOT:-/usr/local/crosstool}"
# Both $USER and root *must* have read/write access to this dir.
readonly SCRATCH_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/llvm-project.XXXXXX")
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

# Prints out and runs the command, but without logging -- intended for use with
# lightweight commands that don't have useful output to parse, e.g. mkdir, tar,
# etc.
runCommand() {
  local message="$1"
  shift
  echo "=> $message"
  echo "==> Running: $*"
  $*
}

runAndLog() {
  local message="$1"
  local log_file="$2"
  shift 2
  echo "=> $message; log in $log_file"
  echo "==> Running: $*"
  # Pop-up a terminal with the output of the current command?
  # e.g.: xterm -e /bin/bash -c "$* >| tee $log_file"
  $* &> $log_file
  if [[ $? != 0 ]]; then
    echo "Error occurred: see most recent log file for details."
    exit
  fi
}

buildLLVM() {
  # Unpack LLVM tarball; should create the directory "llvm".
  cd ${SRC_ROOT}
  runCommand "Unpacking LLVM" tar jxf ${LLVM_PKG_PATH}/${LLVM_PKG}

  # Configure, build, and install LLVM.
  createDir ${LLVM_OBJ_DIR}
  cd ${LLVM_OBJ_DIR}
  runAndLog "Configuring LLVM" ${LLVM_OBJ_DIR}/llvm-configure.log \
      ${LLVM_SRC_DIR}/configure \
      --disable-jit \
      --enable-optimized \
      --prefix=${LLVM_INSTALL_DIR} \
      --target=${CROSS_TARGET} \
      --with-llvmgccdir=${LLVMGCC_INSTALL_DIR}
  runAndLog "Building LLVM" ${LLVM_OBJ_DIR}/llvm-build.log \
      make ${MAKE_OPTS}
}

installLLVMGCC() {
  # Unpack LLVM-GCC tarball; should create the directory "llvm-gcc-4.2".
  cd ${SRC_ROOT}
  runCommand "Unpacking LLVM-GCC" tar jxf ${LLVM_PKG_PATH}/${LLVMGCC_PKG}

  runCommand "Patching libgcc sources" \
    patch ${LLVMGCC_SRC_DIR}/gcc/config/arm/lib1funcs.asm \
          ${PATCH_ROOT}/libgcc-arm-lib1funcs.patch

  # Configure, build, and install LLVM-GCC.
  createDir ${LLVMGCC_OBJ_DIR}
  cd ${LLVMGCC_OBJ_DIR}
  runAndLog "Configuring LLVM-GCC" ${LLVMGCC_OBJ_DIR}/llvmgcc-configure.log \
      ${LLVMGCC_SRC_DIR}/configure \
      --enable-languages=c,c++ \
      --enable-llvm=${LLVM_INSTALL_DIR} \
      --prefix=${LLVMGCC_INSTALL_DIR} \
      --program-prefix=llvm- \
      --target=${CROSS_TARGET} \
      --with-arch=${CROSS_MARCH} \
      --with-as=${CROSS_TARGET_AS} \
      --with-ld=${CROSS_TARGET_LD} \
      --with-sysroot=${SYSROOT}
  # LOCALMOD: this envvar dance forces use of our compiler driver.
  runAndLog "Building LLVM-GCC" ${LLVMGCC_OBJ_DIR}/llvmgcc-build.log \
      make CC_FOR_TARGET=llvm-fake-sfigcc CXX_FOR_TARGET=llvm-fake-sfig++ \
           GCC_FOR_TARGET=llvm-fake-sfigcc CFLAGS=-O2

  # LOCALMOD: the full set of C/C++ compiler envvars isn't propagated into the
  # libstdc++ build, so we get to do it all over again.
  cd arm-none-linux-gnueabi/libstdc++-v3
  runAndLog "Configuring libstdc++" ${LLVMGCC_OBJ_DIR}/libstdc++-configure.log \
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
  runAndLog "Building libstdc++" ${LLVMGCC_OBJ_DIR}/libstdc++-build.log \
      make CC=llvm-fake-sfigcc CXX=llvm-fake-sfig++ CFLAGS=-O2

  # Remove any previous versions to ensure we install
  cd /usr/local/crosstool-untrusted/
  local LIBDIR="arm-none-linux-gnueabi/llvm-gcc-4.2/"
  rm ${LIBDIR}/arm-none-linux-gnueabi/lib/libstdc++{.a,.so,.la,.so.6.0.9,.so.6}
  rm ${LIBDIR}/arm-none-linux-gnueabi/lib/{libgcc_s.so.1,libgcc_s.so}
  rm ${LIBDIR}/lib/gcc/arm-none-linux-gnueabi/4.2.1/libgcc{_eh,}.a
  cd -
  cd ../..
  runAndLog "Installing phase2 gcc output" ${LLVMGCC_OBJ_DIR}/install.log \
      make install
}

echo "Building in ${SCRATCH_ROOT}; installing in ${INSTALL_ROOT}"

createDir ${SRC_ROOT}
createDir ${OBJ_ROOT}

buildLLVM
installLLVMGCC

echo "Done."
