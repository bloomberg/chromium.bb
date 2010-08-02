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

# TODO(pdox): Decide what the target should really permanently be
readonly CROSS_TARGET_ARM=arm-none-linux-gnueabi
readonly CROSS_TARGET_X86_32=i686-none-linux-gnu
readonly CROSS_TARGET_X86_64=x86_64-none-linux-gnu
readonly REAL_CROSS_TARGET=pnacl

readonly INSTALL_ROOT="$(pwd)/toolchain/linux_arm-untrusted"
readonly TARGET_ROOT="${INSTALL_ROOT}/${CROSS_TARGET_ARM}"
readonly ARM_ARCH=armv7-a
readonly ARM_FPU=vfp3
readonly INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET_ARM}"
readonly GCC_VER="4.2.1"

# NOTE: NEWLIB_INSTALL_DIR also server as a SYSROOT
readonly NEWLIB_INSTALL_DIR="${INSTALL_ROOT}/arm-newlib"
readonly DRIVER_INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET_ARM}"

# This toolchain currenlty builds only on linux.
# TODO(abetul): Remove the restriction on developer's OS choices.
readonly NACL_TOOLCHAIN=$(pwd)/toolchain/linux_x86

readonly PATCH_DIR=$(pwd)/tools/patches

readonly BFD_PLUGIN_DIR=${INSTALL_DIR}/lib/bfd-plugins


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
readonly TC_BUILD_LLVM_TOOLS_X8632_SB="${TC_BUILD}/llvm-tools-x8632-sandboxed"
readonly TC_BUILD_BINUTILS_ARM="${TC_BUILD}/binutils-arm"
readonly TC_BUILD_BINUTILS_LIBERTY_X86="${TC_BUILD}/binutils-liberty-x86"
readonly TC_BUILD_BINUTILS_X8632_SB="${TC_BUILD}/binutils-x8632-sandboxed"
readonly TC_BUILD_BINUTILS_X8664_SB="${TC_BUILD}/binutils-x8664-sandboxed"
readonly TC_BUILD_NEWLIB_ARM="${TC_BUILD}/newlib-arm"
readonly TC_BUILD_NEWLIB_BITCODE="${TC_BUILD}/newlib-bitcode"

# This apparently has to be at this location or gcc install breaks.
readonly TC_BUILD_LIBSTDCPP="${TC_BUILD_LLVM_GCC1}/${CROSS_TARGET_ARM}/libstdc++-v3"

readonly TC_BUILD_LIBSTDCPP_BITCODE="${TC_BUILD_LLVM_GCC1}/libstdcpp-bitcode"

# These are fake directories, for storing the timestamp only
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
readonly PNACL_CLIENT_TC_X8632="${PNACL_CLIENT_TC_ROOT}/x8632"
readonly PNACL_CLIENT_TC_X8664="${PNACL_CLIENT_TC_ROOT}/x8664"

# Current milestones within each Hg repo:
readonly LLVM_REV=8d110cbedf4e
readonly LLVM_GCC_REV=34596e3aedfa
readonly NEWLIB_REV=c74ed6d22b4f
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

readonly CROSS_TARGET_AR=${INSTALL_DIR}/bin/${CROSS_TARGET_ARM}-ar
readonly CROSS_TARGET_AS=${INSTALL_DIR}/bin/${CROSS_TARGET_ARM}-as
readonly CROSS_TARGET_LD=${INSTALL_DIR}/bin/${CROSS_TARGET_ARM}-ld
readonly CROSS_TARGET_NM=${INSTALL_DIR}/bin/${CROSS_TARGET_ARM}-nm
readonly CROSS_TARGET_RANLIB=${INSTALL_DIR}/bin/${CROSS_TARGET_ARM}-ranlib
readonly ILLEGAL_TOOL=${DRIVER_INSTALL_DIR}/llvm-fake-illegal

# NOTE: this tools.sh defines: LD_FOR_TARGET, CC_FOR_TARGET, CXX_FOR_TARGET, ...
setup-tools-common() {
  AR_FOR_SFI_TARGET="${CROSS_TARGET_AR}"
  NM_FOR_SFI_TARGET="${CROSS_TARGET_NM}"
  RANLIB_FOR_SFI_TARGET="${CROSS_TARGET_RANLIB}"

  # Preprocessor flags
  CPPFLAGS_FOR_SFI_TARGET="-DMISSING_SYSCALL_NAMES=1"
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


# NOTE: we need to rethink the setup mechanism when we want to
#       produce libgcc for other archs
setup-tools-arm() {
  CC_FOR_SFI_TARGET="${DRIVER_INSTALL_DIR}/llvm-fake-sfigcc -arch arm"
  CXX_FOR_SFI_TARGET="${DRIVER_INSTALL_DIR}/llvm-fake-sfig++ -arch arm"
  # NOTE: this should not be needed, since we do not really fully link anything
  LD_FOR_SFI_TARGET="${ILLEGAL_TOOL}"
  setup-tools-common
}

setup-tools-bitcode() {
  CC_FOR_SFI_TARGET="${DRIVER_INSTALL_DIR}/llvm-fake-sfigcc -emit-llvm"
  CXX_FOR_SFI_TARGET="${DRIVER_INSTALL_DIR}/llvm-fake-sfig++ -emit-llvm"
  # NOTE: this should not be needed, since we do not really fully link anything
  LD_FOR_SFI_TARGET="${ILLEGAL_TOOL}"
  setup-tools-common
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

#@ hg-freshness-check    - Make sure all repos are at the stable revision
hg-freshness-check() {
  hg-pull
  StepBanner "HG-FRESHNESS-CHECK" "Checking for updates..."

  hg-freshness-check-common "${TC_SRC_LLVM}"       ${LLVM_REV}
  hg-freshness-check-common "${TC_SRC_LLVM_GCC}"   ${LLVM_GCC_REV}
  hg-freshness-check-common "${TC_SRC_NEWLIB}"     ${NEWLIB_REV}
  hg-freshness-check-common "${TC_SRC_BINUTILS}"   ${BINUTILS_REV}
}

hg-freshness-check-common() {
  local dir="$1"
  local rev="$2"
  local name=$(basename "${dir}")
  local YESNO

  spushd "${dir}"
  local HGREV=($(hg identify | tr -d '+'))
  spopd
  if [ "${HGREV[0]}" != "$rev" ]; then
    echo "*******************************************************************"
    echo "* Repository hg/${name} is not at the 'stable' revision.           "
    echo "* If this is intentional, enter Y to continue building.            "
    echo "* If not, try: './tools/llvm/utman.sh hg-update-stable-${name}'    "
    echo "*******************************************************************"

    # TODO(pdox): Make this a BUILDBOT flag
    if ${UTMAN_DEBUG}; then
      "hg-update-stable-${name}"
    else
      echo -n "Continue build [Y/N]? "
      read YESNO
      if [ "${YESNO}" != "Y" ] && [ "${YESNO}" != "y" ]; then
        echo "Cancelled build."
        exit -1
      fi
    fi
  fi
}

#@ hg-update-tip         - Update all repos to the tip (may merge)
#@ hg-update-tip-REPO    - Update REPO to the tip
#@                         (REPO can be llvm-gcc, llvm, newlib, binutils)
hg-update-tip() {
  hg-update-tip-llvm-gcc
  hg-update-tip-llvm
  hg-update-tip-newlib
  hg-update-tip-binutils
}

hg-update-tip-llvm-gcc() {
  hg-pull-llvm-gcc
  hg-update-common "${TC_SRC_LLVM_GCC}"
}

hg-update-tip-llvm() {
  hg-pull-llvm
  hg-update-common "${TC_SRC_LLVM}"
}

hg-update-tip-newlib() {
  if hg-update-newlib-confirm; then
    rm -rf "${TC_SRC_NEWLIB}"
    hg-checkout-common ${REPO_NEWLIB}   ${TC_SRC_NEWLIB}   ${NEWLIB_REV}
    hg-update-common "${TC_SRC_NEWLIB}"
    newlib-nacl-headers
  else
    echo "Cancelled."
  fi
}

hg-update-tip-binutils() {
  hg-pull-binutils
  hg-update-common "${TC_SRC_BINUTILS}"
}

#@ hg-update-stable      - Update all repos to the latest stable rev (may merge)
#@ hg-update-stable-REPO - Update REPO to stable
#@                         (REPO can be llvm-gcc, llvm, newlib, binutils)
hg-update-stable() {
  hg-update-stable-llvm-gcc
  hg-update-stable-llvm
  hg-update-stable-newlib
  hg-update-stable-binutils
}

hg-update-stable-llvm-gcc() {
  hg-pull-llvm-gcc
  hg-update-common "${TC_SRC_LLVM_GCC}"  ${LLVM_GCC_REV}
}

hg-update-stable-llvm() {
  hg-pull-llvm
  hg-update-common "${TC_SRC_LLVM}"      ${LLVM_REV}
}

hg-update-stable-newlib() {
  if ${UTMAN_DEBUG} || hg-update-newlib-confirm; then
    rm -rf "${TC_SRC_NEWLIB}"
    hg-checkout-common ${REPO_NEWLIB}   ${TC_SRC_NEWLIB}   ${NEWLIB_REV}
    newlib-nacl-headers
  else
    echo "Cancelled."
  fi
}

hg-update-newlib-confirm() {
  local YESNO
  echo
  echo "Due to special header magic, the newlib repository cannot simply be"
  echo "updated in place. Instead, the entire source directory must be"
  echo "deleted and cloned again."
  echo ""
  echo "WARNING: Local changes to newlib will be lost."
  echo ""
  echo -n "Continue? [Y/N] "
  read YESNO
  [ "${YESNO}" = "Y" ] || [ "${YESNO}" = "y" ]
  return $?
}

hg-update-stable-binutils() {
  hg-pull-binutils
  hg-update-common "${TC_SRC_BINUTILS}"  ${BINUTILS_REV}
}

#@ hg-pull               - Pull all repos. (but do not update working copy)
#@ hg-pull-REPO          - Pull repository REPO.
#@                         (REPO can be llvm-gcc, llvm, newlib, binutils)
hg-pull() {
  StepBanner "HG-PULL" "Running 'hg pull' in all repos..."
  hg-pull-llvm-gcc
  hg-pull-llvm
  hg-pull-newlib
  hg-pull-binutils
}

hg-pull-llvm-gcc() {
  hg-pull-common "${TC_SRC_LLVM_GCC}"
}

hg-pull-llvm() {
  hg-pull-common "${TC_SRC_LLVM}"
}

hg-pull-newlib() {
  hg-pull-common "${TC_SRC_NEWLIB}"
}

hg-pull-binutils() {
  hg-pull-common "${TC_SRC_BINUTILS}"
}

hg-pull-common() {
  local dir="$1"

  assert-dir "$dir" \
    "Repository $(basename "${dir}") doesn't exist. First do 'hg-checkout'"

  spushd "$dir"
  RunWithLog "hg-pull" hg pull
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
  TRUSTED_TOOLCHAIN=native_client/toolchain/linux_arm-trusted/arm-2009q3
  export AR=${TRUSTED_TOOLCHAIN}/bin/arm-none-linux-gnueabi-ar
  export AS=${TRUSTED_TOOLCHAIN}/bin/arm-none-linux-gnueabi-as
  export CC=${TRUSTED_TOOLCHAIN}/bin/arm-none-linux-gnueabi-gcc
  export CXX=${TRUSTED_TOOLCHAIN}/bin/arm-none-linux-gnueabi-g++
  export GYP_DEFINES="target_arch=arm sysroot=${TRUSTED_TOOLCHAIN}/arm-none-linux-gnueabi/libc linux_use_tcmalloc=0 armv7=1 arm_thumb=1"
  export GYP_GENERATORS=make
  export LD=${TRUSTED_TOOLCHAIN}/bin/arm-none-linux-gnueabi-ld
  export RANLIB=${TRUSTED_TOOLCHAIN}/bin/arm-none-linux-gnueabi-ranlib
  # This downloads both toolchains and regenerates gyp.
  gclient runhooks --force
}

#@-------------------------------------------------------------------------
#@ rebuild-pnacl-libs    - organized native libs and build bitcode libs
rebuild-pnacl-libs() {
  clean-pnacl
  newlib-bitcode
  # NOTE: currently also builds some native code
  extrasdk-bitcode
  libstdcpp-bitcode
  organize-native-code
}


#@ everything            - Build and install untrusted SDK.
everything() {

  PathSanityCheck
  check-for-trusted

  hg-checkout
  hg-freshness-check

  clean-install
  RecordRevisionInfo

  clean-logs

  binutils-arm
  llvm
  driver
  gcc-stage1-sysroot
  gcc-stage1 ${CROSS_TARGET_ARM}
  gcc-stage1 ${CROSS_TARGET_X86_32}

  rebuild-pnacl-libs

  # NOTE: we delay the tool building till after the sdk is essentially
  #      complete, so that sdk sanity checks don't fail
  misc-tools

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

  status-helper "NEWLIB-ARM"        newlib-arm

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
  rm  -f "${TARGET_ROOT}"/lib/lib*.a
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
             --target=${CROSS_TARGET_ARM} \
             --prefix=${INSTALL_DIR} \
             --with-llvmgccdir=${INSTALL_DIR}
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
  ln -sf ../../lib/LLVMgold.so "${BFD_PLUGIN_DIR}"
  ln -sf ../../lib/libLTO.so "${BFD_PLUGIN_DIR}"
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
  local target=$1

  if gcc-stage1-needs-configure ${target}; then
    gcc-stage1-clean ${target}
    gcc-stage1-configure ${target}
  else
    SkipBanner "GCC-STAGE1" "configure"
  fi

  if gcc-stage1-needs-make ${target}; then
    gcc-stage1-make ${target}
  else
    SkipBanner "GCC-STAGE1" "make"
  fi

  gcc-stage1-install ${target}
}

#+ gcc-stage1-sysroot    - setup initial sysroot
gcc-stage1-sysroot() {
  StepBanner "GCC-STAGE1" "Setting up initial sysroot"

  local sys_include="${INSTALL_DIR}/${CROSS_TARGET_ARM}/include"
  local sys_include2="${INSTALL_DIR}/${CROSS_TARGET_ARM}/sys-include"

  rm -rf "${sys_include}" "${sys_include2}"
  mkdir -p "${sys_include}"
  ln -sf "${sys_include}" "${sys_include2}"
  cp -r "${TC_SRC_NEWLIB}"/newlib-trunk/newlib/libc/include/* ${sys_include}
}

#+ gcc-stage1-clean      - Clean gcc stage 1
gcc-stage1-clean() {
  StepBanner "GCC-STAGE1" "Clean"
  local target=$1
  local objdir="${TC_BUILD_LLVM_GCC1}-${target}"
  rm -rf "${objdir}"
}

gcc-stage1-needs-configure() {
  local target=$1
  [ ! -f "${TC_BUILD_LLVM_GCC1}-${target}/config.status" ]
  return $?
}

#+ gcc-stage1-configure  - Configure GCC stage 1
gcc-stage1-configure() {
  local target=$1
  StepBanner "GCC-STAGE1" "Configure $target"

  local srcdir="${TC_SRC_LLVM_GCC}"
  local objdir="${TC_BUILD_LLVM_GCC1}-${target}"

  mkdir -p "${objdir}"
  spushd "${objdir}"

  # NOTE: hack, assuming presence of x86/32 toolchain (used for both 32/64)
  local x86_32_as="${INSTALL_ROOT}/../linux_x86/bin/nacl-as"
  local x86_64_as="${INSTALL_ROOT}/../linux_x86/bin/nacl64-as"
  local config_opts=""
  case ${target} in
      arm-*)
          config_opts="--with-arch=${ARM_ARCH} --with-fpu=${ARM_FPU}"
          ;;
      i686-*)
          config_opts="--with-as=${x86_32_as}"
          ;;
      x86_64-*)
          config_opts="--with-as=${x86_64_as}"
          ;;
  esac

  # TODO(robertm): do we really need CROSS_TARGET_*
  RunWithLog llvm-pregcc-${target}.configure \
      env -i PATH=/usr/bin/:/bin \
             CC=${CC} \
             CXX=${CXX} \
             CFLAGS="-Dinhibit_libc" \
             ${srcdir}/llvm-gcc-4.2/configure \
               --prefix=${INSTALL_DIR} \
               --enable-llvm=${INSTALL_DIR} \
               --with-newlib \
               --disable-libmudflap \
               --disable-decimal-float \
               --disable-libssp \
               --disable-libgomp \
               --enable-languages=c,c++ \
               --disable-threads \
               --disable-libstdcxx-pch \
               --disable-shared \
               --without-headers \
               ${config_opts} \
               --target=${target} \
               --with-sysroot="${NEWLIB_INSTALL_DIR}"

  spopd
}

gcc-stage1-needs-make() {
  local srcdir="${TC_SRC_LLVM_GCC}"
  local objdir="${TC_BUILD_LLVM_GCC1}-${target}"
  local target=$1

  # In safe mode, rebuild gcc-stage1 when LLVM is updated.
  if ${UTMAN_SAFE_MODE}; then
    speculative-check "llvm" && return 0
    ts-newer-than "${TC_BUILD_LLVM}" \
                  "${TC_BUILD_LLVM_GCC1}-${target}" && return 0
  fi

  ts-modified "$srcdir" "$objdir"
  return $?
}

gcc-stage1-make() {
  StepBanner "GCC-STAGE1" "Make"

  local target=$1
  local srcdir="${TC_SRC_LLVM_GCC}"
  local objdir="${TC_BUILD_LLVM_GCC1}-${target}"
  spushd ${objdir}

  ts-touch-open "${objdir}"

  mkdir -p "${objdir}/dummy-bin"
  ln -s ${CROSS_TARGET_AR} "${objdir}/dummy-bin/${target}-ar"
  ln -s ${CROSS_TARGET_RANLIB} "${objdir}/dummy-bin/${target}-ranlib"

  # NOTE: we add ${INSTALL_DIR}/bin to PATH
  RunWithLog llvm-pregcc-${target}.make \
       env -i PATH=/usr/bin/:/bin:${INSTALL_DIR}/bin:${objdir}/dummy-bin \
              CC=${CC} \
              CXX=${CXX} \
              CFLAGS="-Dinhibit_libc" \
              make ${MAKE_OPTS} all-gcc

  xgcc-patch ${target}

  # NOTE: This builds more than what we need right now. For example,
  # libstdc++ is unused (we always use the bitcode one). This might change
  # when we start supporting shared libraries.
  RunWithLog llvm-pregcc2-${target}.make \
       env -i PATH=/usr/bin/:/bin:${INSTALL_DIR}/bin:${objdir}/dummy-bin \
              CC=${CC} \
              CXX=${CXX} \
              CFLAGS="-Dinhibit_libc" \
              make ${MAKE_OPTS} all

  rm gcc/xgcc
  mv gcc/xgcc-real gcc/xgcc

  ts-touch-commit "${objdir}"

  spopd
}

#+ xgcc-patch          - Patch xgcc and clean libgcc
xgcc-patch() {
  # This is a hack. Ideally gcc would be configured with a pnacl-nacl target
  # and xgcc would produce sfi code automatically. This is not the case, so
  # we cheat gcc's build by replacing xgcc behind its back.
  local target=$1
  StepBanner "GCC-STAGE1" "Patching xgcc and cleaning libgcc $target"

  local arch=""
  local driver=""

  case ${target} in
      arm-*)
          arch="arm"
          ;;
      i686-*)

          arch="x86-32"
          ;;
      x86_64-*)
          arch="x86-64"
          ;;
  esac
  mv gcc/xgcc gcc/xgcc-real
  # N.B:
  # * We have to use the arch cc1 istead of the more common practice of using
  # the ARM cc1 for everything. The reason is that the ARM cc1 will reject a
  # asm that uses ebx for example
  # * Because of the previous item, we have to use the arch driver bacuse
  # the ARM driver will pass options like -mfpu that the x86 cc1 cannot handle.
  cat > gcc/xgcc <<EOF
#!/bin/sh

DIR="\$(readlink -mn \${0%/*})"
DIR2="\${DIR}/../../../../toolchain/linux_arm-untrusted/arm-none-linux-gnueabi"
\${DIR2}/llvm-fake-sfigcc --driver=\${DIR}/xgcc-real -arch ${arch} ${CPPFLAGS_FOR_SFI_TARGET} "\$@"
EOF
  chmod 755 gcc/xgcc

  RunWithLog libgcc-${target}.clean \
      env -i PATH=/usr/bin/:/bin:${INSTALL_DIR}/bin \
             make clean-target-libgcc
}

#+ gcc-stage1-install    - Install GCC stage 1
gcc-stage1-install() {
  local target=$1
  StepBanner "GCC-STAGE1" "Install $target"

  local objdir="${TC_BUILD_LLVM_GCC1}-${target}"
  spushd "${TC_BUILD_LLVM_GCC1}-${target}"

  # NOTE: we add ${INSTALL_DIR}/bin to PATH
  RunWithLog llvm-pregcc-${target}.install \
       env -i PATH=/usr/bin/:/bin:${INSTALL_DIR}/bin:${objdir}/dummy-bin \
              CC=${CC} \
              CXX=${CXX} \
              CFLAGS="-Dinhibit_libc" \
              make ${MAKE_OPTS} install

  spopd
}


#########################################################################
#########################################################################
#                          < LIBSTDCPP-BITCODE >
#########################################################################
#########################################################################

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

#+ libstdcpp-bitcode-clean - clean libstdcpp in bitcode
libstdcpp-bitcode-clean() {
  StepBanner "LIBSTDCPP-BITCODE" "Clean"
  rm -rf "${TC_BUILD_LIBSTDCPP_BITCODE}"
}

libstdcpp-bitcode-needs-configure() {
  speculative-check "gcc-stage1" && return 0
  ts-newer-than "${TC_BUILD_LLVM_GCC1}-${CROSS_TARGET_ARM}" \
                "${TC_BUILD_LIBSTDCPP_BITCODE}" && return 0
  [ ! -f "${TC_BUILD_LIBSTDCPP_BITCODE}/config.status" ]
  return #?
}

#+ libstdcpp-bitcode-configure - configure libstdcpp for bitcode
libstdcpp-bitcode-configure() {
  StepBanner "LIBSTDCPP-BITCODE" "Configure"
  setup-tools-bitcode
  libstdcpp-configure-common "${TC_BUILD_LIBSTDCPP_BITCODE}"
  setup-tools-arm
}

libstdcpp-configure-common() {
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="$1"

  mkdir -p "${objdir}"
  spushd "${objdir}"

  RunWithLog llvm-gcc.configure_libstdcpp \
      env -i PATH=/usr/bin/:/bin:${INSTALL_DIR}/bin \
        "${STD_ENV_FOR_LIBSTDCPP[@]}" \
        "${srcdir}"/configure \
          --host=${CROSS_TARGET_ARM} \
          --prefix=${INSTALL_DIR} \
          --enable-llvm=${INSTALL_DIR} \
          --with-newlib \
          --disable-libstdcxx-pch \
          --disable-shared \
          --enable-languages=c,c++ \
          --target=${CROSS_TARGET_ARM} \
          --with-sysroot=${NEWLIB_INSTALL_DIR} \
          --with-arch=${ARM_ARCH} \
          --srcdir="${srcdir}"
  spopd
}

libstdcpp-bitcode-needs-make() {
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="${TC_BUILD_LIBSTDCPP_BITCODE}"

  ts-modified "$srcdir" "$objdir"
  return $?
}

#+ libstdcpp-bitcode-make - Make libstdcpp in bitcode
libstdcpp-bitcode-make() {
  StepBanner "LIBSTDCPP-BITCODE" "Make"
  setup-tools-bitcode
  libstdcpp-make-common "${TC_BUILD_LIBSTDCPP_BITCODE}"
  setup-tools-arm
}

libstdcpp-make-common() {
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="$1"

  ts-touch-open "${objdir}"

  spushd "${objdir}"
  RunWithLog llvm-gcc.make_libstdcpp \
    env -i PATH=/usr/bin/:/bin:${INSTALL_DIR}/bin \
        make \
        "${STD_ENV_FOR_LIBSTDCPP[@]}" \
        ${MAKE_OPTS}
  spopd

  ts-touch-commit "${objdir}"
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
    ${srcdir}/binutils-2.20/configure --prefix=${INSTALL_DIR} \
                                      --target=${CROSS_TARGET_ARM} \
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
  StepBanner "BINUTILS-ARM" "Install"
  local objdir="${TC_BUILD_BINUTILS_ARM}"
  spushd "${objdir}"

  RunWithLog binutils.arm.install \
    env -i PATH="/usr/bin:/bin" \
    make \
      install ${MAKE_OPTS}

  spopd
}

#+-------------------------------------------------------------------------
#+ binutils-liberty-x86          - Build and install binutils-libiberty for X86
binutils-liberty-x86() {
  local srcdir="${TC_SRC_BINUTILS}"

  assert-dir "${srcdir}" "You need to checkout binutils."

  if binutils-liberty-x86-needs-configure; then
    binutils-liberty-x86-clean
    binutils-liberty-x86-configure
  else
    SkipBanner "BINUTILS-LIBERTY-X86" "configure"
  fi

  if binutils-liberty-x86-needs-make; then
    binutils-liberty-x86-make
  else
    SkipBanner "BINUTILS-LIBERTY-X86" "make"
  fi

  binutils-liberty-x86-install
}

binutils-liberty-x86-needs-configure() {
  [ ! -f "${TC_BUILD_BINUTILS_LIBERTY_X86}/config.status" ]
  return $?
}

#+ binutils-liberty-x86-clean    - Clean binutils-liberty
binutils-liberty-x86-clean() {
  StepBanner "BINUTILS-LIBERTY-X86" "Clean"
  local objdir="${TC_BUILD_BINUTILS_LIBERTY_X86}"
  rm -rf ${objdir}
}

#+ binutils-liberty-x86-configure - Configure binutils-liberty for X86
binutils-liberty-x86-configure() {
  StepBanner "BINUTILS-LIBERTY-X86" "Configure"

  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS_LIBERTY_X86}"

  mkdir -p "${objdir}"
  spushd "${objdir}"

  RunWithLog binutils.liberty.x86.configure \
    env -i \
    PATH="/usr/bin:/bin" \
    CC=${CC} \
    CXX=${CXX} \
    ${srcdir}/binutils-2.20/configure --prefix=${PNACL_CLIENT_TC_ROOT} \
                                      --with-sysroot=${NEWLIB_INSTALL_DIR}
  spopd
}

binutils-liberty-x86-needs-make() {
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS_LIBERTY_X86}"

  ts-modified "$srcdir" "$objdir"
  return $?
}

#+ binutils-liberty-x86-make     - Make binutils-liberty for X86
binutils-liberty-x86-make() {
  StepBanner "BINUTILS-LIBERTY-X86" "Make"
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS_LIBERTY_X86}"
  spushd "${objdir}"

  ts-touch-open "${objdir}"

  RunWithLog binutils.liberty.x86.make \
    env -i PATH="/usr/bin:/bin" \
    make ${MAKE_OPTS} all-libiberty

  ts-touch-commit "${objdir}"

  spopd
}

#+ binutils-liberty-x86-install  - Install binutils-liberty for X86
binutils-liberty-x86-install() {
  StepBanner "BINUTILS-LIBERTY-X86" "Install"
  local objdir="${TC_BUILD_BINUTILS_LIBERTY_X86}"
  spushd "${objdir}"

  RunWithLog binutils.liberty.x86.install \
    env -i PATH="/usr/bin:/bin" \
    make install-libiberty ${MAKE_OPTS}

  spopd
}

#########################################################################
#     CLIENT BINARIES (SANDBOXED)
#########################################################################

#+-------------------------------------------------------------------------
#+ llvm-tools-x8632-sb       - Build and install x8632 llvm tools (sandboxed)
llvm-tools-x8632-sb() {
  StepBanner "LLVM-TOOLS-X8632-SB"

  local srcdir="${TC_SRC_LLVM}"

  assert-dir "${srcdir}" "You need to checkout llvm."

  if [ ! -d ${NACL_TOOLCHAIN} ] ; then
    echo "ERROR: install Native Client toolchain"
    exit -1
  fi

  llvm-tools-x8632-sb-clean
  llvm-tools-x8632-sb-configure
  llvm-tools-x8632-sb-make
  llvm-tools-x8632-sb-install
}

llvm-tools-x8632-sb-needs-configure() {
  [ ! -f "${TC_BUILD_LLVM_TOOLS_X8632_SB}/config.status" ]
  return $?
}

#+ llvm-tools-x8632-sb-clean - Clean x8632 llvm tools (sandboxed)
llvm-tools-x8632-sb-clean() {
  StepBanner "LLVM-TOOLS-X8632-SB" "Clean"
  local objdir="${TC_BUILD_LLVM_TOOLS_X8632_SB}"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"
}

#+ llvm-tools-x8632-sb-configure - Configure x8632 llvm tools (sandboxed)
llvm-tools-x8632-sb-configure() {
  StepBanner "LLVM-TOOLS-X8632-SB" "Configure"
  local srcdir="${TC_SRC_LLVM}"
  local objdir="${TC_BUILD_LLVM_TOOLS_X8632_SB}"

  spushd ${objdir}
  RunWithLog \
    llvm.tools.x8632.sandboxed.configure \
    env -i \
    PATH="/usr/bin:/bin" \
    AR="${NACL_TOOLCHAIN}/bin/nacl-ar" \
    AS="${NACL_TOOLCHAIN}/bin/nacl-as" \
    CC="${NACL_TOOLCHAIN}/bin/nacl-gcc -DPNACL_TOOLCHAIN_SANDBOX=1 -m32 -O2 -static -I${NACL_TOOLCHAIN}/nacl/include" \
    CXX="${NACL_TOOLCHAIN}/bin/nacl-g++ -DPNACL_TOOLCHAIN_SANDBOX=1 -m32 -O2 -static -I${NACL_TOOLCHAIN}/nacl/include" \
    EMULATOR_FOR_BUILD="$(pwd)/scons-out/opt-linux-x86-32/staging/sel_ldr -d" \
    LD="${NACL_TOOLCHAIN}/bin/nacl-ld" \
    RANLIB="${NACL_TOOLCHAIN}/bin/nacl-ranlib" \
    LDFLAGS="-s" \
    ${srcdir}/llvm-trunk/configure \
                             --prefix=${PNACL_CLIENT_TC_X8632} \
                             --host=nacl \
                             --target=${CROSS_TARGET_ARM} \
                             --disable-jit \
                             --enable-optimized \
                             --enable-targets=x86,x86_64 \
                             --enable-static \
                             --enable-threads=no \
                             --enable-pic=no \
                             --enable-shared=no \
                             --with-sysroot=${NEWLIB_INSTALL_DIR}
  spopd
}

llvm-tools-x8632-sb-needs-make() {
  local srcdir="${TC_SRC_LLVM}"
  local objdir="${TC_BUILD_LLVM_TOOLS_X8632_SB}"

  ts-modified "$srcdir" "$objdir"
  return $?
}

#+ llvm-tools-x8632-sb-make - Make x8632 llvm tools (sandboxed)
llvm-tools-x8632-sb-make() {
  StepBanner "LLVM-TOOLS-X8632-SB" "Make"
  local objdir="${TC_BUILD_LLVM_TOOLS_X8632_SB}"
  spushd ${objdir}

  ts-touch-open "${objdir}"

  RunWithLog llvm.tools.x8632.sandboxed.make \
    env -i PATH="/usr/bin:/bin" \
    make VERBOSE=1 tools-only

  ts-touch-commit "${objdir}"

  spopd
}

#+ llvm-tools-x8632-sb-install - Install x8632 llvm tools (sandboxed)
llvm-tools-x8632-sb-install() {
  StepBanner "LLVM-TOOLS-X8632-SB" "Install"
  local objdir="${TC_BUILD_LLVM_TOOLS_X8632_SB}"
  spushd ${objdir}

  RunWithLog llvm.tools.x8632.sandboxed.install \
    env -i PATH="/usr/bin:/bin" \
    make install

  spopd
}

#+-------------------------------------------------------------------------
#+ binutils-x8632-sb       - Build and install binutils (sandboxed) for x8632
binutils-x8632-sb() {
  local srcdir="${TC_SRC_BINUTILS}"

  assert-dir "${srcdir}" "You need to checkout binutils."

  if [ ! -d ${NACL_TOOLCHAIN} ] ; then
    echo "ERROR: install Native Client toolchain"
    exit -1
  fi

  if [ ! -f ${PNACL_CLIENT_TC_ROOT}/lib/libiberty.a ] ; then
    echo "ERROR: missing lib. Run this script with  binutils-liberty-x86 option"
    exit -1
  fi

  if binutils-x8632-sb-needs-configure; then
    binutils-x8632-sb-clean
    binutils-x8632-sb-configure
  else
    SkipBanner "BINUTILS-X8632-SB" "configure"
  fi

  if binutils-x8632-sb-needs-make; then
    binutils-x8632-sb-make
  else
    SkipBanner "BINUTILS-X8632-SB" "make"
  fi

  binutils-x8632-sb-install
}

binutils-x8632-sb-needs-configure() {
  [ ! -f "${TC_BUILD_BINUTILS_X8632_SB}/config.status" ]
  return $?
}

#+ binutils-x8632-sb-clean - Clean binutils (sandboxed) for x8632
binutils-x8632-sb-clean() {
  StepBanner "BINUTILS-X8632-SB" "Clean"
  local objdir="${TC_BUILD_BINUTILS_X8632_SB}"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"
}

#+ binutils-x8632-sb-configure - Configure binutils (sandboxed) for x8632
binutils-x8632-sb-configure() {
  StepBanner "BINUTILS-X8632-SB" "Configure"
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS_X8632_SB}"

  mkdir ${TC_BUILD_BINUTILS_X8632_SB}/opcodes
  spushd ${objdir}
  cp ${PNACL_CLIENT_TC_ROOT}/lib/libiberty.a ./opcodes/.
  RunWithLog \
    binutils.x8632.sandboxed.configure \
    env -i \
    PATH="/usr/bin:/bin" \
    AR="${NACL_TOOLCHAIN}/bin/nacl-ar" \
    AS="${NACL_TOOLCHAIN}/bin/nacl-as" \
    CC="${NACL_TOOLCHAIN}/bin/nacl-gcc" \
    CXX="${NACL_TOOLCHAIN}/bin/nacl-g++" \
    LD="${NACL_TOOLCHAIN}/bin/nacl-ld" \
    RANLIB="${NACL_TOOLCHAIN}/bin/nacl-ranlib" \
    CFLAGS="-m32 -O2 -DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5 -DNACL_TOOLCHAIN_PATCH -DPNACL_TOOLCHAIN_SANDBOX -I${NACL_TOOLCHAIN}/nacl/include" \
    LDFLAGS="-s" \
    LDFLAGS_FOR_BUILD="-L ." \
    ${srcdir}/binutils-2.20/configure \
                             --prefix=${PNACL_CLIENT_TC_X8632} \
                             --host=nacl \
                             --target=nacl \
                             --disable-nls \
                             --enable-static \
                             --enable-shared=no \
                             --with-sysroot=${NEWLIB_INSTALL_DIR}
  spopd
}

binutils-x8632-sb-needs-make() {
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS_X8632_SB}"

  ts-modified "$srcdir" "$objdir"
  return $?
}

#+ binutils-x8632-sb-make - Make binutils (sandboxed) for x8632
binutils-x8632-sb-make() {
  StepBanner "BINUTILS-X8632-SB" "Make"
  local objdir="${TC_BUILD_BINUTILS_X8632_SB}"
  spushd ${objdir}

  ts-touch-open "${objdir}"

  RunWithLog binutils.x8632.sandboxed.make \
    env -i PATH="/usr/bin:/bin" \
    make ${MAKE_OPTS} all-gas

  ts-touch-commit "${objdir}"

  spopd
}

#+ binutils-x8632-sb-install - Install binutils (sandboxed) for x8632
binutils-x8632-sb-install() {
  StepBanner "BINUTILS-X8632-SB" "Install"
  local objdir="${TC_BUILD_BINUTILS_X8632_SB}"
  spushd ${objdir}

  RunWithLog binutils.x8632.sandboxed.install \
    env -i PATH="/usr/bin:/bin" \
    make install-gas

  spopd
}

#+-------------------------------------------------------------------------
#TODO(abetul): Share more code with binutils 32-bit sandboxing.
#+ binutils-x8664-sb       - Build and install binutils (sandboxed) for x8664
binutils-x8664-sb() {
  local srcdir="${TC_SRC_BINUTILS}"

  assert-dir "${srcdir}" "You need to checkout binutils."

  if [ ! -d ${NACL_TOOLCHAIN} ] ; then
    echo "ERROR: install Native Client toolchain"
    exit -1
  fi

  if [ ! -f ${PNACL_CLIENT_TC_ROOT}/lib/libiberty.a ] ; then
    echo "ERROR: missing lib. Run this script with  binutils-liberty-x86 option"
    exit -1
  fi

  if binutils-x8664-sb-needs-configure; then
    binutils-x8664-sb-clean
    binutils-x8664-sb-configure
  else
    SkipBanner "BINUTILS-X8664-SB" "configure"
  fi

  if binutils-x8664-sb-needs-make; then
    binutils-x8664-sb-make
  else
    SkipBanner "BINUTILS-X8664-SB" "make"
  fi

  binutils-x8664-sb-install
}

binutils-x8664-sb-needs-configure() {
  [ ! -f "${TC_BUILD_BINUTILS_X8664_SB}/config.status" ]
  return $?
}

#+ binutils-x8664-sb-clean - Clean binutils (sandboxed) for x8664
binutils-x8664-sb-clean() {
  StepBanner "BINUTILS-X8664-SB" "Clean"
  local objdir="${TC_BUILD_BINUTILS_X8664_SB}"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"
}

#+ binutils-x8664-sb-configure - Configure binutils (sandboxed) for x8664
binutils-x8664-sb-configure() {
  StepBanner "BINUTILS-X8664-SB" "Configure"
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS_X8664_SB}"

  mkdir ${TC_BUILD_BINUTILS_X8664_SB}/opcodes
  spushd ${objdir}
  cp ${PNACL_CLIENT_TC_ROOT}/lib/libiberty.a ./opcodes/.
  RunWithLog \
    binutils.x8664.sandboxed.configure \
    env -i \
    PATH="/usr/bin:/bin" \
    AR="${NACL_TOOLCHAIN}/bin/nacl64-ar" \
    AS="${NACL_TOOLCHAIN}/bin/nacl64-as" \
    CC="${NACL_TOOLCHAIN}/bin/nacl64-gcc" \
    CXX="${NACL_TOOLCHAIN}/bin/nacl64-g++" \
    LD="${NACL_TOOLCHAIN}/bin/nacl64-ld" \
    RANLIB="${NACL_TOOLCHAIN}/bin/nacl64-ranlib" \
    CFLAGS="-m64 -O2 -DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5 -DNACL_TOOLCHAIN_PATCH -DPNACL_TOOLCHAIN_SANDBOX -I${NACL_TOOLCHAIN}/nacl64/include" \
    LDFLAGS="-s" \
    LDFLAGS_FOR_BUILD="-L." \
    ${srcdir}/binutils-2.20/configure \
                             --prefix=${PNACL_CLIENT_TC_X8664} \
                             --host=nacl64 \
                             --target=nacl64 \
                             --disable-nls \
                             --enable-static \
                             --enable-shared=no \
                             --with-sysroot=${NEWLIB_INSTALL_DIR}
  spopd
}

binutils-x8664-sb-needs-make() {
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS_X8664_SB}"

  ts-modified "$srcdir" "$objdir"
  return $?
}

#+ binutils-x8664-sb-make - Make binutils (sandboxed) for x8664
binutils-x8664-sb-make() {
  StepBanner "BINUTILS-X8664-SB" "Make"
  local objdir="${TC_BUILD_BINUTILS_X8664_SB}"
  spushd ${objdir}

  ts-touch-open "${objdir}"

  RunWithLog binutils.x8664.sandboxed.make \
    env -i PATH="/usr/bin:/bin" \
    make ${MAKE_OPTS} all-gas

  ts-touch-commit "${objdir}"

  spopd
}

#+ binutils-x8664-sb-install - Install binutils (sandboxed) for x8664
binutils-x8664-sb-install() {
  StepBanner "BINUTILS-X8664-SB" "Install"
  local objdir="${TC_BUILD_BINUTILS_X8664_SB}"
  spushd ${objdir}

  RunWithLog binutils.x8664.sandboxed.install \
    env -i PATH="/usr/bin:/bin" \
    make install-gas

  spopd
}

#+-------------------------------------------------------------------------
#@ build-sandboxed-translators - Build and package up sandboxed translators
build-sandboxed-translators() {
  StepBanner "BUILD SANDBOXED TRANSLATORS"

  # TODO(abetul): Missing arm translator work.
  # StepBanner "ARM" "Sandboxing"

  StepBanner "X86" "Prepare"
  StepBanner "---" "-------"
  binutils-liberty-x86
  echo ""

  StepBanner "32-bit X86" "Sandboxing"
  StepBanner "----------" "----------"
  binutils-x8632-sb
  cp tools/llvm/dummy_translator_x8632.sh "${PNACL_CLIENT_TC_X8632}/translator"
  echo ""

  StepBanner "64-bit X86" "Sandboxing"
  StepBanner "----------" "----------"
  binutils-x8664-sb
  cp tools/llvm/dummy_translator_x8664.sh "${PNACL_CLIENT_TC_X8664}/translator"
  echo ""
}

# TODO(abetul): Make sure this works with arm when it's ready to demo.
# TODO(abetul): Replace ld and llc with sandboxed executables.
#+-------------------------------------------------------------------------
#@ copy-translator-to-dir <arch> <dir> - Copies <arch> translator components to <dir>
copy-translator-to-dir() {
  StepBanner "COPY TRANSLATOR TO DIRECTORY"

  if [ $# != 2 ]; then
    echo "Usage copy-translator-to-dir <arch> <dir>"
    exit -1
  fi

  assert-dir "${PNACL_CLIENT_TC_X8632}" \
        "Run this script with build-sandboxed-translators"
  assert-dir "${PNACL_CLIENT_TC_X8664}" \
        "Run this script with build-sandboxed-translators"

  local arch=$1
  local dir_name=$2
  local bindir="${PNACL_CLIENT_TC_ROOT}/${arch}"
  assert-dir "${bindir}" "Unsupported arch. Choose one of: x8632, x8664"

  if ! mkdir -p "${dir_name}" ; then
     echo "ERROR: ${dir_name} can't be created."
    exit -1
  fi

  assert-file "${TARGET_ROOT}/bin/llc" "Install PNaCl toolchain."
  cp "${TARGET_ROOT}/bin/llc" "${dir_name}"

  assert-file "${bindir}/bin/as" \
        "Run this script with build-sandboxed-translators"
  cp "${bindir}/bin/as" "${dir_name}"

  assert-file "${TARGET_ROOT}/bin/arm-none-linux-gnueabi-ld" \
        "Install PNaCl toolchain."
  cp "${TARGET_ROOT}/bin/arm-none-linux-gnueabi-ld" "${dir_name}/ld"

  assert-dir "${PNACL_TOOLCHAIN_ROOT}/libs-${arch}" \
        "Install PNaCl toolchain."
  cp "${PNACL_TOOLCHAIN_ROOT}/libs-${arch}/"* "${dir_name}/."

  assert-file "$(pwd)/tools/llvm/ld_script_${arch}_untrusted" \
        "Install NaCl toolchain."
  cp "$(pwd)/tools/llvm/ld_script_${arch}_untrusted" "${dir_name}/ld_script"

  assert-file "${bindir}/translator" \
        "Run this script with build-sandboxed-translators"
  cp "${bindir}/translator" "${dir_name}"

  echo "Done"
}

#########################################################################
#     < NEWLIB-ARM >
#     < NEWLIB-BITCODE >
#########################################################################

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

#+ newlib-bitcode-clean  - Clean bitcode newlib.
newlib-bitcode-clean() {
  StepBanner "NEWLIB-BITCODE" "Clean"
  rm -rf "${TC_BUILD_NEWLIB_BITCODE}"
}

newlib-bitcode-needs-configure() {
  speculative-check "gcc-stage1" && return 0
  ts-newer-than "${TC_BUILD_LLVM_GCC1}-${CROSS_TARGET_ARM}" \
                   "${TC_BUILD_NEWLIB_BITCODE}" && return 0

  [ ! -f "${TC_BUILD_NEWLIB_BITCODE}/config.status" ]
  return #?
}

#+ newlib-bitcode-configure - Configure bitcode Newlib
newlib-bitcode-configure() {
  StepBanner "NEWLIB-BITCODE" "Configure"
  setup-tools-bitcode
  newlib-configure-common "${TC_BUILD_NEWLIB_BITCODE}"
  setup-tools-arm
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
    ${srcdir}/newlib-trunk/configure \
        --disable-libgloss \
        --disable-multilib \
        --enable-newlib-reent-small \
        --prefix="${NEWLIB_INSTALL_DIR}" \
        --disable-newlib-supplied-syscalls \
        --disable-texinfo \
        --target="${REAL_CROSS_TARGET}"
  spopd
}

newlib-bitcode-needs-make() {
  local srcdir="${TC_SRC_NEWLIB}"
  local objdir="${TC_BUILD_NEWLIB_BITCODE}"

  ts-modified "$srcdir" "$objdir"
  return $?
}

#+ newlib-bitcode-make   - Make bitcode Newlib
newlib-bitcode-make() {
  StepBanner "NEWLIB-BITCODE" "Make"

  setup-tools-bitcode
  newlib-make-common "${TC_BUILD_NEWLIB_BITCODE}"
  setup-tools-arm
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

#+ newlib-bitcde-install    - Install Bitcode Newlib
newlib-bitcode-install() {
  StepBanner "NEWLIB-BITCODE" "Install"

  local objdir="${TC_BUILD_NEWLIB_BITCODE}"

  spushd "${objdir}"

  # NOTE: we might be better off not using install, as we are already
  #       doing a bunch of copying of headers and libs further down
  RunWithLog newlib.install \
    env -i PATH="/usr/bin:/bin" \
      make \
      "${STD_ENV_FOR_NEWLIB[@]}" \
      install ${MAKE_OPTS}

  ###########################################################
  #                -- HACK HACK HACK --
  # newlib installs into ${REAL_CROSS_TARGET}
  # For now, move it back to the old ${CROSS_TARGET_ARM}
  # where everything expects it to be.
  rm -rf "${NEWLIB_INSTALL_DIR}/${CROSS_TARGET_ARM}"
  mv "${NEWLIB_INSTALL_DIR}/${REAL_CROSS_TARGET}" \
     "${NEWLIB_INSTALL_DIR}/${CROSS_TARGET_ARM}"
  ###########################################################

  StepBanner "NEWLIB-BITCODE" "Extra-install"
  local sys_include=${INSTALL_DIR}/${CROSS_TARGET_ARM}/include
  local sys_lib=${INSTALL_DIR}/${CROSS_TARGET_ARM}/lib
  # NOTE: we provide a new one via extra-sdk
  rm ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET_ARM}/include/pthread.h

  cp ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET_ARM}/include/machine/endian.h \
    ${sys_include}
  cp ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET_ARM}/include/sys/param.h \
    ${sys_include}
  cp ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET_ARM}/include/newlib.h \
    ${sys_include}

  # NOTE: we provide our own pthread.h via extra-sdk
  StepBanner "NEWLIB-BITCODE" "Removing old pthreads headers"
  rm -f "${NEWLIB_INSTALL_DIR}/${CROSS_TARGET_ARM}/usr/include/pthread.h"
  rm -f "${sys_include}/pthread.h"

  StepBanner "NEWLIB-BITCODE" "copying libraries"
  local destdir="${PNACL_BITCODE_ROOT}"
  # We only install libc/libg/libm
  mkdir -p "${destdir}"
  cp ${objdir}/${REAL_CROSS_TARGET}/newlib/lib[cgm].a "${destdir}"

  spopd
}


#########################################################################
#     < EXTRASDK-BITCODE >
#########################################################################
# NOTE: the EXTRASDK-BITCODE does a little bit of double duty: it
#       produces both bitcode and since we are using bc-arm
#       also some native arm obj/lib
# TODO(robertm): sepatate those two duties

#+ extrasdk-bitcode-clean  - Clean bitcode extra-sdk
#+                         and also the arm native libraries

# NOTE: this function does a little bit of double duty: it produces
#       the bitcode parts of the nacl_sdk and a few additional native
#       object like the startup code files.
#       It would be nice to disentangle those but this will require more
#       surgery in SConstruct and other places
extrasdk-bitcode-clean() {
  StepBanner "EXTRASDK-BITCODE" "Clean"
  rm -rf "${TC_BUILD_EXTRASDK_BITCODE}"
}

#+ extrasdk-bitcode      - Build and install extra SDK libs (bitcode)
extrasdk-bitcode() {
  StepBanner "EXTRASDK-BITCODE"

  if extrasdk-bitcode-needs-make; then
    extrasdk-bitcode-make
  fi
  extrasdk-bitcode-install
}


extrasdk-bitcode-needs-make() {
  speculative-check "gcc-stage1" && return 0
  ts-newer-than "${TC_BUILD_LLVM_GCC1}-${CROSS_TARGET_ARM}" \
                "${TC_BUILD_EXTRASDK_BITCODE}" && return 0
  return 1   # false
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

  # Keep a backup of the files extrasdk generated
  # in case we want to re-install them again.
  local include_install="${NEWLIB_INSTALL_DIR}/${CROSS_TARGET_ARM}/include/nacl"
  local lib_install="${PNACL_BITCODE_ROOT}"
  local include_save="${TC_BUILD_EXTRASDK_BITCODE}/include-nacl"
  local lib_save="${TC_BUILD_EXTRASDK_BITCODE}/lib"
  local lib_save_native="${TC_BUILD_EXTRASDK_BITCODE}/lib_native"

  rm -rf "${include_save}"
  rm -rf "${lib_save}"
  rm -rf "${lib_save_native}"

  # First take care of the native arm libs/objs which we also generate
  mkdir -p ${lib_save_native}
  mv "${PNACL_BITCODE_ROOT}"/crt*.o\
     "${PNACL_BITCODE_ROOT}"/intrinsics.o\
     "${PNACL_BITCODE_ROOT}"/libcrt_platform.a\
     "${lib_save_native}"

  cp -a "${lib_install}" "${lib_save}"
  cp -a "${include_install}" "${include_save}"

  # Except libc/libg/libm, since they do not belong to extrasdk...
  rm -f "${lib_save}"/lib[cgm].a

  ts-touch-commit "${objdir}"

}

extrasdk-bitcode-install() {
  StepBanner "EXTRASDK-BITCODE" "Install from ${TC_BUILD_EXTRASDK_BITCODE}"

  # Copy from the save directories
  local include_install="${NEWLIB_INSTALL_DIR}/${CROSS_TARGET_ARM}/include/nacl"
  local lib_install="${PNACL_BITCODE_ROOT}"
  local lib_install_native="${PNACL_ARM_ROOT}"
  local include_save="${TC_BUILD_EXTRASDK_BITCODE}/include-nacl"
  local lib_save="${TC_BUILD_EXTRASDK_BITCODE}/lib"
  local lib_save_native="${TC_BUILD_EXTRASDK_BITCODE}/lib_native"

  mkdir -p "${include_install}"
  mkdir -p "${lib_install}"
  mkdir -p "${lib_install_native}"

  cp -fa "${include_save}"/*    "${include_install}"
  cp -fa "${lib_save}"/*        "${lib_install}"
  cp -fa "${lib_save_native}"/* "${lib_install_native}"

  # Do install_libpthread just in case it was wiped out.
  TARGET_CODE=bc-arm RunWithLog "extra_sdk_bitcode.libpthread" \
      ./scons MODE=nacl_extra_sdk \
      platform=arm \
      sdl=none \
      naclsdk_validate=0 \
      disable_nosys_linker_warnings=1 \
      install_libpthread
}

#+ newlib-nacl-headers   - Add custom NaCl headers to newlib
newlib-nacl-headers() {
  StepBanner "Adding nacl headers to newlib"
  here=$(pwd)
  spushd ${TC_SRC_NEWLIB}/newlib-trunk
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
  for s in sfigcc sfig++ bcld illegal nop ; do
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

  readonly arm_src=${INSTALL_ROOT}
  readonly x86_src=${NACL_TOOLCHAIN}
  readonly arm_llvm_gcc=${TARGET_ROOT}

  # TODO(espindola): There is a transitive dependency from libgcc to
  # libc, libnosys and libnacl. We should try to factor this better.
  # NOTE: the (cyclic) dependency from libgcc to libgcc has likely
  #       been removed for arm but may still exist for x86-32/64

  StepBanner "PNaCl" "arm native code: ${PNACL_ARM_ROOT}"
  mkdir -p ${PNACL_ARM_ROOT}

  cp -f ${arm_llvm_gcc}/lib/gcc/arm-none-linux-gnueabi/${GCC_VER}/libgcc.a \
    ${PNACL_ARM_ROOT}

  DebugRun ls -l ${PNACL_ARM_ROOT}

  # TODO(espindola): These files have been built with the conventional
  # nacl-gcc. The ABI might not be exactly the same as the one used by
  # PNaCl. We should build these files with PNaCl.
  StepBanner "PNaCl" "x86-32 native code: ${PNACL_X8632_ROOT}"
  mkdir -p ${PNACL_X8632_ROOT}
  cp -f ${arm_llvm_gcc}/lib/gcc/${CROSS_TARGET_X86_32}/${GCC_VER}/libgcc.a \
    ${PNACL_X8632_ROOT}
  cp -f  ${x86_src}/nacl64/lib/32/crt*.o \
    ${x86_src}/nacl64/lib/32/libcrt*.a \
    ${x86_src}/nacl64/lib/32/intrinsics.o \
    ${PNACL_X8632_ROOT}
  DebugRun ls -l ${PNACL_X8632_ROOT}

  StepBanner "PNaCl" "x86-64 native code: ${PNACL_X8664_ROOT}"
  mkdir -p ${PNACL_X8664_ROOT}
  cp -f ${x86_src}/lib/gcc/nacl64/4.4.3/libgcc.a \
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


readonly LLVM_DIS=${INSTALL_DIR}/bin/llvm-dis
readonly LLVM_AR=${INSTALL_DIR}/bin/${CROSS_TARGET_ARM}-ar


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


scons-build-sel_ldr () {
  local  platform=$1
  ./scons platform=${platform} ${SCONS_ARGS_SEL_LDR[@]} sel_ldr
}

scons-clean-pnacl-build-dir () {
  local  platform=$1
  # NOTE: we expect to have differnt dirs for each platform soon
  rm -rf scons-out/nacl-arm
}

scons-pnacl-build () {
  local platform=$1
  shift
  # NOTE: we currently run all of pnacl through the arm target so we need
  #       to turn of qemu emulation when we are not really targeting arm
  #
  local emu_flags="force_emulator="
  if [[ ${platform} == "arm" ]] ; then
    emu_flags=""
  fi
  TARGET_CODE=bc-${platform} ./scons ${SCONS_ARGS[@]} \
          force_sel_ldr=scons-out/opt-linux-${platform}/staging/sel_ldr \
          ${emu_flags} "$@"
}

test-scons-common () {
  local platform=$1
  shift
  scons-build-sel_ldr ${platform}
  scons-clean-pnacl-build-dir ${platform}

  if [ $# -eq 0 ] || ([ $# -eq 1 ] && [ "$1" == "-k" ]); then
    # first build everything
    scons-pnacl-build ${platform}
    # then also run some of the tests built before
    scons-pnacl-build ${platform} smoke_tests "$@"
  else
    scons-pnacl-build ${platform} "$@"
  fi
}

#@ test-arm              - run arm tests via pnacl toolchain
#@ test-arm <test>       - run a single arm test via pnacl toolchain
test-arm() {
  test-scons-common arm "$@"
}

#@ test-x86-32           - run x86-32 tests via pnacl toolchain
#@ test-x86-32 <test>    - run a single x86-32 test via pnacl toolchain
test-x86-32() {
  test-scons-common x86-32 "$@"
}

#@ test-x86-64           - run all x86-64 tests via pnacl toolchain
#@ test-x86-64 <test>    - run a single x86-64 test via pnacl toolchain
test-x86-64() {
  # TODO(robertm): we cannot run the full suite because c++ derived bitcode
  #                causes the x86-64 backend to crash
  test-scons-common x86-64 smoke_tests "$@"
}


#@ test-all              - run arm, x86-32, and x86-64 tests. (all should pass)
#@ test-all <test>       - run a single test on all architectures.
test-all() {
  if [ $# -eq 1 ] && [ "$1" == "-k" ]; then
    echo "Using -k on test-all is not a good idea."
    exit -1
  fi

  test-arm "$@"
  test-x86-64 "$@"
  test-x86-32 "$@"
}


#@ test-spec <official-spec-dir> <setup> -  run spec tests
test-spec() {
  if [ $# < 2 ]; then
    echo "test-spec {spec2krefdir} {setupfunc} {*optional_list_of_benchmarks}"
    exit 1
  fi;
  official=$(readlink -f $1)
  spushd tests/spec2k
  ./run_all.sh CleanBenchmarks
  ./run_all.sh PopulateFromSpecHarness ${official}
  ./run_all.sh BuildAndRunBenchmarks  $2
  spopd
}

#@ CollectTimingInfo <directory> <timing_result_file> <tagtype...>
#@  CD's into the directory in a subshell and collects all the
#@  relevant timed run info
#@  tagtype just gets printed out.
CollectTimingInfo() {
  wd=$1
  result_file=$2
  setup=$3
  (cd ${wd};
   echo "##################################################" >>${result_file}
   date +"# Completed at %F %H:%M:%S %A ${result_file}" >> ${result_file}
   echo "# " ${wd}
   echo "#" $(uname -a) >> ${result_file}
   echo "# SETUP: ${setup}" >>${result_file}
   echo "##################################################" >>${result_file}
   for ff in $(find . -name "*.compile_time"); do
     cat ${ff} >> ${result_file}
   done
   for ff in $(find . -name "*.run_time"); do
     cat ${ff} >> ${result_file}
   done
   cat ${result_file}
  )
}

#@ timed-test-spec <result-file> <official-spec-dir> <setup> -  run spec tests
#@  result-file is NOT cleared out, so you can reuse the same file to gather
#@  run-time data from different runs.
#@  Individual results files ARE cleaned out during each run, so as to not
#@  pollute the final results file
#@  Note that the VERIFY variable effects the timing!
timed-test-spec() {
  if [ "$#" -lt "3" ]; then
    echo "timed-test-spec {result-file} {spec2krefdir} {setupfunc}"
    exit 1
  fi;
  result_file=$1
  shift
  official=$(readlink -f $1)
  spushd tests/spec2k
  ./run_all.sh CleanBenchmarks
  ./run_all.sh PopulateFromSpecHarness ${official}
  ./run_all.sh TimedBuildAndRunBenchmarks $2
  CollectTimingInfo $(pwd) ${result_file} $2
  spopd
}



#@ test-bot-base         - tests that must pass on the bots to validate a TC
test-bot-base() {
  test-all
}


#@ test-bot-extra <spec-official> - additional tests run on the bots
test-bot-extra() {
  official=$(readlink -f $1)
  spushd tests/spec2k
  ./run_all.sh CleanBenchmarks
  ./run_all.sh PoplateFromSpecHarness ${official}
  ./run_all.sh BuildAndRunBenchmarks SetupPnaclX8632Opt
  ./run_all.sh BuildAndRunBenchmarks SetupPnaclX8632Opt
  ./run_all.sh BuildAndRunBenchmarks SetupPnaclArmOpt

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

  if [ ! -f "${fn}" ]; then
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

######################################################################
######################################################################
#
#                               < MAIN >
#
######################################################################
######################################################################

PathSanityCheck
PackageCheck
setup-tools-arm

[ $# = 0 ] && set -- help  # Avoid reference to undefined $1.
if [ "$(type -t $1)" != "function" ]; then
  #Usage
  echo "ERROR: unknown function '$1'." >&2
  echo "For help, try:"
  echo "    $0 help"
  exit 1
fi

eval "$@"
