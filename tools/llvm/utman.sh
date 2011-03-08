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
#
######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

# Make sure this script is run from the right place
if [[ $(basename $(pwd)) != "native_client" ]] ; then
  echo "ERROR: run this script from the native_client/ dir"
  exit -1
fi

source tools/llvm/common-tools.sh

# NOTE: gcc and llvm have to be synchronized
#       we have chosen toolchains which both are based on gcc-4.2.1

# For different levels of make parallelism change this in your env
readonly UTMAN_CONCURRENCY=${UTMAN_CONCURRENCY:-8}

# TODO(pdox): Decide what the target should really permanently be
readonly CROSS_TARGET_ARM=arm-none-linux-gnueabi
readonly CROSS_TARGET_X86_32=i686-none-linux-gnu
readonly CROSS_TARGET_X86_64=x86_64-none-linux-gnu
readonly BINUTILS_TARGET=arm-pc-nacl
readonly REAL_CROSS_TARGET=pnacl

readonly INSTALL_ROOT="$(pwd)/toolchain/linux_arm-untrusted"
readonly INSTALL_BIN="${INSTALL_ROOT}/bin"
readonly ARM_ARCH=armv7-a
readonly ARM_FPU=vfp
readonly INSTALL_DIR="${INSTALL_ROOT}/${CROSS_TARGET_ARM}"
readonly LDSCRIPTS_DIR="${INSTALL_ROOT}/ldscripts"
readonly GCC_VER="4.2.1"

# NOTE: NEWLIB_INSTALL_DIR also server as a SYSROOT
readonly NEWLIB_INSTALL_DIR="${INSTALL_ROOT}/arm-newlib"

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
readonly TC_BUILD_BINUTILS_ARM="${TC_BUILD}/binutils-arm"
readonly TC_BUILD_BINUTILS_LIBERTY_X86="${TC_BUILD}/binutils-liberty-x86"
readonly TC_BUILD_NEWLIB_ARM="${TC_BUILD}/newlib-arm"
readonly TC_BUILD_NEWLIB_BITCODE="${TC_BUILD}/newlib-bitcode"

# This apparently has to be at this location or gcc install breaks.
readonly TC_BUILD_LIBSTDCPP="${TC_BUILD_LLVM_GCC1}/${CROSS_TARGET_ARM}/libstdc++-v3"

readonly TC_BUILD_LIBSTDCPP_BITCODE="${TC_BUILD_LLVM_GCC1}/libstdcpp-bitcode"

# These are fake directories, for storing the timestamp only
readonly TC_BUILD_EXTRASDK_BITCODE="${TC_BUILD}/extrasdk"

readonly TIMESTAMP_FILENAME="make-timestamp"

# PNaCl toolchain locations (absolute!)
readonly PNACL_TOOLCHAIN_ROOT="${INSTALL_ROOT}"
readonly PNACL_ARM_ROOT="${PNACL_TOOLCHAIN_ROOT}/libs-arm"
readonly PNACL_X8632_ROOT="${PNACL_TOOLCHAIN_ROOT}/libs-x8632"
readonly PNACL_X8664_ROOT="${PNACL_TOOLCHAIN_ROOT}/libs-x8664"
readonly PNACL_BITCODE_ROOT="${PNACL_TOOLCHAIN_ROOT}/libs-bitcode"

# PNaCl client-translators (sandboxed) binary locations
readonly PNACL_SB_ROOT="${INSTALL_ROOT}/tools-sb"
readonly PNACL_SB_X8632="${PNACL_SB_ROOT}/x8632"
readonly PNACL_SB_X8664="${PNACL_SB_ROOT}/x8664"

# Location of PNaCl gcc/g++/as
readonly PNACL_GCC="${INSTALL_BIN}/pnacl-gcc"
readonly PNACL_GPP="${INSTALL_BIN}/pnacl-g++"
readonly PNACL_AS_ARM="${INSTALL_BIN}/pnacl-arm-as"
readonly PNACL_AS_X8632="${INSTALL_BIN}/pnacl-i686-as"
readonly PNACL_AS_X8664="${INSTALL_BIN}/pnacl-x86_64-as"

# Current milestones in each repo
# hg-update-stable  uses these
readonly LLVM_REV=8b90cf891e03
readonly LLVM_GCC_REV=80ce86616af9
readonly NEWLIB_REV=d0ac50acf303
readonly BINUTILS_REV=ff48f58cb707

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
# tools/llvm/utman.sh untrusted_sdk <file>
# NOTE: this has not been tried in a while and may no longer work
CC=${CC:-}
# TODO(espindola): This should be ${CXX:-}, but llvm-gcc's configure has a
# bug that brakes the build if we do that.
CXX=${CXX:-g++}

readonly CROSS_TARGET_AR=${INSTALL_DIR}/bin/${BINUTILS_TARGET}-ar
readonly CROSS_TARGET_AS=${INSTALL_DIR}/bin/${BINUTILS_TARGET}-as
readonly CROSS_TARGET_LD=${INSTALL_DIR}/bin/${BINUTILS_TARGET}-ld
readonly CROSS_TARGET_NM=${INSTALL_DIR}/bin/${BINUTILS_TARGET}-nm
readonly CROSS_TARGET_RANLIB=${INSTALL_DIR}/bin/${BINUTILS_TARGET}-ranlib
readonly ILLEGAL_TOOL=${INSTALL_DIR}/pnacl-illegal

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

  # NOTE: this seems to be no longer  used
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
    AR="${AR_FOR_SFI_TARGET}"
    AR_FOR_TARGET="${AR_FOR_SFI_TARGET}"
    NM_FOR_TARGET="${NM_FOR_SFI_TARGET}"
    RANLIB="${RANLIB_FOR_SFI_TARGET}"
    RANLIB_FOR_TARGET="${RANLIB_FOR_SFI_TARGET}"
    AS_FOR_TARGET="${ILLEGAL_TOOL}"
    LD_FOR_TARGET="${ILLEGAL_TOOL}"
    OBJDUMP_FOR_TARGET="${ILLEGAL_TOOL}" )

  # NOTE: we do not expect the assembler or linker to be used to build newlib.a
  #       hence the use of ILLEGAL_TOOL.
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
  CC_FOR_SFI_TARGET="${PNACL_GCC} -arch arm"
  CXX_FOR_SFI_TARGET="${PNACL_GPP} -arch arm"
  # NOTE: this should not be needed, since we do not really fully link anything
  LD_FOR_SFI_TARGET="${ILLEGAL_TOOL}"
  setup-tools-common
}

setup-tools-bitcode() {
  CC_FOR_SFI_TARGET="${PNACL_GCC}"
  CXX_FOR_SFI_TARGET="${PNACL_GPP}"
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

#@ hg-info-all         - Show status of repositories
hg-info-all() {
  hg-pull-all

  hg-info "${TC_SRC_LLVM}"       ${LLVM_REV}
  hg-info "${TC_SRC_LLVM_GCC}"   ${LLVM_GCC_REV}
  hg-info "${TC_SRC_NEWLIB}"     ${NEWLIB_REV}
  hg-info "${TC_SRC_BINUTILS}"   ${BINUTILS_REV}
}

#@ hg-freshness-check    - Make sure all repos are at the stable revision
hg-freshness-check() {
  StepBanner "HG-FRESHNESS-CHECK" "Checking for updates..."

  # Leave this global and mutable
  FRESHNESS_WARNING=false

  hg-freshness-check-common "${TC_SRC_LLVM}"       ${LLVM_REV}
  hg-freshness-check-common "${TC_SRC_LLVM_GCC}"   ${LLVM_GCC_REV}
  hg-freshness-check-common "${TC_SRC_NEWLIB}"     ${NEWLIB_REV}
  hg-freshness-check-common "${TC_SRC_BINUTILS}"   ${BINUTILS_REV}

  if ${FRESHNESS_WARNING}; then
    if ! confirm-yes "Continue build" ; then
      echo "Cancelled build."
      exit -1
    fi
  fi
}

hg-freshness-check-common() {
  local dir="$1"
  local rev="$2"
  local name=$(basename "${dir}")

  if ! hg-at-revision "${dir}" "${rev}" ; then
    echo "*******************************************************************"
    echo "* Warning: hg/${name} is not at the 'stable' revision.             "
    echo "*******************************************************************"

    # TODO(pdox): Make this a BUILDBOT flag
    if ${UTMAN_DEBUG}; then
      "hg-update-stable-${name}"
    else
      FRESHNESS_WARNING=true
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
  hg-update "${TC_SRC_LLVM_GCC}"
}

hg-update-tip-llvm() {
  hg-pull-llvm
  hg-update "${TC_SRC_LLVM}"
}

hg-update-tip-newlib() {
  if hg-update-newlib-confirm; then
    rm -rf "${TC_SRC_NEWLIB}"
    hg-checkout ${REPO_NEWLIB}   ${TC_SRC_NEWLIB}   ${NEWLIB_REV}
    hg-update "${TC_SRC_NEWLIB}"
    newlib-nacl-headers
  else
    echo "Cancelled."
  fi
}

hg-update-tip-binutils() {
  hg-pull-binutils
  hg-update "${TC_SRC_BINUTILS}"
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
  hg-update "${TC_SRC_LLVM_GCC}"  ${LLVM_GCC_REV}
}

hg-update-stable-llvm() {
  hg-pull-llvm
  hg-update "${TC_SRC_LLVM}"      ${LLVM_REV}
}

hg-update-stable-newlib() {
  if ${UTMAN_DEBUG} || hg-update-newlib-confirm; then
    rm -rf "${TC_SRC_NEWLIB}"
    hg-checkout ${REPO_NEWLIB}   ${TC_SRC_NEWLIB}   ${NEWLIB_REV}
    newlib-nacl-headers
  else
    echo "Cancelled."
  fi
}

hg-update-newlib-confirm() {
  echo
  echo "Due to special header magic, the newlib repository cannot simply be"
  echo "updated in place. Instead, the entire source directory must be"
  echo "deleted and cloned again."
  echo ""
  echo "WARNING: Local changes to newlib will be lost."
  echo ""
  confirm-yes "Continue"
  return $?
}

hg-update-stable-binutils() {
  hg-pull-binutils
  hg-update "${TC_SRC_BINUTILS}"  ${BINUTILS_REV}
}

#@ hg-pull-all           - Pull all repos. (but do not update working copy)
#@ hg-pull-REPO          - Pull repository REPO.
#@                         (REPO can be llvm-gcc, llvm, newlib, binutils)
hg-pull-all() {
  StepBanner "HG-PULL" "Running 'hg pull' in all repos..."
  hg-pull-llvm-gcc
  hg-pull-llvm
  hg-pull-newlib
  hg-pull-binutils
}

hg-pull-llvm-gcc() {
  hg-pull "${TC_SRC_LLVM_GCC}"
}

hg-pull-llvm() {
  hg-pull "${TC_SRC_LLVM}"
}

hg-pull-newlib() {
  hg-pull "${TC_SRC_NEWLIB}"
}

hg-pull-binutils() {
  hg-pull "${TC_SRC_BINUTILS}"
}

#@ hg-checkout-all       - check out mercurial repos needed to build toolchain
#@                          (skips repos which are already checked out)
hg-checkout-all() {
  StepBanner "HG-CHECKOUT-ALL"
  hg-checkout-llvm-gcc
  hg-checkout-llvm
  hg-checkout-binutils
  hg-checkout-newlib
}

hg-checkout-llvm-gcc() {
  hg-checkout ${REPO_LLVM_GCC} ${TC_SRC_LLVM_GCC} ${LLVM_GCC_REV}
}

hg-checkout-llvm() {
  hg-checkout ${REPO_LLVM}     ${TC_SRC_LLVM}     ${LLVM_REV}
}

hg-checkout-binutils() {
  hg-checkout ${REPO_BINUTILS} ${TC_SRC_BINUTILS} ${BINUTILS_REV}
}

hg-checkout-newlib() {
  local add_headers=false
  if [ ! -d "${TC_SRC_NEWLIB}" ]; then
    add_headers=true
  fi
  hg-checkout ${REPO_NEWLIB}   ${TC_SRC_NEWLIB}   ${NEWLIB_REV}
  if ${add_headers}; then
    newlib-nacl-headers
  fi
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
  extrasdk-clean
  clean-pnacl
  newlib-bitcode
  libstdcpp-bitcode
  # NOTE: currently also builds some native code
  extrasdk-make-install
  organize-native-code
}


#@ everything            - Build and install untrusted SDK.
everything() {

  mkdir -p "${INSTALL_ROOT}"

  # This is needed to build misc-tools and run ARM tests.
  # We check this early so that there are no surprises later, and we can
  # handle all user interaction early.
  check-for-trusted

  hg-checkout-all
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
  gcc-stage1 ${CROSS_TARGET_X86_64}

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

#@ status                - Show status of build directories
status() {
  # TODO(robertm): this is currently broken
  StepBanner "BUILD STATUS"

  status-helper "BINUTILS-ARM"      binutils-arm
  status-helper "LLVM"              llvm
  status-helper "GCC-STAGE1"        gcc-stage1

  status-helper "NEWLIB-ARM"        newlib-arm

  status-helper "NEWLIB-BITCODE"    newlib-bitcode
  status-helper "EXTRASDK-BITCODE"  extrasdk
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

  clean
  everything
  prune
  install-translators srpc
  prune-translator-install srpc
  tarball $1
}

#+ prune                 - Prune toolchain
prune() {
  StepBanner "PRUNE" "Pruning toolchain"
  local ACCEPTABLE_SIZE=250
  local dir_size_before=$(get_dir_size_in_mb ${INSTALL_ROOT})

  SubBanner "Size before: ${INSTALL_ROOT} ${dir_size_before}MB"
  echo "removing some static libs we do not have any use for"
  rm  -f "${INSTALL_DIR}"/lib/lib*.a

  echo "removing x86-32 frontend"
  rm -rf "${INSTALL_DIR}"/libexec/gcc/i686-none-linux-gnu/
  rm -rf "${INSTALL_DIR}"/bin/i686-none-linux-gnu-*

  echo "removing x86-64 frontend"
  rm -rf "${INSTALL_DIR}"/libexec/gcc/x86_64-none-linux-gnu/
  rm -rf "${INSTALL_DIR}"/bin/x86_64-none-linux-gnu-*

  echo "stripping binaries"
  strip "${INSTALL_DIR}"/libexec/gcc/arm-none-linux-gnueabi/4.2.1/c*
  if ! strip "${INSTALL_DIR}"/bin/* ; then
    echo "NOTE: some failures during stripping are expected"
  fi

  echo "removing llvm headers"
  rm -rf "${INSTALL_DIR}"/include/llvm*

  local dir_size_after=$(get_dir_size_in_mb ${INSTALL_ROOT})
  SubBanner "Size after: ${INSTALL_ROOT} ${dir_size_after}MB"

  if [[ ${dir_size_after} -gt ${ACCEPTABLE_SIZE} ]] ; then
    echo "ERROR: size of toolchain exceeds ${ACCEPTABLE_SIZE}MB"
    exit -1
  fi

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

# Default case - Optimized configure
LLVM_EXTRA_OPTIONS="--enable-optimized"

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
             --with-binutils-include=${binutils_include} \
             --enable-targets=x86,x86_64,arm \
             --target=${CROSS_TARGET_ARM} \
             --prefix=${INSTALL_DIR} \
             --with-llvmgccdir=${INSTALL_DIR} \
             ${LLVM_EXTRA_OPTIONS}


  spopd
}

#+ llvm-configure-dbg        - Run LLVM configure
#  Not used by default. Call manually.
llvm-configure-dbg() {
  StepBanner "LLVM" "Configure With Debugging"
  LLVM_EXTRA_OPTIONS="--disable-optimized \
        --enable-debug-runtime \
        --enable-assertions "
  llvm-configure
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
           NACL_SANDBOX=0 \
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

  ln -sf ../../lib/libLLVMgold.so "${BFD_PLUGIN_DIR}"
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
  local target=$1
  StepBanner "GCC-${target}"

  if gcc-stage1-needs-configure ${target}; then
    gcc-stage1-clean ${target}
    gcc-stage1-configure ${target}
  else
    SkipBanner "GCC-${target}" "configure"
  fi

  # We must always make before we do make install, because
  # the build must occur in a patched environment.
  # http://code.google.com/p/nativeclient/issues/detail?id=1128
  gcc-stage1-make ${target}

  gcc-stage1-install ${target}
}

#+ gcc-stage1-sysroot    - setup initial sysroot
gcc-stage1-sysroot() {
  StepBanner "GCC" "Setting up initial sysroot"

  local sys_include="${INSTALL_DIR}/${CROSS_TARGET_ARM}/include"
  local sys_include2="${INSTALL_DIR}/${CROSS_TARGET_ARM}/sys-include"

  rm -rf "${sys_include}" "${sys_include2}"
  mkdir -p "${sys_include}"
  ln -sf "${sys_include}" "${sys_include2}"
  cp -r "${TC_SRC_NEWLIB}"/newlib-trunk/newlib/libc/include/* ${sys_include}
}

#+ gcc-stage1-clean      - Clean gcc stage 1
gcc-stage1-clean() {
  local target=$1

  StepBanner "GCC-${target}" "Clean"
  local objdir="${TC_BUILD_LLVM_GCC1}-${target}"
  rm -rf "${objdir}"
}

gcc-stage1-needs-configure() {
  local target=$1
  speculative-check "llvm" && return 0
  ts-newer-than "${TC_BUILD_LLVM}" \
                "${TC_BUILD_LLVM_GCC1}-${target}" && return 0
  [ ! -f "${TC_BUILD_LLVM_GCC1}-${target}/config.status" ]
  return $?
}

#+ gcc-stage1-configure  - Configure GCC stage 1
gcc-stage1-configure() {
  local target=$1
  StepBanner "GCC-${target}" "Configure ${target}"

  local srcdir="${TC_SRC_LLVM_GCC}"
  local objdir="${TC_BUILD_LLVM_GCC1}-${target}"

  mkdir -p "${objdir}"
  spushd "${objdir}"

  # NOTE: hack, assuming presence of x86/32 toolchain (used for both 32/64)
  local config_opts=""
  case ${target} in
      arm-*)
          config_opts="--with-as=${PNACL_AS_ARM} \
                       --with-arch=${ARM_ARCH} \
                       --with-fpu=${ARM_FPU}"
          ;;
      i686-*)
          config_opts="--with-as=${PNACL_AS_X8632}"
          ;;
      x86_64-*)
          config_opts="--with-as=${PNACL_AS_X8664}"
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
               --disable-multilib \
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

gcc-stage1-make() {
  local target=$1
  local srcdir="${TC_SRC_LLVM_GCC}"
  local objdir="${TC_BUILD_LLVM_GCC1}-${target}"
  spushd ${objdir}

  StepBanner "GCC-${target}" "Make (Stage 1)"

  ts-touch-open "${objdir}"

  # In case it is still patched from an interrupted make
  xgcc-unpatch-nofail "${target}"

  # we built our version of binutil for multiple targets, so they work here
  local ar=${CROSS_TARGET_AR}
  local ranlib=${CROSS_TARGET_RANLIB}
  mkdir -p "${objdir}/dummy-bin"
  ln -sf ${ar} "${objdir}/dummy-bin/${target}-ar"
  ln -sf ${ranlib} "${objdir}/dummy-bin/${target}-ranlib"

  # NOTE: we add ${INSTALL_DIR}/bin to PATH
  RunWithLog llvm-pregcc-${target}.make \
       env -i PATH=/usr/bin/:/bin:${INSTALL_DIR}/bin:${objdir}/dummy-bin \
              CC=${CC} \
              CXX=${CXX} \
              CFLAGS="-Dinhibit_libc" \
              make ${MAKE_OPTS} all-gcc

  xgcc-patch "${target}"

  RunWithLog libgcc.clean \
      env -i PATH=/usr/bin/:/bin:${INSTALL_DIR}/bin \
             make clean-target-libgcc

  StepBanner "GCC-${target}" "Make (Stage 2)"
  # NOTE: This builds more than what we need right now. For example,
  # libstdc++ is unused (we always use the bitcode one). This might change
  # when we start supporting shared libraries.
  RunWithLog llvm-pregcc2-${target}.make \
       env -i PATH=/usr/bin/:/bin:${INSTALL_DIR}/bin:${objdir}/dummy-bin \
              CC=${CC} \
              CXX=${CXX} \
              CFLAGS="-Dinhibit_libc" \
              make ${MAKE_OPTS} all

  xgcc-unpatch "${target}"

  ts-touch-commit "${objdir}"

  spopd
}

#+ xgcc-patch          - Patch xgcc and clean libgcc
xgcc-patch() {
  # This is a hack. Ideally gcc would be configured with a pnacl-nacl target
  # and xgcc would produce sfi code automatically. This is not the case, so
  # we cheat gcc's build by replacing xgcc behind its back.
  local target="$1"
  local objdir="${TC_BUILD_LLVM_GCC1}-${target}"
  local XGCC="${objdir}/gcc/xgcc"
  local XGCC_REAL="${XGCC}-real"
  local arch=""
  local driver=""

  StepBanner "GCC-${target}" "Patching xgcc and cleaning libgcc $target"

  # Make sure XGCC exists
  if [ ! -f "${XGCC}" ] ; then
    Abort "xgcc-patch" "Real xgcc does not exist."
    return
  fi

  # Make sure it is a real ELF file
  if ! is-ELF "${XGCC}" ; then
    Abort "xgcc-patch" "xgcc already patched"
    return
  fi

  # N.B:
  # * We have to use the arch cc1 istead of the more common practice of using
  # the ARM cc1 for everything. The reason is that the ARM cc1 will reject a
  # asm that uses ebx for example
  # * Because of the previous item, we have to use the arch driver bacuse
  # the ARM driver will pass options like -mfpu that the x86 cc1 cannot handle.
  case ${target} in
      arm-*)    arch="arm" ;;
      i686-*)   arch="x86-32" ;;
      x86_64-*) arch="x86-64" ;;
  esac

  mv "${XGCC}" "${XGCC_REAL}"
  cat > "${XGCC}" <<EOF
#!/bin/sh
XGCC="\$(readlink -m \${0})"
${PNACL_GCC} \\
--driver="\${XGCC}-real" -arch ${arch} ${CPPFLAGS_FOR_SFI_TARGET} "\$@"
EOF

  chmod 755 "${XGCC}"

}

xgcc-unpatch() {
  local target="$1"
  local objdir="${TC_BUILD_LLVM_GCC1}-${target}"
  local XGCC="${objdir}/gcc/xgcc"
  local XGCC_REAL="${XGCC}-real"

  StepBanner "GCC-${target}" "Unpatching xgcc"

  # Make sure XGCC exists
  if [ ! -f "${XGCC}" ] ; then
    Abort "xgcc-unpatch" "Real xgcc does not exist."
    return
  fi

  # Make sure it isn't a real ELF file
  if is-ELF "${XGCC}" ; then
    Abort "xgcc-unpatch" "xgcc already unpatched?"
    return
  fi

  rm "${XGCC}"
  mv "${XGCC_REAL}" "${XGCC}"
}

xgcc-unpatch-nofail() {
  local target="$1"
  local objdir="${TC_BUILD_LLVM_GCC1}-${target}"
  local XGCC="${objdir}/gcc/xgcc"
  local XGCC_REAL="${XGCC}-real"

  # If the old patch file exists, delete it.
  if [ -f "${XGCC}" ] && ! is-ELF "${XGCC}"; then
    rm "${XGCC}"
  fi

  # If there's a real one around, move it back.
  if [ -f "${XGCC_REAL}" ]; then
    mv "${XGCC_REAL}" "${XGCC}"
  fi
}

#+ gcc-stage1-install    - Install GCC stage 1
gcc-stage1-install() {
  local target=$1
  StepBanner "GCC-${target}" "Install ${target}"

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

  # install headers (=install-data)
  # for good measure make sure we do not keep any old headers
  rm -rf "${INSTALL_ROOT}/include/c++"
  RunWithLog llvm-gcc.install_libstdcpp \
    make \
    "${STD_ENV_FOR_LIBSTDCPP[@]}" \
    ${MAKE_OPTS} install-data
  # install bitcode library
  cp "${objdir}/src/.libs/libstdc++.a" "${dest}"

  spopd
}

#+ misc-tools            - Build and install sel_ldr and validator for ARM.
misc-tools() {
  if has-trusted-toolchain; then
    StepBanner "MISC-ARM" "Building sel_ldr (ARM)"

    # TODO(robertm): revisit some of these options
    RunWithLog arm_sel_ldr \
      ./scons MODE=opt-linux \
      platform=arm \
      sdl=none \
      naclsdk_validate=0 \
      sysinfo=0 \
      sel_ldr
    rm -rf  "${INSTALL_ROOT}/tools-arm"
    mkdir "${INSTALL_ROOT}/tools-arm"
    cp scons-out/opt-linux-arm/obj/src/trusted/service_runtime/sel_ldr \
      ${INSTALL_ROOT}/tools-arm
  else
    StepBanner "MISC-ARM" "Skipping ARM sel_ldr (No trusted ARM toolchain)"
  fi

  StepBanner "MISC-ARM" "Building validator (ARM)"
  RunWithLog arm_ncval_core \
    ./scons MODE=opt-linux \
    targetplatform=arm \
    sysinfo=0 \
    arm-ncval-core
  rm -rf  "${INSTALL_ROOT}/tools-x86"
  mkdir "${INSTALL_ROOT}/tools-x86"
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

  # enable multiple targets so that we can use the same ar with all .o files
  local targ="arm-pc-nacl,i686-pc-nacl,x86_64-pc-nacl"
  mkdir -p "${objdir}"
  spushd "${objdir}"

  # --enable-checking is to avoid a build failure:
  #   tc-arm.c:2489: warning: empty body in an if-statement
  # The --enable-gold and --enable-plugins options are on so that we
  # can use gold's support for plugin to link PNaCl modules.

  # TODO(pdox): Building binutils for nacl/nacl64 target currently requires
  # providing NACL_ALIGN_* defines. This should really be defined inside
  # binutils instead.
  RunWithLog binutils.arm.configure \
    env -i \
    PATH="/usr/bin:/bin" \
    CC=${CC} \
    CXX=${CXX} \
    CFLAGS="-DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5" \
    ${srcdir}/binutils-2.20/configure --prefix=${INSTALL_DIR} \
                                      --target=${BINUTILS_TARGET} \
                                      --enable-targets=${targ} \
                                      --enable-checking \
                                      --enable-gold=both \
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
      ${srcdir}/binutils-2.20/configure --prefix=${PNACL_SB_ROOT}
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

check-sb-mode() {
  local mode=$1
  if [ ${mode} != "srpc" ] && [ ${mode} != "nonsrpc" ]; then
    echo "ERROR: Unsupported mode. Choose one of: srpc, nonsrpc"
    exit -1
  fi
}

check-sb-arch() {
  # TODO(pdox): allow ARM when we self-build.
  local arch=$1
  if [ ${arch} != "x8632" ] && [ ${arch} != "x8664" ]; then
    echo "ERROR: Unsupported arch. Choose one of: x8632, x8664"
    exit -1
  fi
}

#+-------------------------------------------------------------------------
#+ llvm-tools-sb <arch> <mode> - Build and install llvm tools (sandboxed)
llvm-tools-sb() {
  local srcdir="${TC_SRC_LLVM}"

  assert-dir "${srcdir}" "You need to checkout llvm."

  if [ $# -ne 2 ]; then
    echo "ERROR: Usage llvm-tools-sb <arch> <mode>"
    exit -1
  fi

  local arch=$1
  local mode=$2
  check-sb-arch ${arch}
  check-sb-mode ${mode}

  if [ ! -d ${NACL_TOOLCHAIN} ] ; then
    echo "ERROR: Install Native Client toolchain"
    exit -1
  fi

  if llvm-tools-sb-needs-configure "${arch}" "${mode}"; then
    llvm-tools-sb-clean "${arch}" "${mode}"
    llvm-tools-sb-configure "${arch}" "${mode}"
  else
    SkipBanner "LLVM-TOOLS-SB" "configure ${arch} ${mode}"
  fi

  if llvm-tools-sb-needs-make "${arch}" "${mode}"; then
    llvm-tools-sb-make "${arch}" "${mode}"
  else
    SkipBanner "LLVM-TOOLS-SB" "make ${arch} ${mode}"
  fi

  llvm-tools-sb-install "${arch}" "${mode}"
}

llvm-tools-sb-needs-configure() {
  local arch=$1
  local mode=$2
  [ ! -f "${TC_BUILD}/llvm-tools-${arch}-${mode}-sandboxed/config.status" ]
  return $?
}

# llvm-tools-sb-clean - Clean llvm tools (sandboxed) for x86
llvm-tools-sb-clean() {
  local arch=$1
  local mode=$2
  StepBanner "LLVM-TOOLS-SB" "Clean ${arch} ${mode}"
  local objdir="${TC_BUILD}/llvm-tools-${arch}-${mode}-sandboxed"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"
}

# llvm-tools-sb-configure - Configure llvm tools (sandboxed) for x86
llvm-tools-sb-configure() {
  local arch=$1
  local mode=$2
  StepBanner "LLVM-TOOLS-SB" "Configure ${arch} ${mode}"
  local srcdir="${TC_SRC_LLVM}"
  local objdir="${TC_BUILD}/llvm-tools-${arch}-${mode}-sandboxed"
  local installdir="${PNACL_SB_ROOT}/${arch}/${mode}"
  local bitsize=""
  local nacl=""
  local target""

  if [ ${arch} == "x8632" ]; then
    bitsize=32
    nacl=nacl
    target=x86
  elif [ ${arch} == "x8664" ]; then
    bitsize=64
    nacl=nacl64
    target=x86_64
  fi

  local flags="-m${bitsize} -static -I${NACL_TOOLCHAIN}/${nacl}/include \
      -L${NACL_TOOLCHAIN}/${nacl}/lib"
  if [ ${mode} == "srpc" ]; then
    flags+=" -DNACL_SRPC"
  fi

  spushd ${objdir}
  RunWithLog \
      llvm.tools.${arch}.${mode}.sandboxed.configure \
      env -i \
      PATH="/usr/bin:/bin" \
      AR="${NACL_TOOLCHAIN}/bin/${nacl}-ar" \
      AS="${NACL_TOOLCHAIN}/bin/${nacl}-as" \
      CC="${NACL_TOOLCHAIN}/bin/${nacl}-gcc ${flags}" \
      CXX="${NACL_TOOLCHAIN}/bin/${nacl}-g++ ${flags}" \
      LD="${NACL_TOOLCHAIN}/bin/${nacl}-ld" \
      RANLIB="${NACL_TOOLCHAIN}/bin/${nacl}-ranlib" \
      LDFLAGS="-s" \
      ${srcdir}/llvm-trunk/configure \
        --prefix=${installdir} \
        --host=nacl \
        --disable-jit \
        --enable-optimized \
        --target=${CROSS_TARGET_ARM} \
        --enable-targets=${target} \
        --enable-pic=no \
        --enable-static \
        --enable-shared=no
  spopd
}

llvm-tools-sb-needs-make() {
  local arch=$1
  local mode=$2
  local srcdir="${TC_SRC_LLVM}"
  local objdir="${TC_BUILD}/llvm-tools-${arch}-${mode}-sandboxed"

  ts-modified "$srcdir" "$objdir"
  return $?
}

# llvm-tools-sb-make - Make llvm tools (sandboxed) for x86
llvm-tools-sb-make() {
  local arch=$1
  local mode=$2
  StepBanner "LLVM-TOOLS-SB" "Make ${arch} ${mode}"
  local objdir="${TC_BUILD}/llvm-tools-${arch}-${mode}-sandboxed"
  spushd ${objdir}

  ts-touch-open "${objdir}"

  local build_with_srpc=0
  if [ ${mode} == "srpc" ]; then
    build_with_srpc=1
  fi

  RunWithLog llvm.tools.${arch}.${mode}.sandboxed.make \
      env -i PATH="/usr/bin:/bin" \
      ONLY_TOOLS=llc \
      NACL_SANDBOX=1 \
      NACL_SRPC=${build_with_srpc} \
      KEEP_SYMBOLS=1 \
      VERBOSE=1 \
      make ENABLE_OPTIMIZED=1 OPTIMIZE_OPTION=-O3 tools-only ${MAKE_OPTS}

  ts-touch-commit "${objdir}"

  spopd
}

# llvm-tools-sb-install - Install llvm tools (sandboxed) for x86
llvm-tools-sb-install() {
  local arch=$1
  local srpc=$2
  StepBanner "LLVM-TOOLS-SB" "Install ${arch} ${srpc}"
  local objdir="${TC_BUILD}/llvm-tools-${arch}-${srpc}-sandboxed"
  spushd ${objdir}

  RunWithLog llvm.tools.${arch}.${srpc}.sandboxed.install \
      env -i PATH="/usr/bin:/bin" \
      ONLY_TOOLS=llc \
      NACL_SANDBOX=1 \
      KEEP_SYMBOLS=1 \
      make install ${MAKE_OPTS}

  spopd
}

#+-------------------------------------------------------------------------
#+ binutils-sb <arch> <mode> - Build and install binutils (sandboxed)
binutils-sb() {
  local srcdir="${TC_SRC_BINUTILS}"

  assert-dir "${srcdir}" "You need to checkout binutils."

  if [ $# -ne 2 ]; then
    echo "ERROR: Usage binutils-sb <arch> <mode>"
    exit -1
  fi

  local arch=$1
  local mode=$2
  check-sb-arch ${arch}
  check-sb-mode ${mode}

  if [ ! -d ${NACL_TOOLCHAIN} ] ; then
    echo "ERROR: Install Native Client toolchain"
    exit -1
  fi

  if [ ! -f ${PNACL_SB_ROOT}/lib/libiberty.a ] ; then
    echo "ERROR: Missing lib. Run this script with binutils-liberty-x86 option"
    exit -1
  fi

  if binutils-sb-needs-configure "${arch}" "${mode}"; then
    binutils-sb-clean "${arch}" "${mode}"
    binutils-sb-configure "${arch}" "${mode}"
  else
    SkipBanner "BINUTILS-SB" "configure ${arch} ${mode}"
  fi

  if binutils-sb-needs-make "${arch}" "${mode}"; then
    binutils-sb-make "${arch}" "${mode}"
  else
    SkipBanner "BINUTILS-SB" "make ${arch} ${mode}"
  fi

  binutils-sb-install "${arch}" "${mode}"
}

binutils-sb-needs-configure() {
  local arch=$1
  local mode=$2
  [ ! -f "${TC_BUILD}/binutils-${arch}-${mode}-sandboxed/config.status" ]
  return $?
}

# binutils-sb-clean - Clean binutils (sandboxed) for x86
binutils-sb-clean() {
  local arch=$1
  local mode=$2
  StepBanner "BINUTILS-SB" "Clean ${arch} ${mode}"
  local objdir="${TC_BUILD}/binutils-${arch}-${mode}-sandboxed"
  rm -rf "${objdir}"
  mkdir -p "${objdir}"
}

# binutils-sb-configure - Configure binutils (sandboxed) for x86
binutils-sb-configure() {
  local arch=$1
  local mode=$2
  StepBanner "BINUTILS-SB" "Configure ${arch} ${mode}"
  local bitsize=${arch:3:2}
  local nacl="nacl${bitsize/"32"/}"
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD}/binutils-${arch}-${mode}-sandboxed"
  local installdir="${PNACL_SB_ROOT}/${arch}/${mode}"

  local flags="-DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5 -DNACL_TOOLCHAIN_PATCH"
  if [ ${mode} == "srpc" ]; then
    flags+=" -DNACL_SRPC"
  fi

  mkdir ${objdir}/opcodes
  spushd ${objdir}
  cp ${PNACL_SB_ROOT}/lib/libiberty.a ./opcodes/.
  RunWithLog \
      binutils.${arch}.${mode}.sandboxed.configure \
      env -i \
      PATH="/usr/bin:/bin" \
      AR="${NACL_TOOLCHAIN}/bin/${nacl}-ar" \
      AS="${NACL_TOOLCHAIN}/bin/${nacl}-as" \
      CC="${NACL_TOOLCHAIN}/bin/${nacl}-gcc" \
      CXX="${NACL_TOOLCHAIN}/bin/${nacl}-g++" \
      LD="${NACL_TOOLCHAIN}/bin/${nacl}-ld" \
      RANLIB="${NACL_TOOLCHAIN}/bin/${nacl}-ranlib" \
      CFLAGS="-m${bitsize} -O3 ${flags} -I${NACL_TOOLCHAIN}/${nacl}/include" \
      LDFLAGS="-s" \
      LDFLAGS_FOR_BUILD="-L ." \
      ${srcdir}/binutils-2.20/configure \
        --prefix=${installdir} \
        --host=${nacl} \
        --target=${nacl} \
        --disable-nls \
        --disable-werror \
        --enable-static \
        --enable-shared=no
  spopd
}

binutils-sb-needs-make() {
  local arch=$1
  local mode=$2
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD}/binutils-${arch}-${mode}-sandboxed"

  ts-modified "$srcdir" "$objdir"
  return $?
}

# binutils-sb-make - Make binutils (sandboxed) for x86
binutils-sb-make() {
  local arch=$1
  local mode=$2
  StepBanner "BINUTILS-SB" "Make ${arch} ${mode}"
  local objdir="${TC_BUILD}/binutils-${arch}-${mode}-sandboxed"

  spushd ${objdir}

  ts-touch-open "${objdir}"

  local build_with_srpc=0
  if [ ${mode} == "srpc" ]; then
    build_with_srpc=1
  fi

  RunWithLog binutils.${arch}.sandboxed.make \
      env -i PATH="/usr/bin:/bin" \
      NACL_SRPC=${build_with_srpc} \
      make ${MAKE_OPTS} all-gas all-ld

  ts-touch-commit "${objdir}"

  spopd
}

# binutils-sb-install - Install binutils (sandboxed) for x86
binutils-sb-install() {
  local arch=$1
  local mode=$2
  StepBanner "BINUTILS-SB" "Install ${arch} ${mode}"
  local objdir="${TC_BUILD}/binutils-${arch}-${mode}-sandboxed"

  spushd ${objdir}

  RunWithLog binutils.${arch}.${mode}.sandboxed.install \
      env -i PATH="/usr/bin:/bin" \
      make install-gas install-ld

  spopd
}

#+--------------------------------------------------------------------------
#@ install-translators {srpc/nonsrpc} - Builds and installs sandboxed
#@                                      translator components
install-translators() {
  if [ $# -ne 1 ]; then
    echo "ERROR: Usage install-translators <srpc/nonsrpc>"
    exit -1
  fi

  local srpc_kind=$1
  check-sb-mode ${srpc_kind}

  StepBanner "INSTALL SANDBOXED TRANSLATORS (${srpc_kind})"

  # StepBanner "ARM" "Sandboxing"
  # StepBanner "---" "----------"
  # TODO(abetul): Missing arm translator work.

  StepBanner "X86" "Prepare"
  StepBanner "---" "-------"
  binutils-liberty-x86
  echo ""

  StepBanner "32-bit X86" "Sandboxing"
  StepBanner "----------" "----------"
  binutils-sb x8632 ${srpc_kind}
  llvm-tools-sb x8632 ${srpc_kind}
  echo ""

  mkdir -p ${PNACL_SB_X8632}/script
  assert-file "$(pwd)/tools/llvm/ld_script_x8632_untrusted" \
        "Install NaCl toolchain."
  cp "$(pwd)/tools/llvm/ld_script_x8632_untrusted" \
        "${PNACL_SB_X8632}/script/ld_script"

  assert-file "$(pwd)/tools/llvm/dummy_translator_x8632.sh" \
        "Install NaCl toolchain."
  cp tools/llvm/dummy_translator_x8632.sh "${PNACL_SB_X8632}/translator"

  StepBanner "64-bit X86" "Sandboxing"
  StepBanner "----------" "----------"
  binutils-sb x8664 ${srpc_kind}
  llvm-tools-sb x8664 ${srpc_kind}
  echo ""

  mkdir -p ${PNACL_SB_X8664}/script
  assert-file "$(pwd)/tools/llvm/ld_script_x8664_untrusted" \
        "Install NaCl toolchain."
  cp "$(pwd)/tools/llvm/ld_script_x8664_untrusted" \
        "${PNACL_SB_X8664}/script/ld_script"

  assert-file "$(pwd)/tools/llvm/dummy_translator_x8664.sh" \
        "Install NaCl toolchain."
  cp tools/llvm/dummy_translator_x8664.sh "${PNACL_SB_X8664}/translator"

  echo "Done"
}

#+-------------------------------------------------------------------------
#@ prune-translator-install - Prunes translator install directories
prune-translator-install() {
  if [ $# -ne 1 ]; then
    echo "ERROR: Usage prune-translator-install <srpc/nonsrpc>"
    exit -1
  fi

  local srpc_kind=$1
  check-sb-mode ${srpc_kind}

  StepBanner "PRUNE" "Pruning translator installs (${srpc_kind})"

  spushd "${PNACL_SB_X8632}/${srpc_kind}"
  rm -rf include lib nacl share
  rm -rf bin/llvm-config bin/tblgen
  spopd

  spushd "${PNACL_SB_X8664}/${srpc_kind}"
  rm -rf include lib nacl64 share
  rm -rf bin/llvm-config bin/tblgen
  spopd

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
#     < EXTRASDK >
#########################################################################
#+ extrasdk-clean  - Clean extra-sdk stuff

extrasdk-clean() {
  StepBanner "EXTRASDK-BITCODE" "Clean"
  rm -rf "${TC_BUILD_EXTRASDK_BITCODE}"

  StepBanner "EXTRASDK-BITCODE" "Clean bitcode lib"
  # TODO(robertm): consider having a dedicated dir for this so can
  #                delete this wholesale
  # Do not clean libc and libstdc++ but everything else
  rm -f "${PNACL_BITCODE_ROOT}"/*google*.a
  rm -f "${PNACL_BITCODE_ROOT}"/*nacl*
  rm -f "${PNACL_BITCODE_ROOT}"/libpthread.a
  rm -f "${PNACL_BITCODE_ROOT}"/libsrpc.a
  rm -f "${PNACL_BITCODE_ROOT}"/libnpapi.a
  rm -f "${PNACL_BITCODE_ROOT}"/libppapi.a
  rm -f "${PNACL_BITCODE_ROOT}"/libnosys.a
  rm -f "${PNACL_BITCODE_ROOT}"/libav.a
  rm -f "${PNACL_BITCODE_ROOT}"/libgio.a

  StepBanner "EXTRASDK-BITCODE" "Clean arm libs"
  # Do not clean libgcc but everything else
  rm -f "${PNACL_ARM_ROOT}"/*crt*

  StepBanner "EXTRASDK-BITCODE" "Clean x86-32 libs"
  # Do not clean libgcc but everything else
  rm -f "${PNACL_X8632_ROOT}"/*crt*

  StepBanner "EXTRASDK-BITCODE" "Clean x86-64 libs"
  # Do not clean libgcc but everything else
  rm -f "${PNACL_X8664_ROOT}"/*crt*

  # clean scons obj dirs
  rm -rf scons-out/nacl_extra_sdk-*-pnacl
}

#+ extrasdk-make-install  - build and install all extra sdk components
extrasdk-make-install() {
  StepBanner "EXTRASDK"
  local headerdir="${NEWLIB_INSTALL_DIR}/${CROSS_TARGET_ARM}/include"

  StepBanner "EXTRASDK" "Make/Install headers"
  RunWithLog "extra_sdk.headers" \
      ./scons MODE=nacl_extra_sdk \
      extra_sdk_lib_destination="${PNACL_BITCODE_ROOT}" \
      extra_sdk_include_destination="${headerdir}" \
      bitcode=1 \
      platform=arm \
      sdl=none \
      naclsdk_validate=0 \
      extra_sdk_update_header

  StepBanner "EXTRASDK" "Make/Install bitcode libpthread"
  RunWithLog "extra_sdk.bitcode_libpthread" \
      ./scons MODE=nacl_extra_sdk -j 8\
      extra_sdk_lib_destination="${PNACL_BITCODE_ROOT}" \
      extra_sdk_include_destination="${headerdir}" \
      bitcode=1 \
      platform=arm \
      sdl=none \
      naclsdk_validate=0 \
      install_libpthread

  StepBanner "EXTRASDK" "Make/Install bitcode components"
  RunWithLog "extra_sdk.bitcode_components" \
      ./scons MODE=nacl_extra_sdk -j 8\
      extra_sdk_lib_destination="${PNACL_BITCODE_ROOT}" \
      extra_sdk_include_destination="${headerdir}" \
      disable_nosys_linker_warnings=1 \
      bitcode=1 \
      platform=arm \
      sdl=none \
      naclsdk_validate=0 \
      --verbose \
      extra_sdk_libs

  StepBanner "EXTRASDK" "Make/Install arm components"
  RunWithLog "extra_sdk.arm_components" \
      ./scons MODE=nacl_extra_sdk \
      extra_sdk_lib_destination="${PNACL_ARM_ROOT}" \
      extra_sdk_include_destination="${headerdir}" \
      bitcode=1 \
      platform=arm \
      sdl=none \
      naclsdk_validate=0 \
      --verbose \
      extra_sdk_libs_platform

  StepBanner "EXTRASDK" "Make/Install x86-32 components"
  RunWithLog "extra_sdk.libs_x8632" \
      ./scons MODE=nacl_extra_sdk \
      extra_sdk_lib_destination="${PNACL_X8632_ROOT}" \
      extra_sdk_include_destination="${headerdir}" \
      bitcode=1 \
      platform=x86-32 \
      sdl=none \
      naclsdk_validate=0 \
      --verbose \
      extra_sdk_libs_platform

  StepBanner "EXTRASDK" "Make/Install x86-64 components"
  RunWithLog "extra_sdk.libs_x8664" \
      ./scons MODE=nacl_extra_sdk \
      extra_sdk_lib_destination="${PNACL_X8664_ROOT}" \
      extra_sdk_include_destination="${headerdir}" \
      bitcode=1 \
      platform=x86-64 \
      sdl=none \
      naclsdk_validate=0 \
      --verbose \
      extra_sdk_libs_platform
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
  # need to prep the dir just in case..
  prep-install-dir
  # otherwise linker-install will stomp it.
  linker-install
  driver-install
  driver-intrinsics
}

driver-intrinsics() {
  StepBanner "DRIVER" "Install LLVM intrinsics"
  "${INSTALL_DIR}"/bin/llvm-as \
    tools/llvm/llvm-intrinsics.ll \
    -o "${INSTALL_ROOT}/llvm-intrinsics.bc"
  "${INSTALL_DIR}"/bin/llvm-as \
    tools/llvm/llvm-preserve.ll \
    -o "${INSTALL_ROOT}/llvm-preserve.bc"
}

# Just in case we're calling this manually
prep-install-dir() {
  mkdir -p ${INSTALL_DIR}
}

# We need to adjust the start address and aligment of nacl arm modules
linker-install() {
   StepBanner "DRIVER" "Installing untrusted ld scripts"
   mkdir -p "${LDSCRIPTS_DIR}"
   cp tools/llvm/ld_script_arm_untrusted "${LDSCRIPTS_DIR}"
   cp tools/llvm/ld_script_x8632_untrusted "${LDSCRIPTS_DIR}"
   cp tools/llvm/ld_script_x8664_untrusted "${LDSCRIPTS_DIR}"
}

# the driver is a simple python script which changes its behavior
# depending under the name it is invoked as
# TODO(jasonwkim) [2010/08/17 09:20:24 PDT (Tuesday)]
# This routine can be deleted once the rest of the build scripts are updated
driver-install() {
  StepBanner "DRIVER" "Installing driver adaptors to ${INSTALL_DIR}"
  # TODO(robertm): move the driver to their own dir
  # rm -rf  ${INSTALL_DIR}
  mkdir -p "${INSTALL_BIN}"
  rm -f "${INSTALL_BIN}/pnacl-*"
  cp tools/llvm/driver.py "${INSTALL_BIN}"
  for s in gcc g++ as arm-as i686-as x86_64-as \
           bclink bcopt dis ld translate illegal nop \
           ar nm ranlib ; do
    local t="pnacl-$s"
    ln -fs driver.py "${INSTALL_BIN}/$t"
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
  readonly arm_llvm_gcc=${INSTALL_DIR}

  StepBanner "PNaCl" "arm native code: ${PNACL_ARM_ROOT}"
  mkdir -p ${PNACL_ARM_ROOT}
  local startup_dir=${arm_llvm_gcc}/lib/gcc/arm-none-linux-gnueabi/${GCC_VER}
  cp -f ${startup_dir}/libgcc.a ${PNACL_ARM_ROOT}
  cp -f ${startup_dir}/libgcc_eh.a ${PNACL_ARM_ROOT}
  DebugRun ls -l ${PNACL_ARM_ROOT}

  StepBanner "PNaCl" "x86-32 native code: ${PNACL_X8632_ROOT}"
  mkdir -p ${PNACL_X8632_ROOT}
  local startup_dir=${arm_llvm_gcc}/lib/gcc/${CROSS_TARGET_X86_32}/${GCC_VER}
  cp -f ${startup_dir}/libgcc.a ${PNACL_X8632_ROOT}
  cp -f ${startup_dir}/libgcc_eh.a ${PNACL_X8632_ROOT}
  DebugRun ls -l ${PNACL_X8632_ROOT}

  StepBanner "PNaCl" "x86-64 native code: ${PNACL_X8664_ROOT}"
  mkdir -p ${PNACL_X8664_ROOT}
  local startup_dir=${arm_llvm_gcc}/lib/gcc/${CROSS_TARGET_X86_64}/${GCC_VER}
  cp -f ${startup_dir}/libgcc.a ${PNACL_X8664_ROOT}
  cp -f ${startup_dir}/libgcc_eh.a ${PNACL_X8664_ROOT}
  DebugRun ls -l ${PNACL_X8664_ROOT}
}

######################################################################
######################################################################
#     < VERIFY >
######################################################################
######################################################################

readonly LLVM_DIS=${INSTALL_DIR}/bin/llvm-dis
readonly LLVM_OPT=${INSTALL_DIR}/bin/opt
readonly LLVM_AR=${CROSS_TARGET_AR}

# Note: we could replace this with a modified version of tools/elf_checker.py
#       if we do not want to depend on {NACL_TOOLCHAIN}
readonly NACL_OBJDUMP=${NACL_TOOLCHAIN}/bin/nacl-objdump

# Usage: VerifyArchive <checker> <filename> <pattern>
VerifyArchive() {
  local checker="$1"
  local pattern="$2"
  local archive="$3"
  local tmp="/tmp/ar-verify-${RANDOM}"
  rm -rf ${tmp}
  mkdir -p ${tmp}
  cp "${archive}" "${tmp}"
  spushd ${tmp}
  echo -n "verify $(basename "${archive}"): "
  ${LLVM_AR} x $(basename ${archive})
  # extract all the files
  local count=0
  for i in ${pattern} ; do
    if [ ! -e "$i" ]; then
      # we may also see the unexpanded pattern here if there is no match
      continue
    fi
    count=$((count+1))
    ${checker} $i
  done
  if [ "${count}" = "0" ] ; then
    echo "FAIL - archive empty or wrong contents: ${archive}"
    ls -l "${tmp}"
    exit -1
  fi
  echo "PASS  (${count} files)"
  rm -rf "${tmp}"
  spopd
}

#
# verify-object-llvm <obj>
#
#   Verifies that a given .o file is bitcode and free of ASMSs
verify-object-llvm() {
  t=$(${LLVM_DIS} $1 -o -)

  if grep asm <<<$t ; then
    echo
    echo "ERROR asm in $1"
    echo
    exit -1
  fi
}


# verify-object-arm <obj>
#
#   Ensure that the ARCH properties are what we expect, this is a little
#   fragile and needs to be updated when tools change
verify-object-arm() {
  # TODO(robertm): we should check ABI version
  # http://code.google.com/p/nativeclient/issues/detail?id=1482
  arch_info=$(readelf -A $1)
  #TODO(robertm): some refactoring and cleanup needed
  if ! grep -q "Tag_VFP_arch: VFPv2" <<< ${arch_info} ; then
    echo "ERROR $1 - bad Tag_VFP_arch\n"
    #TODO(robertm): figure out what the right thing to do is here, c.f.
    # http://code.google.com/p/nativeclient/issues/detail?id=966
    readelf -A $1 | grep  Tag_VFP_arch
    exit -1
  fi

  if ! grep -q "Tag_CPU_arch: v7" <<< ${arch_info} ; then
    echo "FAIL bad $1 Tag_CPU_arch\n"
    readelf -A $1 | grep Tag_CPU_arch
    exit -1
  fi
}


# verify-object-x86-32 <obj>
#
verify-object-x86-32() {
  arch_info=$(${NACL_OBJDUMP} -f $1)
  if ! grep -q "elf32-nacl" <<< ${arch_info} ; then
    echo "ERROR $1 - bad file format\n"
    echo ${arch_info}
    exit -1
  fi
}

# verify-object-x86-64 <obj>
#
verify-object-x86-64() {
  arch_info=$(${NACL_OBJDUMP} -f $1)
  if ! grep -q "elf64-nacl" <<< ${arch_info} ; then
    echo "ERROR $1 - bad file format\n"
    echo ${arch_info}
    exit -1
  fi
}


#
# verify-archive-llvm <archive>
# Verifies that a given archive is bitcode and free of ASMSs
#
verify-archive-llvm() {
  # Currently all the files are .o in the llvm archives.
  # Eventually more and more should be .bc.
  VerifyArchive verify-object-llvm '*.bc *.o' "$@"
}

#
# verify-archive-arm <archive>
# Verifies that a given archive is a proper arm achive
#
verify-archive-arm() {
  VerifyArchive verify-object-arm '*.o' "$@"
}

#
# verify-archive-x86-32 <archive>
# Verifies that a given archive is a proper x86-32 achive
#
verify-archive-x86-32() {
  VerifyArchive verify-object-x86-32 '*.o' "$@"
}

#
# verify-archive-x86-64 <archive>
# Verifies that a given archive is a proper x86-64 achive
#
verify-archive-x86-64() {
  VerifyArchive verify-object-x86-64 '*.o' "$@"
}
#@-------------------------------------------------------------------------
#+ verify                - Verifies that toolchain/pnacl-untrusted ELF files
#+                         are of the correct architecture.
verify() {
  StepBanner "VERIFY"

  SubBanner "VERIFY: ${PNACL_ARM_ROOT}"
  for i in ${PNACL_ARM_ROOT}/*.o ; do
    verify-object-arm "$i"
  done

  for i in ${PNACL_ARM_ROOT}/*.a ; do
    verify-archive-arm "$i"
  done

  SubBanner "VERIFY: ${PNACL_X8632_ROOT}"
  for i in ${PNACL_X8632_ROOT}/*.o ; do
     verify-object-x86-32  "$i"
  done

  for i in ${PNACL_X8632_ROOT}/*.a ; do
    verify-archive-x86-32 "$i"
  done

  SubBanner "VERIFY: ${PNACL_X8664_ROOT}"
  for i in ${PNACL_X8664_ROOT}/*.o ; do
     verify-object-x86-64  "$i"
  done

  for i in ${PNACL_X8664_ROOT}/*.a ; do
    verify-archive-x86-64 "$i"
  done

  SubBanner "VERIFY: ${PNACL_BITCODE_ROOT}"
  for i in ${PNACL_BITCODE_ROOT}/*.a ; do
    verify-archive-llvm "$i"
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
  readonly SCONS_ARGS=(MODE=nacl,opt-linux
                       --verbose
                       sdl=none
                       bitcode=1)

  readonly SCONS_ARGS_SEL_LDR=(MODE=opt-linux
                               bitcode=1
                               sdl=none
                               --verbose)
else
  readonly SCONS_ARGS=(MODE=nacl,opt-linux
                       sdl=none
                       naclsdk_validate=0
                       sysinfo=0
                       bitcode=1
                       -j${UTMAN_CONCURRENCY})

  readonly SCONS_ARGS_SEL_LDR=(MODE=opt-linux
                               bitcode=1
                               sdl=none
                               naclsdk_validate=0
                               sysinfo=0
                               -j${UTMAN_CONCURRENCY})
fi

#@ show-tests            - see what tests can be run
show-tests() {
  StepBanner "SHOWING TESTS"
  cat $(find tests -name nacl.scons) | grep -o 'run_[A-Za-z_-]*' | sort | uniq
}

#+ scons-determine-tests  - returns:
#+    (a) "true smoke_tests [-k]" if all smoke tests should be built and run.
#+ or (b) "false $@" if not all tests should be built because specific tests
#+        are already identified in $@. The test must be the first element
#+        of $@, but we don't check that here.
scons-determine-tests() {
  if [ $# -eq 0 ] || ([ $# -eq 1 ] && [ "$1" == "-k" ]); then
    echo "true smoke_tests $@" # $@ should only tack on the -k flag or nothing
  else
    echo "false $@"
  fi
}

scons-build-sel_ldr() {
  local  platform=$1
  ./scons platform=${platform} ${SCONS_ARGS_SEL_LDR[@]} sel_ldr
}

scons-build-sel_universal() {
  local  platform=$1
  ./scons platform=${platform} ${SCONS_ARGS_SEL_LDR[@]} sel_universal
}

scons-clean-pnacl-build-dir () {
  rm -rf scons-out/nacl-$1-pnacl
}

scons-clean-pnacl-pic-build-dir () {
  rm -rf scons-out/nacl-$1-pnacl-pic
}

scons-pnacl-build () {
  local platform=$1
  shift
  ./scons ${SCONS_ARGS[@]} \
          platform=${platform} \
          "$@"
}

run-scons-tests() {
  local platform=$1
  local should_build_all=$2
  local testname=$3
  shift 3
  # The rest of the arguments should be flags!

  # See if we should build all the tests.
  if ${should_build_all}; then
    scons-pnacl-build ${platform} $@
  fi

  # Then run the listed tests.
  scons-pnacl-build ${platform} ${testname} $@
}

test-scons-common () {
  local platform=$1
  shift
  scons-clean-pnacl-build-dir ${platform}

  test_setup=$(scons-determine-tests "$@")
  run-scons-tests ${platform} ${test_setup}
}

test-scons-pic-common () {
  local platform=$1
  shift
  scons-clean-pnacl-pic-build-dir ${platform}

  test_setup=$(scons-determine-tests "$@")
  run-scons-tests ${platform} ${test_setup} nacl_pic=1
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
  test-scons-common x86-64 "$@"
}

#@ test-arm-pic           - run all arm pic tests via pnacl toolchain
#@ test-arm-pic <test>    - run a single arm pic test via pnacl toolchain
test-arm-pic() {
  test-scons-pic-common arm "$@"
}

#@ test-x86-32-pic        - run all x86-32 pic tests via pnacl toolchain
#@ test-x86-32-pic <test> - run a single x86-32 pic test via pnacl toolchain
test-x86-32-pic() {
  test-scons-pic-common x86-32 "$@"
}

#@ test-x86-64-pic        - run all x86-64 pic tests via pnacl toolchain
#@ test-x86-64-pic <test> - run a single x86-64 pic test via pnacl toolchain
test-x86-64-pic() {
  test-scons-pic-common x86-64 "$@"
}

#@ test-all              - run arm, x86-32, and x86-64 tests. (all should pass)
#@ test-all <test>       - run a single test on all architectures.
test-all() {
  if [ $# -eq 1 ] && [ "$1" == "-k" ]; then
    echo "Using -k on test-all is not a good idea."
    exit -1
  fi

  test-arm "$@"
  test-arm-pic "$@"
  test-x86-64 "$@"
  test-x86-64-pic "$@"
  test-x86-32 "$@"
  test-x86-32-pic "$@"
}


#@ test-spec <official-spec-dir> <setup> [ref|train] [<benchmarks>]*
#@                       - run spec tests
#@
test-spec() {
  if [[ $# -lt 2 ]]; then
    echo "not enough arguments for test-spec"
    exit 1
  fi;
  official=$(readlink -f $1)
  setup=$2
  shift 2
  spushd tests/spec2k
  ./run_all.sh CleanBenchmarks "$@"
  ./run_all.sh PopulateFromSpecHarness ${official} "$@"
  ./run_all.sh BuildAndRunBenchmarks ${setup} "$@"
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
   mkdir -p $(dirname ${result_file})
   echo "##################################################" >>${result_file}
   date +"# Completed at %F %H:%M:%S %A ${result_file}" >> ${result_file}
   echo "# " ${wd}
   echo "#" $(uname -a) >> ${result_file}
   echo "# SETUP: ${setup}" >>${result_file}
   echo "##################################################" >>${result_file}
   echo "# COMPILE " >> ${result_file}
   for ff in $(find . -name "*.compile_time"); do
     cat ${ff} >> ${result_file}
   done
   echo "# RUN " >> ${result_file}
   for ff in $(find . -name "*.run_time"); do
     cat ${ff} >> ${result_file}
   done
   cat ${result_file}
  )
}


#@ timed-test-spec <result-file> <official-spec-dir> <setup> ... - run spec and
#@  measure time / size data. Data is emitted to stdout, but also collected
#@  in <result-file>. <result-file> is not cleared across runs (but temp files
#@  are cleared on each run).
#@  Note that the VERIFY variable effects the timing!
timed-test-spec() {
  if [ "$#" -lt "3" ]; then
    echo "timed-test-spec {result-file} {spec2krefdir} {setupfunc}" \
         "[ref|train] [benchmark]*"
    exit 1
  fi;
  result_file=$1
  official=$(readlink -f $2)
  setup=$3
  shift 3
  spushd tests/spec2k
  ./run_all.sh CleanBenchmarks "$@"
  ./run_all.sh PopulateFromSpecHarness ${official} "$@"
  ./run_all.sh TimedBuildAndRunBenchmarks ${setup} "$@"
  CollectTimingInfo $(pwd) ${result_file} ${setup}
  spopd
}


#@ test-bot-base         - tests that must pass on the bots to validate a TC
test-bot-base() {
  test-all
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

has-trusted-toolchain() {
  if [ -f toolchain/linux_arm-trusted/ld_script_arm_trusted ]; then
    return 0
  else
    return 1
  fi
}

check-for-trusted() {
  if ! has-trusted-toolchain; then
    echo '*******************************************************************'
    echo '*   The ARM trusted toolchain does not appear to be installed yet *'
    echo '*   It is needed to run ARM tests.                                *'
    echo '*                                                                 *'
    echo '*   To download and install the trusted toolchain, run:           *'
    echo '*                                                                 *'
    echo '*       $ tools/llvm/utman.sh download-trusted                    *'
    echo '*                                                                 *'
    echo '*   To compile the trusted toolchain, use:                        *'
    echo '*                                                                 *'
    echo '*       $ tools/llvm/trusted-toolchain-creator.sh trusted_sdk     *'
    echo '*               (warning: this takes a while)                     *'
    echo '*******************************************************************'

    # If building on the bots, do not continue since it needs to run ARM tests.
    if ${UTMAN_DEBUG} ; then
      echo "Building on bots --> need ARM trusted toolchain to run tests!"
      exit -1
    elif trusted-tc-confirm ; then
      echo "Continuing without ARM trusted toolchain"
    else
      echo "Okay, stopping."
      exit -1
    fi
  fi
}

trusted-tc-confirm() {
  echo
  echo "Do you wish to continue without the ARM trusted TC (skip ARM testing)?"
  echo ""
  confirm-yes "Continue"
  return $?
}

DebugRun() {
  if ${UTMAN_DEBUG}; then
    "$@"
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

mkdir -p "${INSTALL_ROOT}"
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

"$@"
