#!/bin/bash
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
#@                 Untrusted Toolchain Manager
#@-------------------------------------------------------------------
#@ This script builds the ARM and PNaCl untrusted toolchains.
#@ It MUST be run from the native_client/ directory.
#
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

# SAFE_MODE: When off, gcc-stage1 is not rebuilt when only llvm is modified.
#            This should be "ok" most of the time.
readonly UTMAN_SAFE_MODE=${UTMAN_SAFE_MODE:-false}

# Turn on debugging for this script
readonly UTMAN_DEBUG=${UTMAN_DEBUG:-false}

# For different levels of make parallelism change this in your env
readonly UTMAN_CONCURRENCY=${UTMAN_CONCURRENCY:-8}

readonly INSTALL_ROOT="$(pwd)/toolchain/linux_arm-untrusted"
readonly CROSS_TARGET=arm-none-linux-gnueabi
readonly TARGET_ROOT="${INSTALL_ROOT}/${CROSS_TARGET}"
readonly ARM_ARCH=armv7-a
readonly ARM_FPU=vfp3
readonly LLVM_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}/llvm"
readonly LLVMGCC_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}/llvm-gcc-4.2"
readonly GCC_VER="4.2.1"

# NOTE: NEWLIB_INSTALL_DIR also server as a SYSROOT
readonly NEWLIB_INSTALL_DIR="${INSTALL_ROOT}/arm-newlib"
readonly DRIVER_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET}"

# installing binutils and gcc in the same dir so that gcc
# uses the correct as and ld even if we move the install dir.
readonly BINUTILS_INSTALL_DIR="${LLVMGCC_INSTALL_DIR}"

# This toolchain currenlty builds only on linux.
# TODO(abetul): Remove the restriction on developer's OS choices.
readonly NACL_TOOLCHAIN=$(pwd)/toolchain/linux_x86/sdk/nacl-sdk

readonly PATCH_DIR=$(pwd)/tools/patches

readonly BFD_PLUGIN_DIR=${LLVMGCC_INSTALL_DIR}/lib/bfd-plugins


readonly MAKE_OPTS="-j${UTMAN_CONCURRENCY} VERBOSE=1"

# For speculative build status output. ( see status function )
# Leave this blank, it will be filled during processing.
SPECULATIVE_REBUILD_SET=""


# The directory in which we we keep src dirs (from hg repos)
# and objdirs. These should be ABSOLUTE paths.

readonly TC_SRC="$(pwd)/hg"
readonly TC_BUILD="$(pwd)/toolchain/hg-build"
readonly TC_LOG="$(pwd)/toolchain/hg-log"
readonly TC_LOG_ALL="${TC_LOG}/ALL"

# The location of sources (absolute)
readonly TC_SRC_LLVM="${TC_SRC}/llvm"
readonly TC_SRC_LLVM_GCC="${TC_SRC}/llvm-gcc"
readonly TC_SRC_BINUTILS="${TC_SRC}/binutils"
readonly TC_SRC_NEWLIB="${TC_SRC}/newlib"
readonly TC_SRC_LIBSTDCPP="${TC_SRC_LLVM_GCC}/llvm-gcc-4.2/libstdc++-v3"

# The location of each project
# These should be absolute paths.
readonly TC_BUILD_LLVM="${TC_BUILD}/llvm"
readonly TC_BUILD_LLVM_GCC1="${TC_BUILD}/llvm-gcc-stage1"
readonly TC_BUILD_LLVM_GCC2="${TC_BUILD}/llvm-gcc-stage2"
readonly TC_BUILD_BINUTILS_ARM="${TC_BUILD}/binutils-arm"
readonly TC_BUILD_BINUTILS_ARM_SB="${TC_BUILD}/binutils-arm-sandboxed"
readonly TC_BUILD_BINUTILS_X86_SB="${TC_BUILD}/binutils-x86-sandboxed"
readonly TC_BUILD_NEWLIB_ARM="${TC_BUILD}/newlib-arm"
readonly TC_BUILD_NEWLIB_BITCODE="${TC_BUILD}/newlib-bitcode"

# This apparently has to be at this location or gcc install breaks.
readonly TC_BUILD_LIBSTDCPP=\
"${TC_BUILD_LLVM_GCC2}/${CROSS_TARGET}/libstdc++-v3"

readonly TC_BUILD_LIBSTDCPP_BITCODE="${TC_BUILD_LLVM_GCC2}/libstdcpp-bitcode"

# These are fake directories, for storing the timestamp only
readonly TC_BUILD_EXTRASDK_ARM="${TC_BUILD}/extrasdk-arm"
readonly TC_BUILD_EXTRASDK_BITCODE="${TC_BUILD}/extrasdk-bitcode"

readonly TIMESTAMP_FILENAME="make-timestamp"

# PNaCl toolchain locations (absolute!)
readonly PNACL_TOOLCHAIN_ROOT="${INSTALL_ROOT}"
readonly PNACL_ARM_ROOT="${PNACL_TOOLCHAIN_ROOT}/libs-arm"
readonly PNACL_X8632_ROOT="${PNACL_TOOLCHAIN_ROOT}/libs-x8632"
readonly PNACL_X8664_ROOT="${PNACL_TOOLCHAIN_ROOT}/libs-x8664"
readonly PNACL_BITCODE_ROOT="${PNACL_TOOLCHAIN_ROOT}/libs-bitcode"

# PNaCl client-toolchain (sandboxed) binary locations
readonly PNACL_CLIENT_TC_ROOT="$(pwd)/toolchain/sandboxed_translators"
readonly PNACL_CLIENT_TC_ARM="${PNACL_CLIENT_TC_ROOT}/arm"
readonly PNACL_CLIENT_TC_X86="${PNACL_CLIENT_TC_ROOT}/x86"

# Current milestones within each Hg repo:
readonly LLVM_REV=dba662a50722
readonly LLVM_GCC_REV=08e173d1c977
readonly NEWLIB_REV=5d64fed35b93
readonly BINUTILS_REV=1675524d3abe

# Repositories
readonly REPO_LLVM_GCC="llvm-gcc.nacl-llvm-branches"
readonly REPO_LLVM="nacl-llvm-branches"
readonly REPO_NEWLIB="newlib.nacl-llvm-branches"
readonly REPO_BINUTILS="binutils.nacl-llvm-branches"


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
setup-tools() {

  source tools/llvm/tools.sh
  LD_FOR_SFI_TARGET=${LD_FOR_TARGET}
  CC_FOR_SFI_TARGET=${CC_FOR_TARGET}
  CXX_FOR_SFI_TARGET=${CXX_FOR_TARGET}
  AR_FOR_SFI_TARGET=${AR_FOR_TARGET}
  NM_FOR_SFI_TARGET=${NM_FOR_TARGET}
  RANLIB_FOR_SFI_TARGET=${RANLIB_FOR_TARGET}

  # Preprocessor flags
  CPPFLAGS_FOR_SFI_TARGET="-D__native_client__=1 \
                           -DMISSING_SYSCALL_NAMES=1"
  # C flags
  CFLAGS_FOR_SFI_TARGET="${CPPFLAGS_FOR_SFI_TARGET} \
                         -march=${ARM_ARCH} \
                         -ffixed-r9"
  # C++ flags
  CXXFLAGS_FOR_SFI_TARGET=${CFLAGS_FOR_SFI_TARGET}

  STD_ENV_FOR_GCC_ETC=(
    CFLAGS_FOR_TARGET="${CFLAGS_FOR_SFI_TARGET}"
    CPPFLAGS_FOR_TARGET="${CPPFLAGS_FOR_SFI_TARGET}"
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


  STD_ENV_FOR_LIBSTDCPP=(
    CC="${CC_FOR_SFI_TARGET}"
    CXX="${CXX_FOR_SFI_TARGET}"
    RAW_CXX_FOR_TARGET="${CXX_FOR_SFI_TARGET}"
    LD="${LD_FOR_SFI_TARGET}"
    CFLAGS="${CFLAGS_FOR_SFI_TARGET}"
    CPPFLAGS="${CPPFLAGS_FOR_SFI_TARGET}"
    CXXFLAGS="${CXXFLAGS_FOR_SFI_TARGET}"
    CFLAGS_FOR_TARGET="${CFLAGS_FOR_SFI_TARGET}"
    CPPFLAGS_FOR_TARGET="${CPPFLAGS_FOR_SFI_TARGET}"
    CC_FOR_TARGET="${CC_FOR_SFI_TARGET}"
    GCC_FOR_TARGET="${CC_FOR_SFI_TARGET}"
    CXX_FOR_TARGET="${CXX_FOR_SFI_TARGET}"
    AR_FOR_TARGET="${AR_FOR_SFI_TARGET}"
    NM_FOR_TARGET="${NM_FOR_SFI_TARGET}"
    RANLIB_FOR_TARGET="${RANLIB_FOR_SFI_TARGET}"
    AS_FOR_TARGET="${ILLEGAL_TOOL}"
    LD_FOR_TARGET="${ILLEGAL_TOOL}"
    OBJDUMP_FOR_TARGET="${ILLEGAL_TOOL}" )

  # NOTE: we do not expect the assembler or linker to be used to build newlib.a
  STD_ENV_FOR_NEWLIB=(
    CFLAGS_FOR_TARGET="${CFLAGS_FOR_SFI_TARGET}"
    CPPFLAGS_FOR_TARGET="${CPPFLAGS_FOR_SFI_TARGET}"
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
}




# The gold plugin that we use is documented at
# http://llvm.org/docs/GoldPlugin.html
# Despite its name it is actually used by both gold and bfd. The changes to
# this file to enable its use are:
# * Build shared
# * --enable-gold and --enable-plugin when building binutils
# * --with-binutils-include when building binutils
# * linking the plugin in bfd-plugins


######################################################################
######################################################################
#
#                     < USER ACCESSIBLE FUNCTIONS >
#
######################################################################
######################################################################

#@-------------------------------------------------------------------------

#@ hg-status             - Show status of repositories
hg-status() {

  hg-pull

  hg-status-common "${TC_SRC_LLVM}"       ${LLVM_REV}
  hg-status-common "${TC_SRC_LLVM_GCC}"   ${LLVM_GCC_REV}
  hg-status-common "${TC_SRC_NEWLIB}"     ${NEWLIB_REV}
  hg-status-common "${TC_SRC_BINUTILS}"   ${BINUTILS_REV}
}

hg-status-common() {
  local dir="$1"
  local rev="$2"

  spushd "$dir"
  local hg_status=$(hg status -mard)
  if [ ${#hg_status} -gt 0 ]; then
    LOCAL_CHANGES="YES"
  else
    LOCAL_CHANGES="NO"
  fi

  echo ""
  echo "Directory: hg/$(basename ${dir})"
  echo "  Branch         : $(hg branch)"
  echo "  Revision       : $(hg identify)"
  echo "  Local changes  : ${LOCAL_CHANGES}"
  echo "  Stable Revision: ${rev}"
  echo ""
  spopd
}


#@ hg-update-tip         - Update all repos to the tip (may merge)

hg-update-tip() {
  hg-pull

  hg-update-common "${TC_SRC_LLVM_GCC}"
  hg-update-common "${TC_SRC_LLVM}"
  hg-update-common "${TC_SRC_NEWLIB}"
  hg-update-common "${TC_SRC_BINUTILS}"
}

#@ hg-update-stable      - Update all repos to the latest stable rev (may merge)

hg-update-stable() {
  hg-pull
  hg-update-common "${TC_SRC_LLVM_GCC}"  ${LLVM_GCC_REV}
  hg-update-common "${TC_SRC_LLVM}"      ${LLVM_REV}
  hg-update-common "${TC_SRC_NEWLIB}"    ${NEWLIB_REV}
  hg-update-common "${TC_SRC_BINUTILS}"  ${BINUTILS_REV}
}

#@ hg-pull               - Pull all repos. (but do not update working copy)
hg-pull() {
  StepBanner "HG-PULL" "Running 'hg pull' in all repos..."
  hg-pull-common "${TC_SRC_LLVM_GCC}"
  hg-pull-common "${TC_SRC_LLVM}"
  hg-pull-common "${TC_SRC_NEWLIB}"
  hg-pull-common "${TC_SRC_BINUTILS}"
}

hg-pull-common() {
  local dir="$1"

  assert-dir "$dir" \
    "Repository $(basename "${dir}") doesn't exist. First do 'hg-checkout'"

  spushd "$dir"
  hg pull
  spopd
}

#@ hg-checkout           - check out mercurial repos needed to build toolchain
#@                          (skips repos which are already checked out)

hg-checkout() {
  StepBanner "HG-CHECKOUT"
  mkdir -p "${TC_SRC}"

  local add_headers=false
  if [ ! -d "${TC_SRC_NEWLIB}" ]; then
    add_headers=true
  fi

  hg-checkout-common ${REPO_LLVM_GCC} ${TC_SRC_LLVM_GCC} ${LLVM_GCC_REV}
  hg-checkout-common ${REPO_LLVM}     ${TC_SRC_LLVM}     ${LLVM_REV}
  hg-checkout-common ${REPO_BINUTILS} ${TC_SRC_BINUTILS} ${BINUTILS_REV}
  hg-checkout-common ${REPO_NEWLIB}   ${TC_SRC_NEWLIB}   ${NEWLIB_REV}

  if ${add_headers}; then
    newlib-nacl-headers
  fi
}

hg-checkout-common() {
  local repo=$1
  local dest=$2
  local rev=$3

  if [ ! -d ${dest} ] ; then
    StepBanner "HG-CHECKOUT" "Checking out new repository for ${repo} @ ${rev}"

    # Use a temporary directory just in case HG has problems
    # with long filenames during checkout.
    local TMPDIR="/tmp/hg-${rev}-$RANDOM"

    hg clone "https://${repo}.googlecode.com/hg/" "${TMPDIR}"
    spushd "${TMPDIR}"
    hg up -C ${rev}
    mv "${TMPDIR}" "${dest}"
    spopd
    echo "Done."
  else
    StepBanner "HG-CHECKOUT" "Using existing source for ${repo} in ${dest}"
  fi

}

hg-update-common() {
  local dir="$1"

  assert-dir "$dir" \
    "Repository $(basename "${dir}") doesn't exist. First do 'hg-checkout'"

  spushd "${dir}"
  if [ $# == 2 ]; then
    local rev=$2
    hg update ${rev}
  else
    hg update
  fi

  spopd
}

#@ hg-clean              - Remove all repos. (WARNING: local changes are lost)
hg-clean() {
  StepBanner "HG-CLEAN"

  echo "Are you sure?"
  echo "This will DELETE all source repositories under 'native_client/hg'"
  echo "Any local changes will be lost."
  echo ""
  echo "Type YES to confirm: "
  read CONFIRM_TEXT

  if [ $CONFIRM_TEXT == "YES" ]; then
    StepBanner "HG-CLEAN" "Cleaning Mercurial repositories"
    rm -rf "${TC_SRC}"
  else
    StepBanner "HG-CLEAN" "Clean cancelled by user"
  fi
}

#@-------------------------------------------------------------------------

#@ download-trusted      - Download and Install trusted SDKs (arm,x86-32,x86-64)
#@                         (your untrusted build will not be modified)
download-trusted() {
  StepBanner "DOWNLOAD-TRUSTED" "Downloading trusted toolchains"
  local installdir="${INSTALL_ROOT}"
  local tmpdir="${installdir}-backup"
  local dldir="${installdir}-downloaded"

  rm -rf "${dldir}"

  if [ -d "${tmpdir}" ]; then
    echo "ERROR: It is not expected that directory '${tmpdir}' exists."
    echo "       Please delete it if you don't need it"
    exit -1
  fi

  if [ -d "${installdir}" ]; then
    mv "${installdir}" "${tmpdir}"
  fi

  download-toolchains

  mv "${installdir}" "${dldir}"
  if [ -d "${tmpdir}" ]; then
    mv "${tmpdir}" "${installdir}"
  fi
}

#@-------------------------------------------------------------------------

#@ download-toolchains   - Download and Install all SDKs (arm,x86-32,x86-64)

download-toolchains() {
  # "--help" prevents building libs after the download
  # TODO(robertm): fix this in the SConstruct file to be less of a hack
  ./scons platform=arm --download --help sdl=none
  # we use "targetplatform" so that this works on both 32 and 64bit systems
  ./scons targetplatform=x86-64 --download --help sdl=none
}

#@-------------------------------------------------------------------------
#@ rebuild-pnacl-libs    - organized native libs and build bitcode libs
rebuild-pnacl-libs() {
  clean-pnacl
  organize-native-code
  newlib-bitcode
  extrasdk-bitcode
  libstdcpp-bitcode
}


#@ everything            - Build and install untrusted SDK.
everything() {

  PathSanityCheck
  check-for-trusted

  hg-checkout
  clean-install
  RecordRevisionInfo

  clean-logs

  binutils-arm
  llvm
  gcc-stage1
  driver
  gcc-stage2
  libstdcpp-arm
  gcc-stage3

  # NOTE: this builds native libs NOT needed for pnacl and currently
  #       also does some header file shuffling which IS needed
  newlib-arm
  extrasdk-arm
  misc-tools

  rebuild-pnacl-libs

  verify
}

#@ all                   - Alias for 'everything'
all() {
  everything
}

#@ progress              - Show build progress (open in another window)
progress() {
  StepBanner "PROGRESS WINDOW"

  while true; do
    if [ -f "${TC_LOG_ALL}" ]; then
      if tail --version > /dev/null; then
        # GNU coreutils tail
        tail -s 0.05 --max-unchanged-stats=20 --follow=name "${TC_LOG_ALL}"
      else
        # MacOS tail
        tail -F "${TC_LOG_ALL}"
      fi
    fi
    sleep 1
  done
}

#@ status                - Show status of build directories
status() {
  StepBanner "BUILD STATUS"

  status-helper "BINUTILS-ARM"      binutils-arm
  status-helper "LLVM"              llvm
  status-helper "GCC-STAGE1"        gcc-stage1
  status-helper "GCC-STAGE2"        gcc-stage2
  status-helper "LIBSTDCPP-ARM"     libstdcpp-arm

  status-helper "NEWLIB-ARM"        newlib-arm
  status-helper "EXTRASDK-ARM"      extrasdk-arm

  status-helper "NEWLIB-BITCODE"    newlib-bitcode
  status-helper "EXTRASDK-BITCODE"  extrasdk-bitcode
  status-helper "LIBSTDCPP-BITCODE" extrasdk-bitcode

}

status-helper() {
  local title="$1"
  local mod="$2"

  if ${mod}-needs-configure; then
    StepBanner "$title" "NEEDS FULL REBUILD"
    speculative-add "${mod}"
  elif ${mod}-needs-make; then
    StepBanner "$title" "NEEDS MAKE (INCREMENTAL)"
    speculative-add "${mod}"
  else
    StepBanner "$title" "OK (UP TO DATE)"
  fi
}

speculative-add() {
  local mod="$1"
  SPECULATIVE_REBUILD_SET="${SPECULATIVE_REBUILD_SET} ${mod}"
}

speculative-check() {
  local mod="$1"
  local search=$(echo "${SPECULATIVE_REBUILD_SET}" | grep -F "$mod")
  [ ${#search} -gt 0 ]
  return $?
}



#@ clean                 - Clean the build and install directories.
clean() {
  StepBanner "CLEAN" "Cleaning build, log, and install directories."

  clean-logs
  clean-build
  clean-install
}

#+ clean-logs            - Clean all logs
clean-logs() {
  rm -rf "${TC_LOG}"
}

#+ clean-build           - Clean all build directories
clean-build() {
  rm -rf "${TC_BUILD}"
}

#+ clean-install         - Clean install directories
clean-install() {
  rm -rf "${INSTALL_ROOT}"
}

#+ clean-pnacl           - Removes the pnacl-untrusted installation directory
clean-pnacl() {
  StepBanner "pnacl" "cleaning ${PNACL_TOOLCHAIN_ROOT}/libs-*"
  rm -rf ${PNACL_ARM_ROOT} \
         ${PNACL_BITCODE_ROOT} \
         ${PNACL_X8632_ROOT} \
         ${PNACL_X8664_ROOT}
}




#@-------------------------------------------------------------------------

#@ untrusted_sdk <file>  - Create untrusted SDK tarball from scratch
#@                          (clean + all + prune + tarball)

untrusted_sdk() {
  if [ ! -n "${1:-}" ]; then
    echo "Error: untrusted_sdk needs a tarball name." >&2
    exit 1
  fi

  PathSanityCheck
  clean
  everything
  prune
  tarball $1
}

#+ prune                 - Prune tree to make tarball smaller.

prune() {
  StepBanner "PRUNE" "Pruning llvm sourcery tree"

  SubBanner "Size before: $(du -msc "${TARGET_ROOT}")"
  rm  -f "${TARGET_ROOT}"/llvm/lib/lib*.a
  SubBanner "Size after: $(du -msc "${TARGET_ROOT}")"
}

#+ tarball <filename>    - Produce tarball file

tarball() {
  if [ ! -n "${1:-}" ]; then
    echo "Error: tarball needs a tarball name." >&2
    exit 1
  fi

  local tarball="$1"
  StepBanner "TARBALL" "Creating tar ball ${tarball}"

  tar zcf "${tarball}" -C "${INSTALL_ROOT}" .
}


#########################################################################
#                              < LLVM >
#########################################################################


#+-------------------------------------------------------------------------
#+ llvm                  - Configure, build and install LLVM.

llvm() {
  StepBanner "LLVM"

  local srcdir="${TC_SRC_LLVM}"

  assert-dir "${srcdir}" "You need to checkout LLVM."

  if llvm-needs-configure; then
    llvm-configure
  else
    SkipBanner "LLVM" "configure"
  fi

  if llvm-needs-make; then
    llvm-make
  else
    SkipBanner "LLVM" "make"
  fi

  llvm-install
}

#+ llvm-clean            - Clean LLVM completely
llvm-clean() {
  StepBanner "LLVM" "Clean"
  local objdir="${TC_BUILD_LLVM}"
  rm -rf ${objdir}
  mkdir -p ${objdir}
}

#+ llvm-configure        - Run LLVM configure
llvm-configure() {
  StepBanner "LLVM" "Configure"

  local srcdir="${TC_SRC_LLVM}"
  local objdir="${TC_BUILD_LLVM}"

  mkdir -p "${objdir}"
  spushd "${objdir}"

  # The --with-binutils-include is to allow llvm to build the gold plugin
  local binutils_include="${TC_SRC_BINUTILS}/binutils-2.20/include"
  RunWithLog "llvm.configure" \
      env -i PATH=/usr/bin/:/bin \
             MAKE_OPTS=${MAKE_OPTS} \
             CC=${CC} \
             CXX=${CXX} \
             ${srcdir}/llvm-trunk/configure \
             --disable-jit \
             --enable-optimized \
             --with-binutils-include=${binutils_include} \
             --enable-targets=x86,x86_64,arm \
             --target=${CROSS_TARGET} \
             --prefix=${LLVM_INSTALL_DIR} \
             --with-llvmgccdir=${LLVMGCC_INSTALL_DIR}
  spopd
}

llvm-needs-configure() {
  [ ! -f "${TC_BUILD_LLVM}/config.status" ]
  return $?
}

llvm-needs-make() {
  local srcdir="${TC_SRC_LLVM}"
  local objdir="${TC_BUILD_LLVM}"

  ts-modified "$srcdir" "$objdir"
  return $?
}

#+ llvm-make             - Run LLVM 'make'
llvm-make() {
  StepBanner "LLVM" "Make"

  local srcdir="${TC_SRC_LLVM}"
  local objdir="${TC_BUILD_LLVM}"

  spushd "${objdir}"

  ts-touch-open "${objdir}"

  RunWithLog llvm.make \
    env -i PATH=/usr/bin/:/bin \
           MAKE_OPTS="${MAKE_OPTS}" \
           CC=${CC} \
           CXX=${CXX} \
           make ${MAKE_OPTS} all

  ts-touch-commit  "${objdir}"

  spopd
}

#+ llvm-install          - Install LLVM
llvm-install() {
  StepBanner "LLVM" "Install"

  spushd "${TC_BUILD_LLVM}"
  RunWithLog llvm.install \
       make ${MAKE_OPTS} install

  mkdir -p "${BFD_PLUGIN_DIR}"
  ln -sf ../../../llvm/lib/LLVMgold.so "${BFD_PLUGIN_DIR}"
  ln -sf ../../../llvm/lib/libLTO.so "${BFD_PLUGIN_DIR}"
  spopd
}



#########################################################################
#     < GCC STAGE 1 >
#########################################################################

# Build "pregcc" which is a gcc that does not depend on having glibc/newlib
# already compiled. This also generates some important headers (confirm this).
#
# NOTE: depends on newlib source being set up so we can use it to set
#       up a sysroot.
#

#+-------------------------------------------------------------------------
#+ gcc-stage1            - build and install pre-gcc
gcc-stage1() {
  StepBanner "GCC-STAGE1"

  gcc-stage1-sysroot

  if gcc-stage1-needs-configure; then
    gcc-stage1-clean
    gcc-stage1-configure
  else
    SkipBanner "GCC-STAGE1" "configure"
  fi

  if gcc-stage1-needs-make; then
    gcc-stage1-make
  else
    SkipBanner "GCC-STAGE1" "make"
  fi

  gcc-stage1-install
}

#+ gcc-stage1-sysroot    - setup initial sysroot
gcc-stage1-sysroot() {
  StepBanner "GCC-STAGE1" "Setting up initial sysroot"

  local sys_include="${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/include"
  local sys_include2="${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/sys-include"

  rm -rf "${sys_include}" "${sys_include2}"
  mkdir -p "${sys_include}"
  ln -sf "${sys_include}" "${sys_include2}"
  cp -r "${TC_SRC_NEWLIB}"/newlib-1.17.0/newlib/libc/include/* ${sys_include}
}

#+ gcc-stage1-clean      - Clean gcc stage 1
gcc-stage1-clean() {
  StepBanner "GCC-STAGE1" "Clean"
  local objdir="${TC_BUILD_LLVM_GCC1}"
  rm -rf "${objdir}"
}

gcc-stage1-needs-configure() {
  [ ! -f "${TC_BUILD_LLVM_GCC1}/config.status" ]
  return $?
}

#+ gcc-stage1-configure  - Configure GCC stage 1
gcc-stage1-configure() {
  StepBanner "GCC-STAGE1" "Configure"

  local srcdir="${TC_SRC_LLVM_GCC}"
  local objdir="${TC_BUILD_LLVM_GCC1}"

  mkdir -p "${objdir}"
  spushd "${objdir}"

  # TODO(robertm): do we really need CROSS_TARGET_*
  RunWithLog llvm-pregcc.configure \
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

  spopd
}

gcc-stage1-needs-make() {
  local srcdir="${TC_SRC_LLVM_GCC}"
  local objdir="${TC_BUILD_LLVM_GCC1}"

  # In safe mode, rebuild gcc-stage1 when LLVM is updated.
  if ${UTMAN_SAFE_MODE}; then
    speculative-check "llvm" && return 0
    ts-newer-than "${TC_BUILD_LLVM}" \
                  "${TC_BUILD_LLVM_GCC1}" && return 0
  fi

  ts-modified "$srcdir" "$objdir"
  return $?
}

gcc-stage1-make() {
  StepBanner "GCC-STAGE1" "Make"

  local srcdir="${TC_SRC_LLVM_GCC}"
  local objdir="${TC_BUILD_LLVM_GCC1}"
  spushd ${objdir}

  ts-touch-open "${objdir}"

  # NOTE: we add ${BINUTILS_INSTALL_DIR}/bin to PATH
  RunWithLog llvm-pregcc.make \
       env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
              CC=${CC} \
              CXX=${CXX} \
              CFLAGS="-Dinhibit_libc" \
              make ${MAKE_OPTS} all

  ts-touch-commit "${objdir}"

  spopd
}

#+ gcc-stage1-install    - Install GCC stage 1
gcc-stage1-install() {
  StepBanner "GCC-STAGE1" "Install"

  spushd "${TC_BUILD_LLVM_GCC1}"

  # NOTE: we add ${BINUTILS_INSTALL_DIR}/bin to PATH
  RunWithLog llvm-pregcc.install \
       env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
              CC=${CC} \
              CXX=${CXX} \
              CFLAGS="-Dinhibit_libc" \
              make ${MAKE_OPTS} install

  spopd
}



#########################################################################
#   < GCC STAGE 2 >
#########################################################################

#+-------------------------------------------------------------------------
#+ gcc-stage2            - build libgcc, libiberty, libstdc++ (needs stage1)
# Build gcc again in order to build libgcc and other essential libs
# Note: depends on
gcc-stage2() {
  StepBanner "GCC-STAGE2"

  if gcc-stage2-needs-configure; then
    gcc-stage2-clean
    gcc-stage2-configure
  else
    SkipBanner "GCC-STAGE2" "configure"
  fi

  if gcc-stage2-needs-make; then
    gcc-stage2-make
  else
    SkipBanner "GCC-STAGE2" "make"
  fi

  gcc-stage2-install   # Partial install for libstdcpp
}

#+ gcc-stage2-clean      - Clean gcc-stage2
gcc-stage2-clean() {
  StepBanner "GCC-STAGE2" "Clean"
  local objdir="${TC_BUILD_LLVM_GCC2}"

  rm -rf "${objdir}"
}


gcc-stage2-needs-configure() {


  # If stage1 has been rebuilt, return true.
  speculative-check "gcc-stage1" && return 0
  ts-newer-than "${TC_BUILD_LLVM_GCC1}" "${TC_BUILD_LLVM_GCC2}" && return 0

  [ ! -f "${TC_BUILD_LLVM_GCC2}/config.status" ]
  return $?
}

#+ gcc-stage2-configure  - Configure gcc-stage2
gcc-stage2-configure() {
  StepBanner "GCC-STAGE2" "Configure"

  local srcdir="${TC_SRC_LLVM_GCC}"
  local objdir="${TC_BUILD_LLVM_GCC2}"

  mkdir -p "${objdir}"
  spushd "${objdir}"

  # TODO(robertm): do we really need CROSS_TARGET_*
  RunWithLog llvm-gcc.configure \
      env -i PATH=/usr/bin/:/bin \
             CC=${CC} \
             CXX=${CXX} \
             CFLAGS="-Dinhibit_libc" \
             CXXFLAGS="-Dinhibit_libc" \
             "${STD_ENV_FOR_GCC_ETC[@]}" \
             "${srcdir}"/llvm-gcc-4.2/configure \
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
               --with-sysroot="${NEWLIB_INSTALL_DIR}"

  spopd
}

gcc-stage2-needs-make() {
  local srcdir="${TC_SRC_LLVM_GCC}"
  local objdir="${TC_BUILD_LLVM_GCC2}"

  ts-modified "$srcdir" "$objdir"
  return $?
}

#+ gcc-stage2-make       - Make gcc-stage2
gcc-stage2-make() {
  StepBanner "GCC-STAGE2" "Make"

  local srcdir="${TC_SRC_LLVM_GCC}"
  local objdir="${TC_BUILD_LLVM_GCC2}"

  ts-touch-open "${objdir}"

  spushd "${objdir}"
  RunWithLog llvm-gcc.make \
       env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
              make \
              "${STD_ENV_FOR_GCC_ETC[@]}" \
              ${MAKE_OPTS} all-gcc
  spopd

  # These are here so they become part of the stage2 'transaction'
  libgcc-clean
  libgcc-make
  liberty-make

  ts-touch-commit "${objdir}"
}

#+ libgcc-clean          - Clean libgcc
libgcc-clean() {
  StepBanner "GCC-STAGE2" "Cleaning libgcc"

  spushd "${TC_BUILD_LLVM_GCC2}/gcc"
  RunWithLog libgcc.clean \
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
             make clean-target-libgcc
  spopd
}

#+ libgcc-make           - Make libgcc
libgcc-make() {
  StepBanner "GCC-STAGE2" "Making libgcc"
  local objdir="${TC_BUILD_LLVM_GCC2}/gcc"

  spushd "${objdir}"
  RunWithLog llvm-gcc.make_libgcc \
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
             make \
             "${STD_ENV_FOR_GCC_ETC[@]}" \
             ${MAKE_OPTS} libgcc.a
  spopd
}

#+ liberty-make          - Make liberty
liberty-make() {
  StepBanner "GCC-STAGE2" "Making Liberty"

  spushd "${TC_BUILD_LLVM_GCC2}"

  # maybe clean libiberty first
  RunWithLog llvm-gcc.make_libiberty \
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
             make \
             "${STD_ENV_FOR_GCC_ETC[@]}" \
             ${MAKE_OPTS} all-target-libiberty

  spopd
}

#+ gcc-stage2-install    - Install gcc-stage2
gcc-stage2-install() {
  StepBanner "GCC-STAGE2" "Install (Partial)"

  local objdir="${TC_BUILD_LLVM_GCC2}"
  spushd "${objdir}"

  cd gcc
  cp gcc-cross "${LLVMGCC_INSTALL_DIR}/bin/llvm-gcc"
  cp g++-cross "${LLVMGCC_INSTALL_DIR}/bin/llvm-g++"
  # NOTE: the "cp" will fail when we upgrade to a more recent compiler,
  #       simply fix this version when this happens
  cp cc1     "${LLVMGCC_INSTALL_DIR}/libexec/gcc/${CROSS_TARGET}/${GCC_VER}"
  cp cc1plus "${LLVMGCC_INSTALL_DIR}/libexec/gcc/${CROSS_TARGET}/${GCC_VER}"

  spopd
}



#########################################################################
#########################################################################
#                          < LIBSTDCPP-ARM >
#                          < LIBSTDCPP-BITCODE >
#########################################################################
#########################################################################

#+-------------------------------------------------------------------------
#+ libstdcpp-arm         - build and install libstdcpp for ARM


libstdcpp-arm() {
  StepBanner "LIBSTDCPP-ARM"

  if libstdcpp-arm-needs-configure; then
    libstdcpp-arm-clean
    libstdcpp-arm-configure
  else
    SkipBanner "LIBSTDCPP-ARM" "configure"
  fi


  if libstdcpp-arm-needs-make; then
    libstdcpp-arm-make
  else
    SkipBanner "LIBSTDCPP-ARM" "make"
  fi

  #libstdcpp-arm-install
}

#+ libstdcpp-bitcode     - build and install libstdcpp in bitcode
libstdcpp-bitcode() {
  StepBanner "LIBSTDCPP-BITCODE"

  if libstdcpp-bitcode-needs-configure; then
    libstdcpp-bitcode-clean
    libstdcpp-bitcode-configure
  else
    SkipBanner "LIBSTDCPP-BITCODE" "configure"
  fi

  if libstdcpp-bitcode-needs-make; then
    libstdcpp-bitcode-make
  else
    SkipBanner "LIBSTDCPP-BITCODE" "make"
  fi

  libstdcpp-bitcode-install
}

#+ libstdcpp-arm-clean   - clean libstdcpp for ARM
libstdcpp-arm-clean() {
  StepBanner "LIBSTDCPP-ARM" "Clean"
  rm -rf "${TC_BUILD_LIBSTDCPP}"
}

#+ libstdcpp-bitcode-clean - clean libstdcpp in bitcode
libstdcpp-bitcode-clean() {
  StepBanner "LIBSTDCPP-BITCODE" "Clean"
  rm -rf "${TC_BUILD_LIBSTDCPP_BITCODE}"
}

libstdcpp-arm-needs-configure() {
  speculative-check "gcc-stage2" && return 0
  ts-newer-than "${TC_BUILD_LLVM_GCC2}" "${TC_BUILD_LIBSTDCPP}" && return 0

  [ ! -f "${TC_BUILD_LIBSTDCPP}/config.status" ]
  return #?
}

libstdcpp-bitcode-needs-configure() {
  speculative-check "gcc-stage2" && return 0
  ts-newer-than "${TC_BUILD_LLVM_GCC2}" \
                "${TC_BUILD_LIBSTDCPP_BITCODE}" && return 0
  [ ! -f "${TC_BUILD_LIBSTDCPP_BITCODE}/config.status" ]
  return #?
}

#+ libstdcpp-arm-configure - configure libstdcpp for ARM
libstdcpp-arm-configure() {
  StepBanner "LIBSTDCPP-ARM" "Configure"
  libstdcpp-configure-common "${TC_BUILD_LIBSTDCPP}"
}

#+ libstdcpp-bitcode-configure - configure libstdcpp for bitcode
libstdcpp-bitcode-configure() {
  StepBanner "LIBSTDCPP-BITCODE" "Configure"
  BITCODE-ON
  libstdcpp-configure-common "${TC_BUILD_LIBSTDCPP_BITCODE}"
  BITCODE-OFF
}

libstdcpp-configure-common() {
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="$1"

  mkdir -p "${objdir}"
  spushd "${objdir}"

  RunWithLog llvm-gcc.configure_libstdcpp \
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
        "${STD_ENV_FOR_LIBSTDCPP[@]}" \
        "${srcdir}"/configure \
          --host=${CROSS_TARGET} \
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
          --srcdir="${srcdir}"
  spopd
}

libstdcpp-arm-needs-make() {
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="${TC_BUILD_LIBSTDCPP}"

  ts-modified "$srcdir" "$objdir"
  return $?
}

libstdcpp-bitcode-needs-make() {
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="${TC_BUILD_LIBSTDCPP_BITCODE}"

  ts-modified "$srcdir" "$objdir"
  return $?
}

#+ libstdcpp-arm-make    - Make libstdcpp for ARM
libstdcpp-arm-make() {
  StepBanner "LIBSTDCPP-ARM" "Make"
  libstdcpp-make-common "${TC_BUILD_LIBSTDCPP}"
}

#+ libstdcpp-bitcode-make - Make libstdcpp in bitcode
libstdcpp-bitcode-make() {
  StepBanner "LIBSTDCPP-BITCODE" "Make"
  BITCODE-ON
  libstdcpp-make-common "${TC_BUILD_LIBSTDCPP_BITCODE}"
  BITCODE-OFF
}

libstdcpp-make-common() {
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="$1"

  ts-touch-open "${objdir}"

  spushd "${objdir}"
  RunWithLog llvm-gcc.make_libstdcpp \
    env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
        make \
        "${STD_ENV_FOR_LIBSTDCPP[@]}" \
        ${MAKE_OPTS}
  spopd

  ts-touch-commit "${objdir}"
}
#+ libstdcpp-arm-install - Install libstdcpp for ARM
libstdcpp-arm-install() {
  StepBanner "LIBSTDCPP-ARM" "Install"
  local objdir="${TC_BUILD_LIBSTDCPP}"

  spushd "${objdir}"
  RunWithLog ${TMP}/llvm-gcc.install_libstdcpp.log \
    env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
      make  ${MAKE_OPTS} install
  spopd
}

#+ libstdcpp-bitcode-install - Install libstdcpp in bitcode
libstdcpp-bitcode-install() {
  StepBanner "LIBSTDCPP-BITCODE" "Install"
  local objdir="${TC_BUILD_LIBSTDCPP_BITCODE}"
  local dest="${PNACL_BITCODE_ROOT}"

  spushd "${objdir}"

  cp "${objdir}/src/.libs/libstdc++.a" "${dest}"

  spopd
}


#########################################################################
#########################################################################
#
#                          < GCC-STAGE3 >
#
#########################################################################
#########################################################################

#+-------------------------------------------------------------------------
#+ gcc-stage3            - install final llvm-gcc and libraries (needs stage2)
gcc-stage3() {
  StepBanner "GCC-STAGE3"

  gcc-stage3-install
  libgcc-install
  liberty-install
  libstdcpp-header-massage
}

#+ gcc-stage3-install          - install final llvm-gcc
gcc-stage3-install() {
  StepBanner "GCC-STAGE3" "Install"

  # NOTE: this is the build dir from stage2
  local objdir="${TC_BUILD_LLVM_GCC2}"

  spushd "${objdir}"
  RunWithLog llvm-gcc.install \
      env -i PATH=/usr/bin/:/bin:${BINUTILS_INSTALL_DIR}/bin \
             make \
             "${STD_ENV_FOR_GCC_ETC[@]}" \
             install

  spopd
}

#+ libgcc-install        - install libgcc
libgcc-install() {
  StepBanner "GCC-STAGE3" "Installing libgcc"

  local objdir="${TC_BUILD_LLVM_GCC2}"
  spushd "${objdir}"
  # NOTE: the "cp" will fail when we upgrade to a more recent compiler,
  #       simply fix this version when this happens
  cp gcc/libg*.a "${LLVMGCC_INSTALL_DIR}/lib/gcc/${CROSS_TARGET}/${GCC_VER}/"
  spopd
}

#+ liberty-install       - install liberty
liberty-install() {
  StepBanner "GCC-STAGE3" "Installing liberty"

  local objdir="${TC_BUILD_LLVM_GCC2}"

  spushd "${objdir}"
  cp libiberty/libiberty.a "${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/lib"
  spopd
}

#+ libstdcpp-header-massage - ?
libstdcpp-header-massage() {
  StepBanner "GCC-STAGE3" "Header massing for libstdc++"

  # Don't ask
  rm -f "${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/include/c++"
  ln -s ../../include/c++ \
        "${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/include/c++"

}


#+ misc-tools            - Build and install sel_ldr and validator for ARM.
misc-tools() {
  StepBanner "MISC-ARM" "Building sel_ldr (ARM)"

   # TODO(robertm): revisit some of these options
   RunWithLog arm_sel_ldr \
          ./scons MODE=opt-linux \
          platform=arm \
          sdl=none \
          naclsdk_validate=0 \
          sysinfo= \
          sel_ldr
   rm -rf  ${INSTALL_ROOT}/tools-arm
   mkdir ${INSTALL_ROOT}/tools-arm
   cp scons-out/opt-linux-arm/obj/src/trusted/service_runtime/sel_ldr\
     ${INSTALL_ROOT}/tools-arm

   StepBanner "MISC-ARM" "Building validator (ARM)"
   RunWithLog arm_ncval_core \
           ./scons MODE=opt-linux \
           targetplatform=arm \
           sysinfo= \
           arm-ncval-core
   rm -rf  ${INSTALL_ROOT}/tools-x86
   mkdir ${INSTALL_ROOT}/tools-x86
   cp scons-out/opt-linux-x86-32-to-arm/obj/src/trusted/validator_arm/\
arm-ncval-core ${INSTALL_ROOT}/tools-x86
}


#########################################################################
#     < BINUTILS >
#########################################################################

#+-------------------------------------------------------------------------
#+ binutils-arm          - Build and install binutils for ARM
binutils-arm() {
  StepBanner "BINUTILS-ARM"

  local srcdir="${TC_SRC_BINUTILS}"

  assert-dir "${srcdir}" "You need to checkout binutils."

  if binutils-arm-needs-configure; then
    binutils-arm-clean
    binutils-arm-configure
  else
    SkipBanner "BINUTILS-ARM" "configure"
  fi

  if binutils-arm-needs-make; then
    binutils-arm-make
  else
    SkipBanner "BINUTILS-ARM" "make"
  fi

  binutils-arm-install
}

#+ binutils-arm-clean    - Clean binutils
binutils-arm-clean() {
  StepBanner "BINUTILS-ARM" "Clean"
  local objdir="${TC_BUILD_BINUTILS_ARM}"
  rm -rf ${objdir}
}

#+ binutils-arm-configure- Configure binutils for ARM
binutils-arm-configure() {
  StepBanner "BINUTILS-ARM" "Configure"

  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS_ARM}"

  mkdir -p "${objdir}"
  spushd "${objdir}"

  # --enable-checking is to avoid a build failure:
  #   tc-arm.c:2489: warning: empty body in an if-statement
  # The --enable-gold and --enable-plugins options are on so that we
  # can use gold's support for plugin to link PNaCl modules.
  RunWithLog binutils.arm.configure \
    env -i \
    PATH="/usr/bin:/bin" \
    CC=${CC} \
    CXX=${CXX} \
    ${srcdir}/binutils-2.20/configure --prefix=${BINUTILS_INSTALL_DIR} \
                                      --target=${CROSS_TARGET} \
                                      --enable-checking \
                                      --enable-gold \
                                      --enable-plugins \
                                      --disable-werror \
                                      --with-sysroot=${NEWLIB_INSTALL_DIR}
  spopd
}

binutils-arm-needs-configure() {
  [ ! -f "${TC_BUILD_BINUTILS_ARM}/config.status" ]
  return $?
}

binutils-arm-needs-make() {
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS_ARM}"

  ts-modified "$srcdir" "$objdir"
  return $?
}

#+ binutils-arm-make     - Make binutils for ARM
binutils-arm-make() {
  StepBanner "BINUTILS-ARM" "Make"
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS_ARM}"
  spushd "${objdir}"

  ts-touch-open "${objdir}"

  RunWithLog binutils.arm.make \
    env -i PATH="/usr/bin:/bin" \
    make ${MAKE_OPTS}

  ts-touch-commit "${objdir}"

  spopd
}

#+ binutils-arm-install  - Install binutils for ARM
binutils-arm-install() {
  StepBanner "BINUTILS" "Install"
  local objdir="${TC_BUILD_BINUTILS_ARM}"
  spushd "${objdir}"

  RunWithLog binutils.arm.install \
    env -i PATH="/usr/bin:/bin" \
    make \
      install ${MAKE_OPTS}

  spopd
}

#########################################################################
#     CLIENT BINARIES (SANDBOXED)
#########################################################################

#+-------------------------------------------------------------------------
#+ binutils-arm-sb       - build and install binutils (sandboxed) for ARM
binutils-arm-sb() {
  StepBanner "BINUTILS-ARM-SB"

  if [ ! -d ${NACL_TOOLCHAIN} ] ; then
    echo "ERROR: install Native Client toolchain"
    exit -1
  fi

  # TODO(pdox): make this incremental
  binutils-arm-sb-clean
  binutils-arm-sb-configure
  binutils-arm-sb-make
  binutils-arm-sb-install
}
#+ binutils-arm-sb-clean - clean binutils (sandboxed) for ARM
binutils-arm-sb-clean() {
  StepBanner "BINUTILS-ARM-SB" "Clean"
  local objdir="${TC_BUILD_BINUTILS_ARM_SB}"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"
}

#+ binutils-arm-sb-configure - configure binutils (sandboxed) for ARM
binutils-arm-sb-configure() {
  StepBanner "BINUTILS-ARM-SB" "Configure"
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS_ARM_SB}"

  spushd ${objdir}
  # --enable-checking is to avoid a build failure:
  #   tc-arm.c:2489: warning: empty body in an if-statement
  # The --enable-gold and --enable-plugins options are on so that we
  # can use gold's support for plugin to link PNaCl modules.
  RunWithLog \
    binutils.arm.sandboxed.configure \
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
    ${srcdir}/binutils-2.20/configure \
                             --prefix=${PNACL_CLIENT_TC_ARM} \
                             --host=nacl \
                             --target=${CROSS_TARGET} \
                             --disable-nls \
                             --enable-checking \
                             --enable-static \
                             --enable-shared=no \
                             --with-sysroot=${NEWLIB_INSTALL_DIR}
  spopd
}

#+ binutils-arm-sb-make - Install binutils (sandboxed) for ARM
binutils-arm-sb-make() {
  StepBanner "BINUTILS-ARM-SB" "Make"
  local objdir="${TC_BUILD_BINUTILS_ARM_SB}"
  spushd ${objdir}

  RunWithLog binutils.arm.sandboxed.make \
    env -i PATH="/usr/bin:/bin" \
    make ${MAKE_OPTS} all-gas all-ld
  spopd
}

#+ binutils-arm-sb-install - Install binutils (sandboxed) for ARM
binutils-arm-sb-install() {
  StepBanner "BINUTILS-ARM-SB" "Install"
  local objdir="${TC_BUILD_BINUTILS_ARM_SB}"
  spushd ${objdir}

  RunWithLog binutils.arm.sandboxed.install \
    env -i PATH="/usr/bin:/bin" \
    make install-gas install-ld

  spopd
}

#+-------------------------------------------------------------------------
#+ binutils-x86-sb       - build and install binutils (sandboxed) for x86
binutils-x86-sb() {
  StepBanner "BINUTILS-X86-SB"

  if [ ! -d ${NACL_TOOLCHAIN} ] ; then
    echo "ERROR: install Native Client toolchain"
    exit -1
  fi

  if [ ! -f ${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/lib/libiberty.a ] ; then
    echo "ERROR: install Portable Native Client toolchain"
    exit -1
  fi

  # TODO(pdox): make this incremental
  binutils-x86-sb-clean
  binutils-x86-sb-configure
  binutils-x86-sb-make
  binutils-x86-sb-install
}

#+ binutils-x86-sb-clean - clean binutils (sandboxed) for x86
binutils-x86-sb-clean() {
  StepBanner "BINUTILS-X86-SB" "Clean"
  local objdir="${TC_BUILD_BINUTILS_X86_SB}"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"
}

#+ binutils-x86-sb-configure - configure binutils (sandboxed) for x86
binutils-x86-sb-configure() {
  StepBanner "BINUTILS-X86-SB" "Configure"
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS_X86_SB}"

  mkdir ${TC_BUILD_BINUTILS_X86_SB}/opcodes
  spushd ${objdir}
  cp ${LLVMGCC_INSTALL_DIR}/${CROSS_TARGET}/lib/libiberty.a ./opcodes/.
  RunWithLog \
    binutils.x86.sandboxed.configure \
    env -i \
    PATH="/usr/bin:/bin" \
    AR="${NACL_TOOLCHAIN}/bin/nacl-ar" \
    AS="${NACL_TOOLCHAIN}/bin/nacl-as" \
    CC="${NACL_TOOLCHAIN}/bin/nacl-gcc" \
    CXX="${NACL_TOOLCHAIN}/bin/nacl-g++" \
    EMULATOR_FOR_BUILD="$(pwd)/scons-out/dbg-linux-x86-32/staging/sel_ldr -d" \
    LD="${NACL_TOOLCHAIN}/bin/nacl-ld" \
    RANLIB="${NACL_TOOLCHAIN}/bin/nacl-ranlib" \
    CFLAGS="-m32 -O2 -DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5 -DNACL_TOOLCHAIN_PATCH -DPNACL_TOOLCHAIN_SANDBOX -I${NACL_TOOLCHAIN}/nacl/include" \
    LDFLAGS="-s" \
    LDFLAGS_FOR_BUILD="-L." \
    ${srcdir}/binutils-2.20/configure \
                             --prefix=${PNACL_CLIENT_TC_X86} \
                             --host=nacl \
                             --target=nacl64 \
                             --disable-nls \
                             --enable-static \
                             --enable-shared=no \
                             --with-sysroot=${NEWLIB_INSTALL_DIR}
  spopd
}

#+ binutils-x86-sb-make - Install binutils (sandboxed) for x86
binutils-x86-sb-make() {
  StepBanner "BINUTILS-X86-SB" "Make"
  local objdir="${TC_BUILD_BINUTILS_X86_SB}"
  spushd ${objdir}

  RunWithLog binutils.x86.sandboxed.make \
    env -i PATH="/usr/bin:/bin" \
    make ${MAKE_OPTS} all-gas
  spopd
}

#+ binutils-x86-sb-install - Install binutils (sandboxed) for x86
binutils-x86-sb-install() {
  StepBanner "BINUTILS-X86-SB" "Install"
  local objdir="${TC_BUILD_BINUTILS_X86_SB}"
  spushd ${objdir}

  RunWithLog binutils.x86.sandboxed.install \
    env -i PATH="/usr/bin:/bin" \
    make install-gas

  spopd
}

#########################################################################
#     < NEWLIB-ARM >
#     < NEWLIB-BITCODE >
#########################################################################

#+-------------------------------------------------------------------------
#+ newlib-arm            - Build and install newlib for ARM.
newlib-arm() {
  StepBanner "NEWLIB-ARM"

  if newlib-arm-needs-configure; then
    newlib-arm-clean
    newlib-arm-configure
  else
    SkipBanner "NEWLIB-ARM" "configure"
  fi

  if newlib-arm-needs-make; then
    newlib-arm-make
  else
    SkipBanner "NEWLIB-ARM" "make"
  fi
  newlib-arm-install
}

#+ newlib-bitcode        - Build and install newlib in bitcode.
newlib-bitcode() {
  StepBanner "NEWLIB-BITCODE"

  if newlib-bitcode-needs-configure; then
    newlib-bitcode-clean
    newlib-bitcode-configure
  else
    SkipBanner "NEWLIB-BITCODE" "configure"
  fi

  if newlib-bitcode-needs-make; then
    newlib-bitcode-make
  else
    SkipBanner "NEWLIB-BITCODE" "make"
  fi

  newlib-bitcode-install
}

#+ newlib-arm-clean      - Clean ARM newlib.
newlib-arm-clean() {
  StepBanner "NEWLIB-ARM" "Clean"
  rm -rf "${TC_BUILD_NEWLIB_ARM}"
}

#+ newlib-bitcode-clean  - Clean bitcode newlib.
newlib-bitcode-clean() {
  StepBanner "NEWLIB-BITCODE" "Clean"
  rm -rf "${TC_BUILD_NEWLIB_BITCODE}"
}

newlib-arm-needs-configure() {
  speculative-check "gcc-stage2" && return 0
  ts-newer-than "${TC_BUILD_LLVM_GCC2}" \
                   "${TC_BUILD_NEWLIB_ARM}" && return 0

  [ ! -f "${TC_BUILD_NEWLIB_ARM}/config.status" ]
  return #?
}

newlib-bitcode-needs-configure() {
  speculative-check "gcc-stage2" && return 0
  ts-newer-than "${TC_BUILD_LLVM_GCC2}" \
                   "${TC_BUILD_NEWLIB_BITCODE}" && return 0

  [ ! -f "${TC_BUILD_NEWLIB_BITCODE}/config.status" ]
  return #?
}

#+ newlib-arm-configure  - Configure ARM Newlib
newlib-arm-configure() {
  StepBanner "NEWLIB-ARM" "Configure"
  newlib-configure-common "${TC_BUILD_NEWLIB_ARM}"
}

#+ newlib-bitcode-configure - Configure bitcode Newlib
newlib-bitcode-configure() {
  StepBanner "NEWLIB-BITCODE" "Configure"
  BITCODE-ON
  newlib-configure-common "${TC_BUILD_NEWLIB_BITCODE}"
  BITCODE-OFF
}

newlib-configure-common() {
  local srcdir="${TC_SRC_NEWLIB}"
  local objdir="$1"
  mkdir -p "${objdir}"
  spushd "${objdir}"

  RunWithLog newlib.configure \
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
  spopd
}

newlib-arm-needs-make() {
  local srcdir="${TC_SRC_NEWLIB}"
  local objdir="${TC_BUILD_NEWLIB_ARM}"

  ts-modified "$srcdir" "$objdir"
  return $?
}

newlib-bitcode-needs-make() {
  local srcdir="${TC_SRC_NEWLIB}"
  local objdir="${TC_BUILD_NEWLIB_BITCODE}"

  ts-modified "$srcdir" "$objdir"
  return $?
}

#+ newlib-arm-make       - Make ARM Newlib
newlib-arm-make() {
  StepBanner "NEWLIB-ARM" "Make"
  newlib-make-common "${TC_BUILD_NEWLIB_ARM}"
}

#+ newlib-bitcode-make   - Make bitcode Newlib
newlib-bitcode-make() {
  StepBanner "NEWLIB-BITCODE" "Make"

  BITCODE-ON
  newlib-make-common "${TC_BUILD_NEWLIB_BITCODE}"
  BITCODE-OFF
}

newlib-make-common() {
  local srcdir="${TC_SRC_NEWLIB}"
  local objdir="$1"

  ts-touch-open "${objdir}"

  spushd "${objdir}"
  RunWithLog newlib.make \
    env -i PATH="/usr/bin:/bin" \
    make \
      "${STD_ENV_FOR_NEWLIB[@]}" \
      ${MAKE_OPTS}
  spopd

  ts-touch-commit "${objdir}"

}

#+ newlib-arm-install    - Install ARM Newlib
newlib-arm-install() {
  StepBanner "NEWLIB-ARM" "Install"

  local objdir="${TC_BUILD_NEWLIB_ARM}"
  spushd "${objdir}"

  RunWithLog newlib.install \
    env -i PATH="/usr/bin:/bin" \
      make \
      "${STD_ENV_FOR_NEWLIB[@]}" \
      install ${MAKE_OPTS}

  StepBanner "NEWLIB-ARM" "Extra-install"
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
  StepBanner "NEWLIB-ARM" "Removing old pthreads"
  rm -f "${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/usr/include/pthread.h"
  rm -f "${sys_include}/pthread.h"

  spopd
}

#+ newlib-bitcode-install - Install Bitcode Newlib
newlib-bitcode-install() {
  StepBanner "NEWLIB-BITCODE" "Install (lib[cgm])"

  local objdir="${TC_BUILD_NEWLIB_BITCODE}"
  local destdir="${PNACL_BITCODE_ROOT}"
  mkdir -p "${destdir}"

  # We only install libc/libg/libm
  cp ${objdir}/${CROSS_TARGET}/newlib/lib[cgm].a "${destdir}"
}





#########################################################################
#     < EXTRASDK-ARM >
#     < EXTRASDK-BITCODE >
#########################################################################

#+-------------------------------------------------------------------------
#+ extrasdk-arm          - Build and install extra SDK libs (ARM)

# TODO(pdox): Merge these somehow.. gracefully?
extrasdk-arm() {
  StepBanner "EXTRASDK-ARM"

  if extrasdk-arm-needs-make; then
    extrasdk-arm-make
  fi
  extrasdk-arm-install
}

#+ extrasdk-bitcode      - Build and install extra SDK libs (bitcode)
extrasdk-bitcode() {
  StepBanner "EXTRASDK-BITCODE"

  if extrasdk-bitcode-needs-make; then
    extrasdk-bitcode-make
  fi
  extrasdk-bitcode-install
}

extrasdk-arm-needs-configure() {
  return 1  # false
}

extrasdk-arm-needs-make() {
  speculative-check "gcc-stage2" && return 0
  ts-newer-than "${TC_BUILD_LLVM_GCC2}" \
                   "${TC_BUILD_EXTRASDK_ARM}" && return 0
  return 1   # false
}

extrasdk-bitcode-needs-configure() {
  return 1  # false
}

extrasdk-bitcode-needs-make() {
  speculative-check "gcc-stage2" && return 0
  ts-newer-than "${TC_BUILD_LLVM_GCC2}" \
                "${TC_BUILD_EXTRASDK_BITCODE}" && return 0
  return 1   # false
}

extrasdk-arm-make() {
  StepBanner "EXTRASDK-ARM" "Make"

  local objdir="${TC_BUILD_EXTRASDK_ARM}"

  mkdir -p "${objdir}"
  ts-touch-open "${objdir}"

  TARGET_CODE=sfi-arm RunWithLog "extra_sdk_arm" \
      ./scons MODE=nacl_extra_sdk \
      platform=arm \
      sdl=none \
      naclsdk_validate=0 \
      extra_sdk_clean \
      extra_sdk_update_header \
      install_libpthread \
      extra_sdk_update

  # Keep a backup of the files extrasdk generated
  # in case we want to re-install them again.

  local include_install="${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/include/nacl"
  local lib_install="${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/lib"
  local include_save="${TC_BUILD_EXTRASDK_ARM}/include-nacl"
  local lib_save="${TC_BUILD_EXTRASDK_ARM}/lib"

  rm -rf "${include_save}"
  rm -rf "${lib_save}"

  cp -a "${lib_install}" "${lib_save}"
  cp -a "${include_install}" "${include_save}"

  # Except libc/libg/libm, since they do not belong to extrasdk...
  rm -f "${lib_save}"/lib[cgm].a

  ts-touch-commit "${objdir}"
}


extrasdk-bitcode-make() {
  StepBanner "EXTRASDK-BITCODE" "Make"

  local objdir="${TC_BUILD_EXTRASDK_BITCODE}"
  mkdir -p "${objdir}"
  ts-touch-open "${objdir}"

  TARGET_CODE=bc-arm RunWithLog "extra_sdk_bitcode" \
      ./scons MODE=nacl_extra_sdk \
      platform=arm \
      sdl=none \
      naclsdk_validate=0 \
      disable_nosys_linker_warnings=1 \
      extra_sdk_clean \
      extra_sdk_update_header \
      install_libpthread \
      extra_sdk_update


  # NOTE: as collateral damage we also generate these as (arm) native code
  rm -f ${PNACL_BITCODE_ROOT}/crt*.o \
    ${PNACL_BITCODE_ROOT}/intrinsics.o \
    ${PNACL_BITCODE_ROOT}/libcrt_platform.a

  # Keep a backup of the files extrasdk generated
  # in case we want to re-install them again.
  local include_install="${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/include/nacl"
  local lib_install="${PNACL_BITCODE_ROOT}"
  local include_save="${TC_BUILD_EXTRASDK_BITCODE}/include-nacl"
  local lib_save="${TC_BUILD_EXTRASDK_BITCODE}/lib"

  rm -rf "${include_save}"
  rm -rf "${lib_save}"

  cp -a "${lib_install}" "${lib_save}"
  cp -a "${include_install}" "${include_save}"

  # Except libc/libg/libm, since they do not belong to extrasdk...
  rm -f "${lib_save}"/lib[cgm].a

  ts-touch-commit "${objdir}"

}

extrasdk-arm-install() {
  StepBanner "EXTRASDK-ARM" "Install"

  # Copy from the save directories
  local include_install="${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/include/nacl"
  local lib_install="${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/lib"
  local include_save="${TC_BUILD_EXTRASDK_ARM}/include-nacl"
  local lib_save="${TC_BUILD_EXTRASDK_ARM}/lib"

  mkdir -p "${include_install}"
  mkdir -p "${lib_install}"

  cp -fa "${include_save}"/* "${include_install}"
  cp -fa "${lib_save}"/*     "${lib_install}"
}

extrasdk-bitcode-install() {
  StepBanner "EXTRASDK-BITCODE" "Install"

  # Copy from the save directories
  local include_install="${NEWLIB_INSTALL_DIR}/${CROSS_TARGET}/include/nacl"
  local lib_install="${PNACL_BITCODE_ROOT}"
  local include_save="${TC_BUILD_EXTRASDK_BITCODE}/include-nacl"
  local lib_save="${TC_BUILD_EXTRASDK_BITCODE}/lib"

  mkdir -p "${include_install}"
  mkdir -p "${lib_install}"

  cp -fa "${include_save}"/* "${include_install}"
  cp -fa "${lib_save}"/*     "${lib_install}"
}

#+ newlib-nacl-headers   - Add custom NaCl headers to newlib
newlib-nacl-headers() {
  StepBanner "Adding nacl headers to newlib"
  here=$(pwd)
  spushd ${TC_SRC_NEWLIB}/newlib-1.17.0
  ${here}/src/trusted/service_runtime/export_header.py \
      ${here}/src/trusted/service_runtime/include \
      newlib/libc/include
  spopd
}



#+-------------------------------------------------------------------------
#+ driver                - Install driver scripts.
driver() {
  StepBanner "DRIVER"
  linker-install
  llvm-fake-install
}

# We need to adjust the start address and aligment of nacl arm modules
linker-install() {
   StepBanner "DRIVER" "Installing untrusted ld script"
   cp tools/llvm/ld_script_arm_untrusted ${TARGET_ROOT}
}

# the driver is a simple python script which changes its behavior
# depending under the name it is invoked as
llvm-fake-install() {
  StepBanner "DRIVER" "Installing driver adaptors to ${DRIVER_INSTALL_DIR}"
  # TODO(robertm): move the driver to their own dir
  #rm -rf  ${DRIVER_INSTALL_DIR}
  #mkdir -p ${DRIVER_INSTALL_DIR}
  cp tools/llvm/llvm-fake.py ${DRIVER_INSTALL_DIR}
  for s in bcgcc bcg++ \
           sfigcc sfig++ \
           sfild bcld bcld-arm bcld-x86-32 bcld-x86-64 \
           illegal nop ; do
    local t="llvm-fake-$s"
    ln -fs llvm-fake.py ${DRIVER_INSTALL_DIR}/$t
  done
}



######################################################################
######################################################################
#
#                           HELPER FUNCTIONS
#
#             (These should not generally be used directly)
#
######################################################################
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

PackageCheck() {
  assert-bin "makeinfo" "makeinfo not found. Please install 'texinfo' package."
  assert-bin "bison"    "bison not found. Please install 'bison' package."
  assert-bin "flex"     "flex not found. Please install 'flex' package."
}

assert-bin() {
  local exe="$1"
  local msg="$2"

  if ! which "$exe" > /dev/null; then
    Banner "ERROR: $msg"
    exit -1
  fi
}



RecordRevisionInfo() {
  mkdir -p "${INSTALL_ROOT}"
  svn info > "${INSTALL_ROOT}/REV"
}


#+ organize-native-code  - Saves the native code libraries for each architecture
#+                         into the toolchain/pnacl-untrusted/<arch> directories.
organize-native-code() {
  StepBanner "PNACL" "Organizing Native Code"

  readonly arm_src=toolchain/linux_arm-untrusted
  readonly x86_src=toolchain/linux_x86/sdk/nacl-sdk
  readonly arm_llvm_gcc=${arm_src}/arm-none-linux-gnueabi/llvm-gcc-4.2

  # TODO(espindola): There is a transitive dependency from libgcc to
  # libc, libnosys and libnacl. We should try to factor this better.

  DebugRun Banner "arm native code: ${PNACL_ARM_ROOT}"
  mkdir -p ${PNACL_ARM_ROOT}

  cp -f ${arm_llvm_gcc}/lib/gcc/arm-none-linux-gnueabi/${GCC_VER}/libgcc.a \
    ${arm_src}/arm-newlib/arm-none-linux-gnueabi/lib/libc.a \
    ${arm_src}/arm-newlib/arm-none-linux-gnueabi/lib/libnosys.a \
    ${arm_src}/arm-newlib/arm-none-linux-gnueabi/lib/libnacl.a \
    ${arm_src}/arm-newlib/arm-none-linux-gnueabi/lib/crt*.o \
    ${arm_src}/arm-newlib/arm-none-linux-gnueabi/lib/libcrt*.a \
    ${arm_src}/arm-newlib/arm-none-linux-gnueabi/lib/intrinsics.o \
    ${PNACL_ARM_ROOT}
  DebugRun ls -l ${PNACL_ARM_ROOT}

  # TODO(espindola): These files have been built with the conventional
  # nacl-gcc. The ABI might not be exactly the same as the one used by
  # PNaCl. We should build these files with PNaCl.
  DebugRun Banner "x86-32 native code: ${PNACL_X8632_ROOT}"
  mkdir -p ${PNACL_X8632_ROOT}
  cp -f ${x86_src}/lib/gcc/nacl64/4.4.3/32/libgcc.a \
    ${x86_src}/nacl64/lib/32/libc.a \
    ${x86_src}/nacl64/lib/32/libnosys.a \
    ${x86_src}/nacl64/lib/32/libnacl.a \
    ${x86_src}/nacl64/lib/32/crt*.o \
    ${x86_src}/nacl64/lib/32/libcrt*.a \
    ${x86_src}/nacl64/lib/32/intrinsics.o \
    ${PNACL_X8632_ROOT}
  DebugRun ls -l ${PNACL_X8632_ROOT}

  DebugRun Banner "x86-64 native code: ${PNACL_X8664_ROOT}"
  mkdir -p ${PNACL_X8664_ROOT}
  cp -f ${x86_src}/lib/gcc/nacl64/4.4.3/libgcc.a \
    ${x86_src}/nacl64/lib/libc.a \
    ${x86_src}/nacl64/lib/libnosys.a \
    ${x86_src}/nacl64/lib/libnacl.a \
    ${x86_src}/nacl64/lib/crt*.o \
    ${x86_src}/nacl64/lib/libcrt*.a \
    ${x86_src}/nacl64/lib/intrinsics.o \
    ${PNACL_X8664_ROOT}

  DebugRun ls -l ${PNACL_X8664_ROOT}

}

######################################################################
######################################################################
#     < VERIFY >
######################################################################
######################################################################


readonly LLVM_DIS=${LLVM_INSTALL_DIR}/bin/llvm-dis
readonly LLVM_AR=${LLVMGCC_INSTALL_DIR}/bin/${CROSS_TARGET}-ar


# Usage: VerifyObject <regexp> <filename>
# Ensure that the ELF "file format" string for <filename> matches <regexp>.
VerifyObject() {
  local pattern=$1 filename=$2
  echo -n "verify $(basename "$filename") [$pattern]: "
  local format=$(objdump -a "$filename" |
                 sed -ne 's/^.* file format //p' |
                 head -1)
  if echo "$format" | grep -q -e "^$pattern$"; then
    echo "PASS"
  else
    echo "FAIL (got '$format')"
    exit -1
  fi
}


# Usage: VerifyLLVMArchive <filename>
VerifyLLVMArchive() {
  local archive="$1"
  local tmp="/tmp/ar-verify-${RANDOM}"
  rm -rf ${tmp}
  mkdir -p ${tmp}
  cp "${archive}" "${tmp}"
  spushd ${tmp}
  echo -n "verify $(basename "$1"): "
  ${LLVM_AR} x $(basename ${archive})
  for i in *.o ; do
    if [ "$i" = "*.o" ]; then
      echo "FAIL (no object files in ${archive})"
      exit -1
    elif ! ${LLVM_DIS} $i -o $i.ll; then
      echo "FAIL (disassembly of $(pwd)/$i failed)"
      exit -1
    elif grep -ql asm $i.ll; then
      echo "FAIL ($(pwd)/$i.ll contained inline assembly)"
      exit -1
    else
      : ok
    fi
  done
  echo "PASS"
  rm -rf "${tmp}"
  spopd

}

# Usage: VerifyLLVMObj <filename>
VerifyLLVMObj() {
  echo -n "verify $(basename "$1"): "
  t=$(${LLVM_DIS} $1 -o -)

  if  grep asm <<<$t ; then
    echo
    echo "ERROR"
    echo
    exit -1
  fi
  echo "PASS"
}

#
# verify-llvm-archive <archive>
# Verifies that a given archive is bitcode and free of ASMSs
#
verify-llvm-archive() {
  echo "verify $1"
  VerifyLLVMArchive "$@"
}

#
# verify-llvm-object <archive>
#
#   Verifies that a given .o file is bitcode and free of ASMSs
verify-llvm-object() {
  echo "verify $1"
  VerifyLLVMObj "$@"
}

#@-------------------------------------------------------------------------
#+ verify                - Verifies that toolchain/pnacl-untrusted ELF files
#+                         are of the correct architecture.
verify() {
  StepBanner "VERIFY"

  SubBanner "VERIFY: ${PNACL_ARM_ROOT}"
  for i in ${PNACL_ARM_ROOT}/*.[oa] ; do
    VerifyObject 'elf32-little\(arm\)\?' "$i"  # objdumps vary in their output.
  done

  SubBanner "VERIFY: ${PNACL_X8632_ROOT}"
  for i in ${PNACL_X8632_ROOT}/*.[oa] ; do
    VerifyObject elf32-i386  "$i"
  done

  SubBanner "VERIFY: ${PNACL_X8664_ROOT}"
  for i in ${PNACL_X8664_ROOT}/*.[oa] ; do
    VerifyObject elf64-x86-64 "$i"
  done

  SubBanner "VERIFY: ${PNACL_BITCODE_ROOT}"
  for i in ${PNACL_BITCODE_ROOT}/*.a ; do
    VerifyLLVMArchive "$i"
  done

  # we currently do not expect any .o files in this directory
  #for i in ${PNACL_BITCODE_ROOT}/*.o ; do
  #done
}

######################################################################
######################################################################
#
#   < TESTING >
#
######################################################################
######################################################################

if ${UTMAN_DEBUG}; then
  readonly SCONS_ARGS=(MODE=nacl
                       --verbose
                       platform=arm
                       sdl=none
                       bitcode=1)

  readonly SCONS_ARGS_SEL_LDR=(sdl=none
                               --verbose)
else
  readonly SCONS_ARGS=(MODE=nacl
                       platform=arm
                       sdl=none
                       naclsdk_validate=0
                       sysinfo=
                       bitcode=1)

  readonly SCONS_ARGS_SEL_LDR=(sdl=none
                               naclsdk_validate=0
                               sysinfo=)
fi

#@ show-tests            - see what tests can be run
show-tests() {
  StepBanner "SHOWING TESTS"
  cat $(find tests -name nacl.scons) | grep -o 'run_[A-Za-z_-]*' | sort | uniq
}

#@ test-arm-old          - run arm tests via the old toolchain
#@ test-arm-old <test>   - run a single arm test via the old toolchain
test-arm-old() {
  ./scons platform=arm ${SCONS_ARGS_SEL_LDR[@]} sel_ldr
  rm -rf scons-out/nacl-arm
  local fixedargs=""
  if [ $# -eq 0 ] || ([ $# -eq 1 ] && [ "$1" == "-k" ]); then
    fixedargs="smoke_tests"
  fi
  TARGET_CODE=sfi-arm ./scons ${SCONS_ARGS[@]} \
          force_sel_ldr=scons-out/opt-linux-arm/staging/sel_ldr \
          ${fixedargs} "$@"
}

#@ test-arm              - run arm tests via pnacl toolchain
#@ test-arm <test>       - run a single arm test via pnacl toolchain
test-arm() {
  ./scons platform=arm ${SCONS_ARGS_SEL_LDR[@]} sel_ldr
  rm -rf scons-out/nacl-arm

  local fixedargs=""
  if [ $# -eq 0 ] || ([ $# -eq 1 ] && [ "$1" == "-k" ]); then
    fixedargs="smoke_tests"
  fi
  TARGET_CODE=bc-arm ./scons ${SCONS_ARGS[@]} \
          force_sel_ldr=scons-out/opt-linux-arm/staging/sel_ldr \
          ${fixedargs} "$@"
}

#@ test-x86-32           - run x86-32 tests via pnacl toolchain
#@ test-x86-32 <test>    - run a single x86-32 test via pnacl toolchain
test-x86-32() {
  ./scons platform=x86-32 ${SCONS_ARGS_SEL_LDR[@]} sel_ldr
  rm -rf scons-out/nacl-arm

  local fixedargs=""
  if [ $# -eq 0 ] || ([ $# -eq 1 ] && [ "$1" == "-k" ]); then
    fixedargs="smoke_tests"
  fi
  TARGET_CODE=bc-x86-32 ./scons ${SCONS_ARGS[@]} \
          force_emulator= \
          force_sel_ldr=scons-out/opt-linux-x86-32/staging/sel_ldr \
          ${fixedargs} "$@"
}

#@ test-x86-64           - run all x86-64 tests via pnacl toolchain
#@ test-x86-64 <test>    - run a single x86-64 test via pnacl toolchain
test-x86-64() {
  ./scons platform=x86-64 ${SCONS_ARGS_SEL_LDR[@]} sel_ldr
  rm -rf scons-out/nacl-arm

  local fixedargs=""
  if [ $# -eq 0 ] || ([ $# -eq 1 ] && [ "$1" == "-k" ]); then
    fixedargs="smoke_tests"
  fi
  TARGET_CODE=bc-x86-64 ./scons ${SCONS_ARGS[@]} \
          force_emulator= \
          force_sel_ldr=scons-out/opt-linux-x86-64/staging/sel_ldr \
          ${fixedargs} "$@"
}


#@ test-all              - run arm, x86-32, and x86-64 tests. (all should pass)
#@ test-all <test>       - run a single test on all architectures.
test-all() {
  if [ $# -eq 1 ] && [ "$1" == "-k" ]; then
    echo "Using -k on test-all is not a good idea."
    exit -1
  fi

  test-arm-old "$@"
  test-arm "$@"
  test-x86-64 "$@"
  test-x86-32 "$@"
}


#@ test-spec <official-spec-dir> <setup> -  run spec tests
test-spec() {
  official=$(readlink -f $1)
  spushd tests/spec2k
  ./run_all.sh CleanBenchmarks
  ./run_all.sh PoplateFromSpecHarness ${official}
  ./run_all.sh BuildAndRunBenchmarks $2
  spopd
}


#@ test-bot-base         - tests that must pass on the bots to validate a TC
test-bot-base() {
  test-all
}


#@ test-bot-extra <spec-official> - additional tests run on the bots
test-bot-extra() {
  test-spec "$1" SetupNaclX8632Opt
  test-spec "$1" SetupNaclX8632
}
######################################################################
######################################################################
#
# UTILITIES
#
######################################################################
######################################################################

#@-------------------------------------------------------------------------
#@ show-config
show-config() {
  Banner "Config Settings:"
  echo "UTMAN_CONCURRENCY: ${UTMAN_CONCURRENCY}"
  echo "UTMAN_SAFE_MODE:   ${UTMAN_SAFE_MODE}"
  echo "UTMAN_DEBUG:       ${UTMAN_DEBUG}"

  Banner "Your Environment:"
  env | grep UTMAN
}

#@ help                  - Usage information.
help() {
  Usage
}

#@ help-full             - Usage information including internal functions.
help-full() {
  Usage2
}

check-for-trusted() {
   if [ ! -f toolchain/linux_arm-trusted/ld_script_arm_trusted ]; then
     echo '*******************************************************************'
     echo '*    The trusted toolchains dont appear to be installed yet.      *'
     echo '*    These must be installed first.                               *'
     echo '*                                                                 *'
     echo '*    To download and install the trusted toolchain, run:          *'
     echo '*                                                                 *'
     echo '*        $ tools/llvm/utman.sh download-trusted                   *'
     echo '*                                                                 *'
     echo '*    To compile the trusted toolchain, use:                       *'
     echo '*                                                                 *'
     echo '*        $ tools/llvm/trusted-toolchain-creator.sh trusted_sdk    *'
     echo '*                (warning: this takes a while)                    *'
     echo '*******************************************************************'

     exit -1
   fi
}


Banner() {
  echo "**********************************************************************"
  echo "**********************************************************************"
  echo " $@"
  echo "**********************************************************************"
  echo "**********************************************************************"
}

StepBanner() {
  local module="$1"
  if [ $# -eq 1 ]; then
    echo ""
    echo "-------------------------------------------------------------------"
    local padding=$(RepeatStr ' ' 28)
    echo "$padding${module}"
    echo "-------------------------------------------------------------------"
  else
    shift 1
    local padding=$(RepeatStr ' ' $((20-${#module})) )
    echo "${module}$padding" "$@"
  fi
}

RepeatStr() {
  local str="$1"
  local count=$2
  local ret=""

  while [ $count -gt 0 ]; do
    ret="${ret}${str}"
    count=$((count-1))
  done
  echo "$ret"
}

SkipBanner() {
    StepBanner "$1" "Skipping $2, already up to date."
}



SubBanner() {
  echo "----------------------------------------------------------------------"
  echo " $@"
  echo "----------------------------------------------------------------------"
}


Usage() {
  egrep "^#@" $0 | cut --bytes=3-
}

Usage2() {
  egrep "^#(@|\+)" $0 | cut --bytes=3-
}

DebugRun() {
  if ${UTMAN_DEBUG}; then
    "$@"
  fi
}

RunWithLog() {
  local log="${TC_LOG}/$1"

  mkdir -p "${TC_LOG}"

  shift 1
  "$@" 2>&1 | tee "${log}" >> "${TC_LOG_ALL}"
  if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo
    Banner "ERROR"
    echo -n "COMMAND:"
    PrettyPrint "$@"
    echo
    echo "LOGFILE: ${log}"
    echo
    echo "PWD: $(pwd)"
    echo
    exit -1
  fi
}

PrettyPrint() {
  # Pretty print, respecting parameter grouping
  for I in "$@"; do
    local has_space=$(echo "$I" | grep " ")
    if [ ${#has_space} -gt 0 ]; then
      echo -n ' "'
      echo -n "$I"
      echo -n '"'
    else
      echo -n " $I"
    fi
  done
  echo
}

# Silent pushd
spushd() {
  pushd "$1" > /dev/null
}

# Silent popd
spopd() {
  popd > /dev/null
}

# Verbose pushd
vpushd() {
  echo ""
  echo "ENTERING: $1"
  pushd "$1" > /dev/null
}

# Verbose popd
vpopd() {
  #local dir=$(dirs +1)
  echo "LEAVING: $(pwd)"
  echo ""
  popd > /dev/null
}


assert-dir() {
  local dir="$1"
  local msg="$2"

  if [ ! -d "${dir}" ]; then
    Banner "ERROR: ${msg}"
    exit -1
  fi
}

assert-file() {
  local fn="$1"
  local msg="$2"

  if [ ! -d "${fn}" ]; then
    Banner "ERROR: ${fn} does not exist. ${msg}"
    exit -1
  fi
}

# Check if the source for a given build has been modified
ts-modified() {
  local srcdir="$1"
  local objdir="$2"
  local tsfile="${objdir}/${TIMESTAMP_FILENAME}"

  if [ -f "$tsfile" ]; then
    local MODIFIED=$(find "${srcdir}" -newer "${tsfile}")
    [ ${#MODIFIED} -gt 0 ]
    ret=$?
  else
    true
    ret=$?
  fi

  return $ret
}

# Record the time when make begins, but don't yet
# write that to the timestamp file.
# (Just in case make fails)

ts-touch-open() {
  local objdir="$1"
  local tsfile="${objdir}/${TIMESTAMP_FILENAME}"
  local tsfile_open="${objdir}/${TIMESTAMP_FILENAME}_OPEN"

  rm -f "${tsfile}"
  touch "${tsfile_open}"
}


# Write the timestamp. (i.e. make has succeeded)

ts-touch-commit() {
  local objdir="$1"
  local tsfile="${objdir}/${TIMESTAMP_FILENAME}"
  local tsfile_open="${objdir}/${TIMESTAMP_FILENAME}_OPEN"

  mv -f "${tsfile_open}" "${tsfile}"
}


# ts-newer-than dirA dirB
# Compare the make timestamps in both object directories.
# returns true (0) if dirA is newer than dirB
# returns false (1) otherwise.
#
# This functions errs on the side of returning 0, since
# that forces a rebuild anyway.

ts-newer-than() {
  local objdir1="$1"
  local objdir2="$2"

  local tsfile1="${objdir1}/${TIMESTAMP_FILENAME}"
  local tsfile2="${objdir2}/${TIMESTAMP_FILENAME}"

  if [ ! -d "${objdir1}" ]; then return 0; fi
  if [ ! -d "${objdir2}" ]; then return 0; fi

  if [ ! -f "${tsfile1}" ]; then return 0; fi
  if [ ! -f "${tsfile2}" ]; then return 0; fi

  local MODIFIED=$(find "${tsfile1}" -newer "${tsfile2}")
  if [ ${#MODIFIED} -gt 0 ]; then
    return 0
  fi
  return 1
}

BITCODE-ON() {
  export TARGET_CODE=bc-arm
  setup-tools
}

BITCODE-OFF() {
  unset TARGET_CODE
  setup-tools
}

######################################################################
######################################################################
#
#                               < MAIN >
#
######################################################################
######################################################################

PathSanityCheck
PackageCheck
setup-tools

[ $# = 0 ] && set -- help  # Avoid reference to undefined $1.
if [ "$(type -t $1)" != "function" ]; then
  #Usage
  echo "ERROR: unknown function '$1'." >&2
  echo "For help, try:"
  echo "    $0 help"
  exit 1
fi

eval "$@"
