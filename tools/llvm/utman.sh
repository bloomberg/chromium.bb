#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#@                 Untrusted Toolchain Manager
#@-------------------------------------------------------------------
#@ This script builds the ARM and PNaCl untrusted toolchains.
#@ It MUST be run from the native_client/ directory.
#
######################################################################
# Directory Layout Description
######################################################################
#
# All directories are relative to BASE which is
# On Linux X86-64: native_client/toolchain/pnacl_linux_x86_64/
# On Linux X86-32: native_client/toolchain/pnacl_linux_i686/
# On Mac X86-32  : native_client/toolchain/pnacl_darwin_i386/
#
######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

# The script is located in "native_client/tools/llvm".
# Set pwd to native_client/
cd "$(dirname "$0")"/../..
if [[ $(basename "$(pwd)") != "native_client" ]] ; then
  echo "ERROR: cannot find native_client/ directory"
  exit -1
fi
readonly NACL_ROOT="$(pwd)"

source tools/llvm/common-tools.sh

SetScriptPath "${NACL_ROOT}/tools/llvm/utman.sh"
SetLogDirectory "${NACL_ROOT}/toolchain/hg-log"

# NOTE: gcc and llvm have to be synchronized
#       we have chosen toolchains which both are based on gcc-4.2.1

# For different levels of make parallelism change this in your env
readonly UTMAN_CONCURRENCY=${UTMAN_CONCURRENCY:-8}
readonly UTMAN_MERGE_TESTING=${UTMAN_MERGE_TESTING:-false}
UTMAN_PRUNE=${UTMAN_PRUNE:-false}
UTMAN_BUILD_ARM=true

if ${BUILD_PLATFORM_MAC} || ${BUILD_PLATFORM_WIN}; then
  # We don't yet support building ARM tools for mac or windows.
  UTMAN_BUILD_ARM=false
fi

# Set the library mode
readonly LIBMODE=${LIBMODE:-newlib}

LIBMODE_NEWLIB=false
LIBMODE_GLIBC=false
if [ ${LIBMODE} == "newlib" ]; then
  LIBMODE_NEWLIB=true
elif [ ${LIBMODE} == "glibc" ]; then
  LIBMODE_GLIBC=true
  UTMAN_BUILD_ARM=false
else
  Fatal "Unknown library mode ${LIBMODE}"
fi
readonly LIBMODE_NEWLIB
readonly LIBMODE_GLIBC

readonly SB_JIT=${SB_JIT:-false}

# TODO(pdox): Decide what the target should really permanently be
readonly CROSS_TARGET_ARM=arm-none-linux-gnueabi
readonly BINUTILS_TARGET=arm-pc-nacl
readonly REAL_CROSS_TARGET=pnacl
readonly NACL64_TARGET=x86_64-nacl

readonly DRIVER_DIR="${NACL_ROOT}/tools/llvm/driver"
readonly ARM_ARCH=armv7-a
readonly ARM_FPU=vfp


readonly NNACL_BASE="${NACL_ROOT}/toolchain/${SCONS_BUILD_PLATFORM}_x86"
readonly NNACL_NEWLIB_ROOT="${NNACL_BASE}_newlib"
readonly NNACL_GLIBC_ROOT="${NNACL_BASE}"

readonly MAKE_OPTS="-j${UTMAN_CONCURRENCY} VERBOSE=1"

readonly NONEXISTENT_PATH="/going/down/the/longest/road/to/nowhere"

# For speculative build status output. ( see status function )
# Leave this blank, it will be filled during processing.
SPECULATIVE_REBUILD_SET=""

readonly PNACL_ROOT="${NACL_ROOT}/pnacl"
readonly PNACL_SUPPORT="${PNACL_ROOT}/support"

readonly GMP_VER=gmp-5.0.2
readonly THIRD_PARTY_GMP="${NACL_ROOT}/../third_party/gmp/${GMP_VER}.tar.bz2"

readonly MPFR_VER=mpfr-3.0.1
readonly THIRD_PARTY_MPFR="${NACL_ROOT}/../third_party/mpfr/${MPFR_VER}.tar.bz2"

readonly MPC_VER=mpc-0.9
readonly THIRD_PARTY_MPC="${NACL_ROOT}/../third_party/mpc/${MPC_VER}.tar.gz"

# The location of Mercurial sources (absolute)
readonly PNACL_HG_ROOT="${NACL_ROOT}/hg"
readonly TC_SRC_UPSTREAM="${PNACL_HG_ROOT}/upstream"
readonly TC_SRC_LLVM="${TC_SRC_UPSTREAM}/llvm"
readonly TC_SRC_LLVM_GCC="${TC_SRC_UPSTREAM}/llvm-gcc"
readonly TC_SRC_LIBSTDCPP="${TC_SRC_LLVM_GCC}/libstdc++-v3"
readonly TC_SRC_BINUTILS="${PNACL_HG_ROOT}/binutils"
readonly TC_SRC_NEWLIB="${PNACL_HG_ROOT}/newlib"
readonly TC_SRC_COMPILER_RT="${PNACL_HG_ROOT}/compiler-rt"
readonly TC_SRC_GOOGLE_PERFTOOLS="${PNACL_HG_ROOT}/google-perftools"

# LLVM sources (svn)
readonly PNACL_SVN_ROOT="${NACL_ROOT}/hg"
readonly TC_SRC_LLVM_MASTER="${PNACL_SVN_ROOT}/llvm-master"
readonly TC_SRC_LLVM_GCC_MASTER="${PNACL_SVN_ROOT}/llvm-gcc-master"
readonly TC_SRC_CLANG="${PNACL_SVN_ROOT}/clang"
readonly TC_SRC_DRAGONEGG="${PNACL_SVN_ROOT}/dragonegg"

# Git sources
readonly PNACL_GIT_ROOT="${PNACL_ROOT}/git"
readonly TC_SRC_GCC="${PNACL_GIT_ROOT}/gcc"
readonly TC_SRC_GMP="${PNACL_ROOT}/third_party/gmp"
readonly TC_SRC_MPFR="${PNACL_ROOT}/third_party/mpfr"
readonly TC_SRC_MPC="${PNACL_ROOT}/third_party/mpc"

# Unfortunately, binutils/configure generates this untracked file
# in the binutils source directory
readonly BINUTILS_MESS="${TC_SRC_BINUTILS}/binutils-2.20/opcodes/i386-tbl.h"

readonly SERVICE_RUNTIME_SRC="${NACL_ROOT}/src/trusted/service_runtime"
readonly EXPORT_HEADER_SCRIPT="${SERVICE_RUNTIME_SRC}/export_header.py"
readonly NACL_SYS_HEADERS="${SERVICE_RUNTIME_SRC}/include"
readonly NACL_HEADERS_TS="${PNACL_HG_ROOT}/nacl.sys.timestamp"
readonly NEWLIB_INCLUDE_DIR="${TC_SRC_NEWLIB}/newlib-trunk/newlib/libc/include"

# The location of each project. These should be absolute paths.
# If a tool below depends on a certain libc, then the build
# directory should have ${LIBMODE} in it to distinguish them.

readonly TC_BUILD="${NACL_ROOT}/toolchain/hg-build-${LIBMODE}"
readonly TC_BUILD_LLVM="${TC_BUILD}/llvm"
readonly TC_BUILD_LLVM_GCC="${TC_BUILD}/llvm-gcc"
readonly TC_BUILD_BINUTILS="${TC_BUILD}/binutils"
readonly TC_BUILD_BINUTILS_LIBERTY="${TC_BUILD}/binutils-liberty"
readonly TC_BUILD_NEWLIB="${TC_BUILD}/newlib"
readonly TC_BUILD_LIBSTDCPP="${TC_BUILD_LLVM_GCC}-arm/libstdcpp"
readonly TC_BUILD_COMPILER_RT="${TC_BUILD}/compiler_rt"
readonly TC_BUILD_GOOGLE_PERFTOOLS="${TC_BUILD}/google-perftools"
readonly TC_BUILD_GCC="${TC_BUILD}/gcc"
readonly TC_BUILD_GMP="${TC_BUILD}/gmp"
readonly TC_BUILD_MPFR="${TC_BUILD}/mpfr"
readonly TC_BUILD_MPC="${TC_BUILD}/mpc"
readonly TC_BUILD_DRAGONEGG="${TC_BUILD}/dragonegg"

readonly TIMESTAMP_FILENAME="make-timestamp"

# PNaCl toolchain installation directories (absolute paths)
readonly TOOLCHAIN_LABEL="pnacl_${BUILD_PLATFORM}_${HOST_ARCH}_${LIBMODE}"
readonly INSTALL_ROOT="${NACL_ROOT}/toolchain/${TOOLCHAIN_LABEL}"
readonly INSTALL_BIN="${INSTALL_ROOT}/bin"

# SDK
readonly INSTALL_SDK_ROOT="${INSTALL_ROOT}/sdk"
readonly INSTALL_SDK_INCLUDE="${INSTALL_SDK_ROOT}/include"
readonly INSTALL_SDK_LIB="${INSTALL_SDK_ROOT}/lib"

# The pattern `lib-${platform}' is implicit in verify() and sdk().
readonly INSTALL_LIB="${INSTALL_ROOT}/lib"
readonly INSTALL_LIB_ARM="${INSTALL_ROOT}/lib-arm"
readonly INSTALL_LIB_X8632="${INSTALL_ROOT}/lib-x86-32"
readonly INSTALL_LIB_X8664="${INSTALL_ROOT}/lib-x86-64"

# PNaCl client-translators (sandboxed) binary locations
readonly INSTALL_SB_TOOLS="${INSTALL_ROOT}/tools-sb"
readonly INSTALL_SB_TOOLS_X8632="${INSTALL_SB_TOOLS}/x8632"
readonly INSTALL_SB_TOOLS_X8664="${INSTALL_SB_TOOLS}/x8664"
readonly INSTALL_SB_TOOLS_UNIVERSAL="${INSTALL_SB_TOOLS}/universal"

# Component installation directories
readonly INSTALL_PKG="${INSTALL_ROOT}/pkg"
readonly NEWLIB_INSTALL_DIR="${INSTALL_PKG}/newlib"
readonly GLIBC_INSTALL_DIR="${INSTALL_PKG}/glibc"
readonly LLVM_INSTALL_DIR="${INSTALL_PKG}/llvm"
readonly LLVM_GCC_INSTALL_DIR="${INSTALL_PKG}/llvm-gcc"
readonly GCC_INSTALL_DIR="${INSTALL_PKG}/gcc"
readonly GMP_INSTALL_DIR="${INSTALL_PKG}/gmp"
readonly MPFR_INSTALL_DIR="${INSTALL_PKG}/mpfr"
readonly MPC_INSTALL_DIR="${INSTALL_PKG}/mpc"
readonly LIBSTDCPP_INSTALL_DIR="${INSTALL_PKG}/libstdcpp"
readonly BINUTILS_INSTALL_DIR="${INSTALL_PKG}/binutils"
readonly BFD_PLUGIN_DIR="${BINUTILS_INSTALL_DIR}/lib/bfd-plugins"
readonly SYSROOT_DIR="${INSTALL_ROOT}/sysroot"
readonly LDSCRIPTS_DIR="${INSTALL_ROOT}/ldscripts"
readonly LLVM_GCC_VER="4.2.1"

# Location of PNaCl gcc/g++/as
readonly PNACL_GCC="${INSTALL_BIN}/pnacl-gcc"
readonly PNACL_GXX="${INSTALL_BIN}/pnacl-g++"
readonly PNACL_DGCC="${INSTALL_BIN}/pnacl-dgcc"
readonly PNACL_DGXX="${INSTALL_BIN}/pnacl-dg++"
readonly PNACL_CLANG="${INSTALL_BIN}/pnacl-clang"
readonly PNACL_CLANGXX="${INSTALL_BIN}/pnacl-clang++"
readonly PNACL_AR="${INSTALL_BIN}/pnacl-ar"
readonly PNACL_RANLIB="${INSTALL_BIN}/pnacl-ranlib"
readonly PNACL_AS="${INSTALL_BIN}/pnacl-as"
readonly PNACL_LD="${INSTALL_BIN}/pnacl-ld"
readonly PNACL_NM="${INSTALL_BIN}/pnacl-nm"
readonly PNACL_TRANSLATE="${INSTALL_BIN}/pnacl-translate"
readonly PNACL_READELF="${INSTALL_BIN}/readelf"
readonly PNACL_SIZE="${INSTALL_BIN}/size"
readonly PNACL_STRIP="${INSTALL_BIN}/pnacl-strip"
readonly ILLEGAL_TOOL="${INSTALL_BIN}"/pnacl-illegal

readonly PNACL_AS_ARM="${INSTALL_BIN}/pnacl-arm-as"
readonly PNACL_AS_X8632="${INSTALL_BIN}/pnacl-i686-as"
readonly PNACL_AS_X8664="${INSTALL_BIN}/pnacl-x86_64-as"

# Set the default frontend.
# Can be default, clang, llvm-gcc, or dragonegg
# "Default" uses whatever is known to work, preferring Clang by default.
readonly DEFAULT_FRONTEND="clang"
readonly FRONTEND="${FRONTEND:-default}"

# For a production (release) build, we want the sandboxed
# translator to only contain the code needed to handle
# its own architecture. For example, the translator shipped with
# an X86-32 browser would only be able to translate to X86-32 code.
# This is so that the translator binary is as small as possible.
#
# If SBTC_PRODUCTION is true, then the translators are built
# separately, one for each architecture, so that each translator
# can only target its own architecture.
#
# If SBTC_PRODUCTION is false, then we instead use PNaCl to
# build a `fat` translator which can target all supported
# architectures. This translator is built as a .pexe
# which can then be translated to each individual architecture.
SBTC_PRODUCTION=${SBTC_PRODUCTION:-false}

# Which toolchain to use for each arch.
if ${LIBMODE_NEWLIB}; then
  SBTC_BUILD_WITH_PNACL="arm x8632 x8664"
else
  SBTC_BUILD_WITH_PNACL="x8632 x8664"
fi

# Current milestones in each repo
readonly UPSTREAM_REV=${UPSTREAM_REV:-6a66f368c292}

readonly NEWLIB_REV=c6358617f3fd
readonly BINUTILS_REV=17a01203bd48
readonly COMPILER_RT_REV=1a3a6ffb31ea
readonly GOOGLE_PERFTOOLS_REV=ad820959663d

readonly LLVM_PROJECT_REV=${LLVM_PROJECT_REV:-142624}
readonly LLVM_MASTER_REV=${LLVM_PROJECT_REV}
readonly LLVM_GCC_MASTER_REV=${LLVM_PROJECT_REV}
readonly CLANG_REV=${LLVM_PROJECT_REV}
readonly DRAGONEGG_REV=${LLVM_PROJECT_REV}

# Repositories
readonly REPO_UPSTREAM="nacl-llvm-branches.upstream"
readonly REPO_NEWLIB="nacl-llvm-branches.newlib"
readonly REPO_BINUTILS="nacl-llvm-branches.binutils"
readonly REPO_COMPILER_RT="nacl-llvm-branches.compiler-rt"
readonly REPO_GOOGLE_PERFTOOLS="nacl-llvm-branches.google-perftools"

# LLVM repos (svn)
readonly REPO_LLVM_MASTER="http://llvm.org/svn/llvm-project/llvm/trunk"
readonly REPO_LLVM_GCC_MASTER="http://llvm.org/svn/llvm-project/llvm-gcc-4.2/trunk"
readonly REPO_CLANG="http://llvm.org/svn/llvm-project/cfe/trunk"
readonly REPO_DRAGONEGG="http://llvm.org/svn/llvm-project/dragonegg/trunk"

# TODO(espindola): This should be ${CXX:-}, but llvm-gcc's configure has a
# bug that brakes the build if we do that.
CC=${CC:-gcc}
CXX=${CXX:-g++}
if ${HOST_ARCH_X8632} ; then
  # These are simple compiler wrappers to force 32bit builds
  # For bots and releases we build the toolchains
  # on the oldest system we care to support. Currently
  # that is a 32 bit hardy. The advantage of this is that we can build
  # the toolchain shared, reducing its size and allowing the use of
  # plugins. You can test them on your system by setting the
  # environment variable HOST_ARCH=x86_32 on a 64 bit system.
  # Make sure you clean all your build dirs
  # before switching arches.
  CC="${NACL_ROOT}/tools/llvm/mygcc32"
  CXX="${NACL_ROOT}/tools/llvm/myg++32"
fi

force-frontend() {
  local frontend="$1"
  if [ "${frontend}" == default ] ; then
    frontend="${DEFAULT_FRONTEND}"
  fi
  case "${frontend}" in
    clang)
      PNACL_CC="${PNACL_CLANG}"
      PNACL_CXX="${PNACL_CLANGXX}"
      ;;
    llvm-gcc)
      PNACL_CC="${PNACL_GCC}"
      PNACL_CXX="${PNACL_GXX}"
      ;;
    dragonegg)
      PNACL_CC="${PNACL_DGCC}"
      PNACL_CXX="${PNACL_DGXX}"
      ;;
    *)
      Fatal "Unknown frontend: ${frontend}"
      ;;
  esac
}

# Every prefer-frontend should be followed by a reset-frontend.
prefer-frontend() {
  local frontend="$1"
  if [ "${FRONTEND}" == default ] ; then
    force-frontend "${frontend}"
  fi
}

reset-frontend() {
  force-frontend "${FRONTEND}"
}

setup-libstdcpp-env() {
  # NOTE: we do not expect the assembler or linker to be used for libs
  #       hence the use of ILLEGAL_TOOL.
  # TODO: the arm bias should be eliminated
  # BUG: http://code.google.com/p/nativeclient/issues/detail?id=865
  STD_ENV_FOR_LIBSTDCPP=(
    CC_FOR_BUILD="${CC}"
    CC="${PNACL_CC}"
    CXX="${PNACL_CXX}"
    RAW_CXX_FOR_TARGET="${PNACL_CXX}"
    LD="${ILLEGAL_TOOL}"
    CFLAGS="--pnacl-arm-bias"
    CPPFLAGS="--pnacl-arm-bias"
    CXXFLAGS="--pnacl-arm-bias"
    CFLAGS_FOR_TARGET="--pnacl-arm-bias"
    CPPFLAGS_FOR_TARGET="--pnacl-arm-bias"
    CC_FOR_TARGET="${PNACL_CC}"
    GCC_FOR_TARGET="${PNACL_CC}"
    CXX_FOR_TARGET="${PNACL_CXX}"
    AR="${PNACL_AR}"
    AR_FOR_TARGET="${PNACL_AR}"
    NM_FOR_TARGET="${PNACL_NM}"
    RANLIB="${PNACL_RANLIB}"
    RANLIB_FOR_TARGET="${PNACL_RANLIB}"
    AS_FOR_TARGET="${ILLEGAL_TOOL}"
    LD_FOR_TARGET="${ILLEGAL_TOOL}"
    OBJDUMP_FOR_TARGET="${ILLEGAL_TOOL}" )
}

setup-newlib-env() {
  STD_ENV_FOR_NEWLIB=(
    CFLAGS_FOR_TARGET="--pnacl-arm-bias"
    CPPFLAGS_FOR_TARGET="--pnacl-arm-bias"
    CC_FOR_TARGET="${PNACL_CC}"
    GCC_FOR_TARGET="${PNACL_CC}"
    CXX_FOR_TARGET="${PNACL_CXX}"
    AR_FOR_TARGET="${PNACL_AR}"
    NM_FOR_TARGET="${PNACL_NM}"
    RANLIB_FOR_TARGET="${PNACL_RANLIB}"
    OBJDUMP_FOR_TARGET="${ILLEGAL_TOOL}"
    AS_FOR_TARGET="${ILLEGAL_TOOL}"
    LD_FOR_TARGET="${ILLEGAL_TOOL}"
    STRIP_FOR_TARGET="${ILLEGAL_TOOL}" )
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

  hg-info "${TC_SRC_UPSTREAM}"   ${UPSTREAM_REV}
  hg-info "${TC_SRC_NEWLIB}"     ${NEWLIB_REV}
  hg-info "${TC_SRC_BINUTILS}"   ${BINUTILS_REV}
  hg-info "${TC_SRC_COMPILER_RT}" ${COMPILER_RT_REV}
  hg-info "${TC_SRC_GOOGLE_PERFTOOLS}" ${GOOGLE_PERFTOOLS_REV}
}

update-all() {
  hg-update-upstream
  svn-update-clang
  svn-update-dragonegg
  hg-update-newlib
  hg-update-binutils
  hg-update-compiler-rt
  hg-update-google-perftools
}

hg-assert-safe-to-update() {
  local name="$1"
  local dir="$2"
  local rev="$3"
  local defstr=$(echo "${name}" | tr '[a-z]-' '[A-Z]_')

  if ! hg-has-changes "${dir}"; then
    return 0
  fi

  if hg-at-revision "${dir}" "${rev}" ; then
    return 0
  fi

  Banner \
    "                         ERROR                          " \
    "                                                        " \
    " hg/${name} needs to be updated to the stable revision  " \
    " but has local modifications.                           " \
    "                                                        " \
    " If your repository is behind stable, update it using:  " \
    "                                                        " \
    "        cd hg/${name}; hg update ${rev}                 " \
    "        (you may need to resolve conflicts)             " \
    "                                                        " \
    " If your repository is ahead of stable, then modify:    " \
    "   ${defstr}_REV   (in tools/llvm/utman.sh)             " \
    " to suppress this error message.                        "
  exit -1
}

svn-assert-safe-to-update() {
  local name="$1"
  local dir="$2"
  local rev="$3"
  local defstr=$(echo "${name}" | tr '[a-z]-' '[A-Z]_')

  if ! svn-has-changes "${dir}"; then
    return 0
  fi

  if svn-at-revision "${dir}" "${rev}" ; then
    return 0
  fi

  Banner \
    "                         ERROR                          " \
    "                                                        " \
    " hg/${name} needs to be updated to the stable revision  " \
    " but has local modifications.                           " \
    "                                                        " \
    " If your repository is behind stable, update it using:  " \
    "                                                        " \
    "        cd hg/${name}; svn update -r ${rev}             " \
    "        (you may need to resolve conflicts)             " \
    "                                                        " \
    " If your repository is ahead of stable, then modify:    " \
    "   ${defstr}_REV   (in tools/llvm/utman.sh)             " \
    " to suppress this error message.                        "
  exit -1
}

hg-bot-sanity() {
  local name="$1"
  local dir="$2"

  if ! ${UTMAN_BUILDBOT} ; then
    return 0
  fi

  if ! hg-on-branch "${dir}" pnacl-sfi ||
     hg-has-changes "${dir}" ||
     hg-has-untracked "${dir}" ; then
    Banner "WARNING: hg/${name} is in an illegal state." \
           "         Wiping and trying again."
    rm -rf "${dir}"
    hg-checkout-${name}
  fi
}

svn-bot-sanity() {
  local name="$1"
  local dir="$2"

  if ! ${UTMAN_BUILDBOT} ; then
    return 0
  fi

  if svn-has-changes "${dir}" ; then
    Banner "WARNING: hg/${name} is in an illegal state." \
           "         Wiping and trying again."
    rm -rf "${dir}"
    svn-checkout-${name}
  fi
}


hg-update-common() {
  local name="$1"
  local rev="$2"
  local dir="$3"

  # If this is a buildbot, do sanity checks here.
  hg-bot-sanity "${name}" "${dir}"

  # Make sure it is safe to update
  hg-assert-branch "${dir}" pnacl-sfi
  hg-assert-safe-to-update "${name}" "${dir}" "${rev}"

  if hg-at-revision "${dir}" "${rev}" ; then
    StepBanner "HG-UPDATE" "Repo ${name} already at ${rev}"
  else
    StepBanner "HG-UPDATE" "Updating ${name} to ${rev}"
    hg-pull "${dir}"
    hg-update "${dir}" ${rev}
  fi
}

svn-update-common() {
  local name="$1"
  local rev="$2"
  local dir="$3"

  # If this is a buildbot, do sanity checks here.
  svn-bot-sanity "${name}" "${dir}"

  # Make sure it is safe to update
  svn-assert-safe-to-update "${name}" "${dir}" "${rev}"

  if svn-at-revision "${dir}" "${rev}" ; then
    StepBanner "SVN-UPDATE" "Repo ${name} already at ${rev}"
  else
    StepBanner "SVN-UPDATE" "Updating ${name} to ${rev}"
    svn-update "${dir}" ${rev}
  fi
}

hg-update-upstream() {
  llvm-unlink-clang
  if ! ${UTMAN_MERGE_TESTING} ; then
    hg-update-common "upstream" ${UPSTREAM_REV} "${TC_SRC_UPSTREAM}"
  fi
  llvm-link-clang
}

svn-update-llvm-master() {
  svn-update-common "llvm-master" ${LLVM_MASTER_REV} "${TC_SRC_LLVM_MASTER}"
}

svn-update-llvm-gcc-master() {
  svn-update-common "llvm-gcc-master" ${LLVM_GCC_MASTER_REV} \
                    "${TC_SRC_LLVM_GCC_MASTER}"
}

svn-update-clang() {
  svn-update-common "clang" ${CLANG_REV} "${TC_SRC_CLANG}"
}

svn-update-dragonegg() {
  svn-update-common "dragonegg" ${DRAGONEGG_REV} "${TC_SRC_DRAGONEGG}"
}

#@ hg-update-newlib      - Update NEWLIB To the stable revision
hg-update-newlib() {
  # Clean the headers first, so that sanity checks inside
  # hg-update-common do not see any local modifications.
  newlib-nacl-headers-check
  newlib-nacl-headers-clean
  hg-update-common "newlib" ${NEWLIB_REV} "${TC_SRC_NEWLIB}"
  newlib-nacl-headers
}

#@ hg-update-binutils    - Update BINUTILS to the stable revision
hg-update-binutils() {
  # Clean the binutils generated file first, so that sanity checks
  # inside hg-update-common do not see any local modifications.
  binutils-mess-hide
  hg-update-common "binutils" ${BINUTILS_REV} "${TC_SRC_BINUTILS}"
  binutils-mess-unhide
}

#@ hg-update-compiler-rt - Update compiler-rt to the stable revision
hg-update-compiler-rt() {
  hg-update-common "compiler-rt" ${COMPILER_RT_REV} "${TC_SRC_COMPILER_RT}"
}

#@ hg-update-google-perftools - Update google-perftools to the stable revision
hg-update-google-perftools() {
  hg-update-common "google-perftools" ${GOOGLE_PERFTOOLS_REV} \
    "${TC_SRC_GOOGLE_PERFTOOLS}"
}

#@ hg-pull-all           - Pull all repos. (but do not update working copy)
#@ hg-pull-REPO          - Pull repository REPO.
#@                         (REPO can be llvm-gcc, llvm, newlib, binutils)
hg-pull-all() {
  StepBanner "HG-PULL" "Running 'hg pull' in all repos..."
  hg-pull-upstream
  hg-pull-newlib
  hg-pull-binutils
  hg-pull-compiler-rt
  hg-pull-google-perftools
}

hg-pull-upstream() {
  hg-pull "${TC_SRC_UPSTREAM}"
}

hg-pull-newlib() {
  hg-pull "${TC_SRC_NEWLIB}"
}

hg-pull-binutils() {
  hg-pull "${TC_SRC_BINUTILS}"
}

hg-pull-compiler-rt() {
  hg-pull "${TC_SRC_COMPILER_RT}"
}

hg-pull-google-perftools() {
  hg-pull "${TC_SRC_GOOGLE_PERFTOOLS}"
}

#@ checkout-all          - check out repos needed to build toolchain
#@                          (skips repos which are already checked out)
checkout-all() {
  StepBanner "CHECKOUT-ALL"
  hg-checkout-upstream
  svn-checkout-clang
  svn-checkout-dragonegg
  hg-checkout-binutils
  hg-checkout-newlib
  hg-checkout-compiler-rt
  hg-checkout-google-perftools
  git-sync
}

hg-checkout-upstream() {
  if ! ${UTMAN_MERGE_TESTING} ; then
    hg-checkout ${REPO_UPSTREAM} "${TC_SRC_UPSTREAM}" ${UPSTREAM_REV}
  fi
  llvm-link-clang
}

svn-checkout-llvm-master() {
  svn-checkout "${REPO_LLVM_MASTER}" "${TC_SRC_LLVM_MASTER}" ${LLVM_MASTER_REV}
}

svn-checkout-llvm-gcc-master() {
  svn-checkout "${REPO_LLVM_GCC_MASTER}" "${TC_SRC_LLVM_GCC_MASTER}" \
               ${LLVM_GCC_MASTER_REV}
}

svn-checkout-clang() {
  svn-checkout "${REPO_CLANG}" "${TC_SRC_CLANG}" ${CLANG_REV}
}

svn-checkout-dragonegg() {
  svn-checkout "${REPO_DRAGONEGG}" "${TC_SRC_DRAGONEGG}" ${DRAGONEGG_REV}
}

hg-checkout-binutils() {
  hg-checkout ${REPO_BINUTILS} "${TC_SRC_BINUTILS}" ${BINUTILS_REV}
}

hg-checkout-newlib() {
  hg-checkout ${REPO_NEWLIB} "${TC_SRC_NEWLIB}" ${NEWLIB_REV}
  newlib-nacl-headers
}

hg-checkout-compiler-rt() {
  hg-checkout ${REPO_COMPILER_RT} "${TC_SRC_COMPILER_RT}" ${COMPILER_RT_REV}
}

hg-checkout-google-perftools() {
  hg-checkout ${REPO_GOOGLE_PERFTOOLS} "${TC_SRC_GOOGLE_PERFTOOLS}" \
    ${GOOGLE_PERFTOOLS_REV}
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
    rm -rf "${PNACL_HG_ROOT}"
  else
    StepBanner "HG-CLEAN" "Clean cancelled by user"
  fi
}

git-sync() {
  local gitbase="${PNACL_GIT_ROOT}"

  mkdir -p "${gitbase}"
  cp "${PNACL_ROOT}"/gclient_template "${gitbase}/.gclient"

  if ! [ -d "${gitbase}/dummydir" ]; then
    spushd "${gitbase}"
    gclient update
    spopd
  fi

  cp "${PNACL_ROOT}"/DEPS "${gitbase}"/dummydir
  spushd "${gitbase}"
  gclient update
  spopd
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

  if [ -d "${installdir}" ]; then
    mv "${installdir}" "${dldir}"
  fi

  if [ -d "${tmpdir}" ]; then
    mv "${tmpdir}" "${installdir}"
  fi
}

#@-------------------------------------------------------------------------

#@ download-toolchains   - Download and Install all SDKs (arm,x86-32,x86-64)

download-toolchains() {
  gclient runhooks --force
}

libc() {
  if ${LIBMODE_NEWLIB} ; then
    # TODO(pdox): Why is this step needed?
    sysroot
    newlib
  elif ${LIBMODE_GLIBC} ; then
    glibc
  fi
}


#@ libs            - install native libs and build bitcode libs
libs() {
  libs-clean
  libs-platform
  if ${LIBMODE_NEWLIB} ; then
    # TODO(pdox): Why is this step needed?
    sysroot
    newlib
  elif ${LIBMODE_GLIBC} ; then
    glibc
  fi

  if ${LIBMODE_NEWLIB}; then
    build-compiler-rt
    # NOTE: this currently depends on "llvm-gcc arm"
    build-libgcc_eh arm
    build-libgcc_eh x86-32
    build-libgcc_eh x86-64

    libstdcpp
  fi
}

#@ everything            - Build and install untrusted SDK. no translator
everything() {
  everything-hg

  everything-post-hg
}

#@ everything            - Checkout everything from the repositories
everything-hg() {
  mkdir -p "${INSTALL_ROOT}"
  if ${UTMAN_IN_CROS_CHROOT}; then
    # TODO: http://code.google.com/p/nativeclient/issues/detail?id=2295
    Banner "You are running in a ChromiumOS Chroot." \
      " You should make sure that the PNaCl sources are properly checked out " \
      " And updated outside of the chroot"
  else
    checkout-all
    StepBanner "Updating upstreaming repository"
    update-all
  fi
}

#@ everything-post-hg does everything AFTER hg setup
everything-post-hg() {
  mkdir -p "${INSTALL_ROOT}"
  # This is needed to build misc-tools and run ARM tests.
  # We check this early so that there are no surprises later, and we can
  # handle all user interaction early.
  check-for-trusted

  clean-install

  clean-logs

  binutils
  llvm
  driver
  llvm-gcc arm

  libs

  # NOTE: we delay the tool building till after the sdk is essentially
  #      complete, so that sdk sanity checks don't fail
  misc-tools
  verify
}

#@ everything-translator   - Build and install untrusted SDK AND translator
everything-translator() {

  everything
  # Building the sandboxed tools requires the SDK
  if ${UTMAN_PRUNE}; then
    prune
  fi
  sdk
  install-translators srpc
  if ${UTMAN_PRUNE}; then
    prune-translator-install srpc
    track-translator-size ${SBTC_BUILD_WITH_PNACL}
  fi
}

glibc() {
  StepBanner "GLIBC" "Copying glibc from NNaCl toolchain"

  mkdir -p "${INSTALL_LIB_X8632}"
  mkdir -p "${INSTALL_LIB_X8664}"
  mkdir -p "${GLIBC_INSTALL_DIR}"

  # Files in: lib/gcc/${NACL64_TARGET}/4.4.3/[32]/
  local LIBS1="crtbegin.o crtbeginT.o crtbeginS.o crtend.o crtendS.o \
               libgcc_eh.a libgcc.a"

  # Files in: ${NACL64_TARGET}/lib[32]/
  local LIBS2="crt1.o crti.o crtn.o \
               libgcc_s.so libgcc_s.so.1 \
               libstdc++.a libstdc++.so* \
               libc.a libc_nonshared.a \
               libc-2.9.so libc.so libc.so.* \
               libm-2.9.so libm.a libm.so libm.so.* \
               libdl-2.9.so libdl.so.* libdl.so libdl.a \
               libpthread-2.9.so libpthread.a libpthread.so \
               libpthread.so.* libpthread_nonshared.a \
               runnable-ld.so \
               ld-2.9.so"

  for lib in ${LIBS1} ; do
    cp -a "${NNACL_GLIBC_ROOT}/lib/gcc/${NACL64_TARGET}/4.4.3/32/"${lib} \
       "${INSTALL_LIB_X8632}"
    cp -a "${NNACL_GLIBC_ROOT}/lib/gcc/${NACL64_TARGET}/4.4.3/"${lib} \
       "${INSTALL_LIB_X8664}"
  done

  for lib in ${LIBS2} ; do
    cp -a "${NNACL_GLIBC_ROOT}/${NACL64_TARGET}/lib32/"${lib} \
          "${INSTALL_LIB_X8632}"
    cp -a "${NNACL_GLIBC_ROOT}/${NACL64_TARGET}/lib/"${lib} \
          "${INSTALL_LIB_X8664}"
  done

  # Copy linker scripts
  # We currently only depend on elf[64]_nacl.x,
  # elf[64]_nacl.xs, and elf[64]_nacl.x.static.
  cp -a "${NNACL_GLIBC_ROOT}/${NACL64_TARGET}/lib/ldscripts/elf_nacl.x"* \
        "${INSTALL_LIB_X8632}"
  cp -a "${NNACL_GLIBC_ROOT}/${NACL64_TARGET}/lib/ldscripts/elf64_nacl.x"* \
        "${INSTALL_LIB_X8664}"

  # ld-nacl have different sonames across 32/64.
  # Create symlinks to make them look the same.
  # TODO(pdox): Can this be fixed in glibc?
  ln -sf "ld-2.9.so" "${INSTALL_LIB_X8632}"/ld-nacl-x86-32.so.1
  ln -sf "ld-2.9.so" "${INSTALL_LIB_X8632}"/ld-nacl-x86-64.so.1
  ln -sf "ld-2.9.so" "${INSTALL_LIB_X8664}"/ld-nacl-x86-32.so.1
  ln -sf "ld-2.9.so" "${INSTALL_LIB_X8664}"/ld-nacl-x86-64.so.1

  # Copy the glibc headers
  cp -a "${NNACL_GLIBC_ROOT}"/${NACL64_TARGET}/include \
        "${GLIBC_INSTALL_DIR}"
}

#@ all                   - Alias for 'everything'
all() {
  everything
}

#@ status                - Show status of build directories
status() {
  # TODO(robertm): this is currently broken
  StepBanner "BUILD STATUS"

  status-helper "BINUTILS"          binutils
  status-helper "LLVM"              llvm
  status-helper "GCC-STAGE1"        llvm-gcc

  status-helper "NEWLIB"            newlib
  status-helper "LIBSTDCPP"         libstdcpp

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
  clean-scons
}

#@ fast-clean            - Clean everything except LLVM.
fast-clean() {
  local did_backup=false
  local backup_dir="${NACL_ROOT}/llvm-build-backup"

  if [ -d "${TC_BUILD_LLVM}" ]; then
    rm -rf "${backup_dir}"
    mv "${TC_BUILD_LLVM}" "${backup_dir}"
    did_backup=true
  fi

  clean

  if ${did_backup} ; then
    mkdir -p "${TC_BUILD}"
    mv "${backup_dir}" "${TC_BUILD_LLVM}"
  fi
}

binutils-mess-hide() {
  local messtmp="${PNACL_HG_ROOT}/binutils.tmp"
  if [ -f "${BINUTILS_MESS}" ] ; then
    mv "${BINUTILS_MESS}" "${messtmp}"
  fi
}

binutils-mess-unhide() {
  local messtmp="${PNACL_HG_ROOT}/binutils.tmp"
  if [ -f "${messtmp}" ] ; then
    mv "${messtmp}" "${BINUTILS_MESS}"
  fi
}

#+ clean-scons           - Clean scons-out directory
clean-scons() {
  rm -rf scons-out
}

#+ clean-build           - Clean all build directories
clean-build() {
  rm -rf "${TC_BUILD}"
}

#+ clean-install         - Clean install directories
clean-install() {
  rm -rf "${INSTALL_ROOT}"
}

#+ libs-clean            - Removes the library directories
libs-clean() {
  StepBanner "LIBS-CLEAN" "Cleaning ${INSTALL_ROOT}/libs-*"
  rm -rf "${INSTALL_LIB}"/*
  rm -rf "${INSTALL_LIB}"-*/*
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
  everything-translator

  # Remove the SDK so it doesn't end up in the tarball
  sdk-clean

  if ${UTMAN_PRUNE}; then
    prune
  fi
  tarball $1
}

#+ prune                 - Prune toolchain
prune() {
  StepBanner "PRUNE" "Pruning toolchain"
  # ACCEPTABLE_SIZE should be much lower for real release,
  # but we are currently doing a debug build and not pruning
  # as aggressively as we could.
  local ACCEPTABLE_SIZE=300
  local dir_size_before=$(get_dir_size_in_mb ${INSTALL_ROOT})

  SubBanner "Size before: ${INSTALL_ROOT} ${dir_size_before}MB"
  echo "removing some static libs we do not have any use for"
  rm  -f "${NEWLIB_INSTALL_DIR}"/lib/lib*.a

  echo "stripping binaries (llvm-gcc, llvm, binutils)"
  strip \
    "${LLVM_GCC_INSTALL_DIR}"/libexec/gcc/${CROSS_TARGET_ARM}/${LLVM_GCC_VER}/c*
  strip "${BINUTILS_INSTALL_DIR}"/bin/*
  if ! strip "${LLVM_INSTALL_DIR}"/bin/* ; then
    echo "NOTE: some failures during stripping are expected"
  fi
  if ! strip "${LLVM_GCC_INSTALL_DIR}"/bin/* ; then
    echo "NOTE: some failures during stripping are expected"
  fi

  echo "removing llvm headers"
  rm -rf "${LLVM_INSTALL_DIR}"/include/llvm*

  echo "removing .pyc files"
  rm -f "${INSTALL_BIN}"/*.pyc

  if ${LIBMODE_GLIBC}; then
    echo "remove pnacl_cache directory"
    rm -rf "${INSTALL_LIB}"/pnacl_cache
    rm -rf "${INSTALL_SDK_LIB}"/pnacl_cache
  fi

  echo "remove driver log"
  rm -f "${INSTALL_ROOT}"/driver.log

  local dir_size_after=$(get_dir_size_in_mb "${INSTALL_ROOT}")
  SubBanner "Size after: ${INSTALL_ROOT} ${dir_size_after}MB"

  if [[ ${dir_size_after} -gt ${ACCEPTABLE_SIZE} ]] ; then
    # TODO(pdox): Move this to the buildbot script so that
    # it can make the bot red without ruining the toolchain archive.
    echo "WARNING: size of toolchain exceeds ${ACCEPTABLE_SIZE}MB"
  fi

}

#+ tarball <filename>    - Produce tarball file
tarball() {
  if [ ! -n "${1:-}" ]; then
    echo "Error: tarball needs a tarball name." >&2
    exit 1
  fi

  RecordRevisionInfo
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
  StepBanner "LLVM (HOST)"

  local srcdir="${TC_SRC_LLVM}"

  assert-dir "${srcdir}" "You need to checkout LLVM."

  if llvm-needs-configure; then
    llvm-clean
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

#+ llvm-link-clang       - Add tools/clang symlink into llvm directory
llvm-link-clang() {
  rm -f "${TC_SRC_LLVM}"/tools/clang
  ln -sf "../../../clang" "${TC_SRC_LLVM}"/tools/clang
}

#+ llvm-unlink-clang     - Remove tools/clang symlink from llvm directory
llvm-unlink-clang() {
  if [ -d "${TC_SRC_LLVM}" ]; then
    rm -f "${TC_SRC_LLVM}"/tools/clang
  fi
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

  llvm-link-clang
  # The --with-binutils-include is to allow llvm to build the gold plugin
  local binutils_include="${TC_SRC_BINUTILS}/binutils-2.20/include"
  RunWithLog "llvm.configure" \
      env -i PATH=/usr/bin/:/bin \
             MAKE_OPTS=${MAKE_OPTS} \
             CC="${CC}" \
             CXX="${CXX}" \
             ${srcdir}/configure \
             --disable-jit \
             --with-binutils-include=${binutils_include} \
             --enable-targets=x86,x86_64,arm \
             --target=${CROSS_TARGET_ARM} \
             --prefix="${LLVM_INSTALL_DIR}" \
             --with-llvmgccdir="${LLVM_GCC_INSTALL_DIR}" \
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
  local objdir="${TC_BUILD_LLVM}"
  ts-modified "${TC_SRC_LLVM}" "${objdir}" ||
    ts-modified "${TC_SRC_CLANG}" "${objdir}"
  return $?
}

#+ llvm-make             - Run LLVM 'make'
llvm-make() {
  StepBanner "LLVM" "Make"

  local srcdir="${TC_SRC_LLVM}"
  local objdir="${TC_BUILD_LLVM}"
  llvm-link-clang

  spushd "${objdir}"

  ts-touch-open "${objdir}"

  RunWithLog llvm.make \
    env -i PATH=/usr/bin/:/bin \
           MAKE_OPTS="${MAKE_OPTS}" \
           NACL_SANDBOX=0 \
           CC="${CC}" \
           CXX="${CXX}" \
           make ${MAKE_OPTS} all

  ts-touch-commit  "${objdir}"

  spopd
}

#+ llvm-install          - Install LLVM
llvm-install() {
  StepBanner "LLVM" "Install"

  spushd "${TC_BUILD_LLVM}"
  llvm-link-clang
  RunWithLog llvm.install \
       make ${MAKE_OPTS} install
  spopd

  llvm-install-links
}

llvm-install-links() {
  local makelink="ln -sf"

  # On Windows, these can't be symlinks.
  if ${BUILD_PLATFORM_WIN}; then
    makelink="cp -a"
  fi

  mkdir -p "${BFD_PLUGIN_DIR}"

  # TODO(pdox): These may no longer be necessary.
  if [ -f "${BFD_PLUGIN_DIR}/../../../llvm/${SO_DIR}/LLVMgold${SO_EXT}" ]; then
    # this is to make sure whatever name LLVMgold.so is, it is always
    # libLLVMgold.so as far as PNaCl is concerned

    StepBanner "Symlinking LLVMgold.so to libLLVMgold.so in " \
     "${BFD_PLUGIN_DIR}/../../../llvm/${SO_DIR}"

    (cd "${BFD_PLUGIN_DIR}/../../../llvm/${SO_DIR}";
      ${makelink} "LLVMgold${SO_EXT}" "${SO_PREFIX}LLVMgold${SO_EXT}";
    )
  fi

  spushd "${BFD_PLUGIN_DIR}"

  ${makelink} ../../../llvm/${SO_DIR}/${SO_PREFIX}LLVMgold${SO_EXT} .
  ${makelink} ../../../llvm/${SO_DIR}/${SO_PREFIX}LTO${SO_EXT} .
  spopd

  spushd "${BINUTILS_INSTALL_DIR}/${SO_DIR}"
  ${makelink} ../../llvm/${SO_DIR}/${SO_PREFIX}LTO${SO_EXT} .
  ${makelink} ../../llvm/${SO_DIR}/${SO_PREFIX}LLVMgold${SO_EXT} .
  spopd
}

#########################################################################
#########################################################################
# GCC/DragonEgg based front-end
#########################################################################
#########################################################################

###############################  GMP  ###############################
#+ gmp                   - Build and install gmp
gmp() {
  gmp-unpack
  if gmp-needs-configure; then
    gmp-clean
    gmp-configure
  else
    SkipBanner "GMP" "Already configured"
  fi
  gmp-install
}

gmp-needs-configure() {
  ! [ -f "${TC_BUILD_GMP}/config.status" ]
  return $?
}

gmp-clean() {
  StepBanner "GMP" "Clean"
  rm -rf "${TC_BUILD_GMP}"
}

gmp-unpack() {
  if ! [ -d "${TC_SRC_GMP}" ]; then
    mkdir -p "${TC_SRC_GMP}"
    tar -jxf "${THIRD_PARTY_GMP}" -C "${TC_SRC_GMP}"
  fi
}

gmp-configure() {
  StepBanner "GMP" "Configure"
  mkdir -p "${TC_BUILD_GMP}"
  spushd "${TC_BUILD_GMP}"
  RunWithLog gmp.configure \
    env -i \
      PATH="/usr/bin:/bin" \
      CC="${CC}" \
      CXX="${CXX}" \
      "${TC_SRC_GMP}"/${GMP_VER}/configure \
         --prefix="${GMP_INSTALL_DIR}" \
         --disable-shared --enable-static
  spopd
}

gmp-install() {
  StepBanner "GMP" "Install"
  spushd "${TC_BUILD_GMP}"
  RunWithLog gmp.install \
    env -i PATH=/usr/bin/:/bin \
      CC="${CC}" \
      CXX="${CXX}" \
      make ${MAKE_OPTS} install
  spopd
}

###############################  MPFR  ###############################
#+ mpfr                  - Build and install mpfr
mpfr() {
  mpfr-unpack
  if mpfr-needs-configure; then
    mpfr-clean
    mpfr-configure
  else
    SkipBanner "MPFR" "Already configured"
  fi

  mpfr-install
}

mpfr-needs-configure() {
  ! [ -f "${TC_BUILD_MPFR}/config.status" ]
  return $?
}

mpfr-clean() {
  StepBanner "MPFR" "Clean"
  rm -rf "${TC_BUILD_MPFR}"
}

mpfr-unpack() {
  if ! [ -d "${TC_SRC_MPFR}" ]; then
    mkdir -p "${TC_SRC_MPFR}"
    tar -jxf "${THIRD_PARTY_MPFR}" -C "${TC_SRC_MPFR}"
  fi
}

mpfr-configure() {
  StepBanner "MPFR" "Configure"
  mkdir -p "${TC_BUILD_MPFR}"
  spushd "${TC_BUILD_MPFR}"

  RunWithLog mpfr.configure \
    env -i \
      PATH="/usr/bin:/bin" \
      CC="${CC}" \
      CXX="${CXX}" \
      "${TC_SRC_MPFR}"/${MPFR_VER}/configure \
        --prefix="${MPFR_INSTALL_DIR}" \
        --with-gmp="${GMP_INSTALL_DIR}" \
        --disable-shared --enable-static
  spopd
}

mpfr-install() {
  StepBanner "MPFR" "Install"
  spushd "${TC_BUILD_MPFR}"
  RunWithLog mpfr.install \
    env -i PATH=/usr/bin/:/bin \
      CC="${CC}" \
      CXX="${CXX}" \
      make ${MAKE_OPTS} install
  spopd
}

###############################  MPC  ###############################
#+ mpc                   - Build and install mpc
mpc() {
  mpc-unpack
  if mpc-needs-configure; then
    mpc-clean
    mpc-configure
  else
    SkipBanner "MPC" "Already configured"
  fi
  mpc-install
}

mpc-needs-configure() {
  ! [ -f "${TC_BUILD_MPC}/config.status" ]
  return $?
}

mpc-unpack() {
  if ! [ -d "${TC_SRC_MPC}" ]; then
    mkdir -p "${TC_SRC_MPC}"
    tar -zxf "${THIRD_PARTY_MPC}" -C "${TC_SRC_MPC}"
  fi
}

mpc-clean() {
  StepBanner "MPC" "Clean"
  rm -rf "${TC_BUILD_MPC}"
}

mpc-configure() {
  StepBanner "MPC" "Configure"
  mkdir -p "${TC_BUILD_MPC}"
  spushd "${TC_BUILD_MPC}"
  RunWithLog mpc.configure \
    env -i \
      PATH="/usr/bin:/bin" \
      CC="${CC}" \
      CXX="${CXX}" \
      "${TC_SRC_MPC}"/${MPC_VER}/configure \
        --prefix="${MPC_INSTALL_DIR}" \
        --with-gmp="${GMP_INSTALL_DIR}" \
        --with-mpfr="${MPFR_INSTALL_DIR}" \
        --disable-shared --enable-static
  spopd
}

mpc-install() {
  StepBanner "MPC" "Install"
  spushd "${TC_BUILD_MPC}"
  RunWithLog mpc.install \
    env -i PATH=/usr/bin/:/bin \
      CC="${CC}" \
      CXX="${CXX}" \
      make ${MAKE_OPTS} install
  spopd
}


##########################  GCC FRONT-END  ############################
#+ gccfe                 - Build and install the gcc frontend
gccfe() {
  StepBanner "GCC FRONTEND"
  gccfe-deps
  if gccfe-needs-configure ; then
    gccfe-clean
    gccfe-configure
  else
    SkipBanner "GCC-FE" "Already configured"
  fi
  gccfe-make
  gccfe-install
  dragonegg-plugin
}

gccfe-deps-clean() {
  gmp-clean
  mpfr-clean
  mpc-clean
}

gccfe-deps() {
  gmp
  mpfr
  mpc
}

gccfe-needs-configure() {
 [ ! -f "${TC_BUILD_GCC}/config.status" ]
 return $?
}

gccfe-clean() {
  StepBanner "GCC-FE" "Clean"
  rm -rf "${TC_BUILD_GCC}"
}

gccfe-configure() {
  StepBanner "GCC-FE" "Configure"
  mkdir -p "${TC_BUILD_GCC}"
  spushd "${TC_BUILD_GCC}"
  RunWithLog gccfe.configure \
    env -i \
      PATH="/usr/bin:/bin" \
      CC="${CC}" \
      CXX="${CXX}" \
      ${TC_SRC_GCC}/configure \
          --prefix="${GCC_INSTALL_DIR}" \
          --with-gmp="${GMP_INSTALL_DIR}" \
          --with-mpfr="${MPFR_INSTALL_DIR}" \
          --with-mpc="${MPC_INSTALL_DIR}" \
          --disable-libmudflap \
          --disable-decimal-float \
          --disable-libssp \
          --disable-libgomp \
          --disable-multilib \
          --disable-libquadmath \
          --disable-libquadmath-support \
          --enable-languages=c,c++ \
          --disable-threads \
          --disable-libstdcxx-pch \
          --disable-shared \
          --without-headers \
          --enable-lto \
          --enable-plugin \
          --target=i686-unknown-linux-gnu
  spopd
}

gccfe-make() {
  StepBanner "GCC-FE" "Make"
  spushd "${TC_BUILD_GCC}"
  RunWithLog gccfe.make \
    env -i \
      PATH="/usr/bin:/bin" \
      CC="${CC}" \
      CXX="${CXX}" \
      make ${MAKE_OPTS} all-gcc
  spopd
}

gccfe-install() {
  StepBanner "GCC-FE" "Install"
  rm -rf "${GCC_INSTALL_DIR}"
  spushd "${TC_BUILD_GCC}"
  RunWithLog gccfe.install \
    env -i \
      PATH="/usr/bin:/bin" \
      CC="${CC}" \
      CXX="${CXX}" \
      make ${MAKE_OPTS} install-gcc
  spopd
}

#+-------------------------------------------------------------------------
#+ dragonegg-plugin      - build and install dragon-egg plugin
dragonegg-plugin() {
  StepBanner "DRAGONEGG" "Building and installing plugin"
  rm -rf "${TC_BUILD_DRAGONEGG}"
  cp -a "${TC_SRC_DRAGONEGG}" "${TC_BUILD_DRAGONEGG}"
  spushd "${TC_BUILD_DRAGONEGG}"
  RunWithLog dragonegg.make \
    env -i \
      PATH="/usr/bin:/bin" \
      PWD="$(pwd)" \
      CC="${CC}" \
      CXX="${CXX}" \
      CFLAGS="-I${GMP_INSTALL_DIR}/include -L${GMP_INSTALL_DIR}/lib" \
      CXXFLAGS="-I${GMP_INSTALL_DIR}/include -L${GMP_INSTALL_DIR}/lib" \
      GCC="${GCC_INSTALL_DIR}/bin/i686-unknown-linux-gnu-gcc" \
      LLVM_CONFIG="${LLVM_INSTALL_DIR}"/bin/llvm-config \
      make ${MAKE_OPTS}
  cp dragonegg.so "${GCC_INSTALL_DIR}/lib"
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


LLVM_GCC_SETUP=false
llvm-gcc-setup() {
  # If this is an internal invocation, don't setup again.
  if ${LLVM_GCC_SETUP} && [ $# -eq 0 ]; then
    return 0
  fi

  if [ $# -ne 1 ] ; then
    Fatal "Please specify architecture: x86-32, x86-64, arm"
  fi
  local arch=$1

  case ${arch} in
    arm) LLVM_GCC_TARGET=${CROSS_TARGET_ARM} ;;
    x86-32) LLVM_GCC_TARGET=${CROSS_TARGET_X86_32} ;;
    x86-64) LLVM_GCC_TARGET=${CROSS_TARGET_X86_64} ;;
    *) Fatal "Unrecognized architecture ${arch}" ;;
  esac
  LLVM_GCC_SETUP=true
  LLVM_GCC_ARCH=${arch}
  LLVM_GCC_BUILD_DIR="${TC_BUILD_LLVM_GCC}-${LLVM_GCC_ARCH}"
  return 0
}

#+-------------------------------------------------------------------------
#+ llvm-gcc            - build and install pre-gcc
llvm-gcc() {
  llvm-gcc-setup "$@"
  StepBanner "LLVM-GCC (HOST) for ${LLVM_GCC_ARCH}"

  if llvm-gcc-needs-configure; then
    llvm-gcc-clean
    llvm-gcc-configure
  else
    SkipBanner "LLVM-GCC ${LLVM_GCC_ARCH}" "configure"
  fi

  # We must always make before we do make install, because
  # the build must occur in a patched environment.
  # http://code.google.com/p/nativeclient/issues/detail?id=1128
  llvm-gcc-make

  llvm-gcc-install
}

#+ sysroot               - setup initial sysroot
sysroot() {
  StepBanner "LLVM-GCC" "Setting up initial sysroot"

  local sys_include="${SYSROOT_DIR}/include"
  local sys_include2="${SYSROOT_DIR}/sys-include"

  rm -rf "${sys_include}" "${sys_include2}"
  mkdir -p "${sys_include}"
  ln -sf "${sys_include}" "${sys_include2}"
  cp -r "${NEWLIB_INCLUDE_DIR}"/* "${sys_include}"
}

#+ llvm-gcc-clean      - Clean gcc stage 1
llvm-gcc-clean() {
  llvm-gcc-setup "$@"
  StepBanner "LLVM-GCC" "Clean"
  local objdir="${LLVM_GCC_BUILD_DIR}"
  rm -rf "${objdir}"
}

llvm-gcc-needs-configure() {
  llvm-gcc-setup "$@"
  speculative-check "llvm" && return 0
  ts-newer-than "${TC_BUILD_LLVM}" \
                "${LLVM_GCC_BUILD_DIR}" && return 0
  [ ! -f "${LLVM_GCC_BUILD_DIR}/config.status" ]
  return $?
}

#+ llvm-gcc-configure  - Configure GCC stage 1
llvm-gcc-configure() {
  llvm-gcc-setup "$@"
  StepBanner "LLVM-GCC" "Configure ${LLVM_GCC_TARGET}"

  local srcdir="${TC_SRC_LLVM_GCC}"
  local objdir="${LLVM_GCC_BUILD_DIR}"

  mkdir -p "${objdir}"
  spushd "${objdir}"

  # NOTE: hack, assuming presence of x86/32 toolchain (used for both 32/64)
  local config_opts=""
  case ${LLVM_GCC_ARCH} in
      arm)
          config_opts="--with-as=${PNACL_AS_ARM} \
                       --with-arch=${ARM_ARCH} \
                       --with-fpu=${ARM_FPU}"
          ;;
      x86-32)
          config_opts="--with-as=${PNACL_AS_X8632}"
          ;;
      x86-64)
          config_opts="--with-as=${PNACL_AS_X8664}"
          ;;
  esac

  local flags=""
  if ${LIBMODE_NEWLIB}; then
    flags+="--with-newlib"
  fi

  RunWithLog llvm-pregcc-${LLVM_GCC_ARCH}.configure \
      env -i PATH=/usr/bin/:/bin \
             CC="${CC}" \
             CXX="${CXX}" \
             CFLAGS="-Dinhibit_libc" \
             AR_FOR_TARGET="${PNACL_AR}" \
             RANLIB_FOR_TARGET="${PNACL_RANLIB}" \
             NM_FOR_TARGET="${PNACL_NM}" \
             ${srcdir}/configure \
               --prefix="${LLVM_GCC_INSTALL_DIR}" \
               --enable-llvm="${LLVM_INSTALL_DIR}" \
               ${flags} \
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
               --target=${LLVM_GCC_TARGET}

  spopd
}

llvm-gcc-make() {
  llvm-gcc-setup "$@"
  local srcdir="${TC_SRC_LLVM_GCC}"
  local objdir="${LLVM_GCC_BUILD_DIR}"
  spushd ${objdir}

  StepBanner "LLVM-GCC" "Make (Stage 1)"

  ts-touch-open "${objdir}"

  RunWithLog llvm-pregcc-${LLVM_GCC_ARCH}.make \
       env -i PATH=/usr/bin/:/bin \
              CC="${CC}" \
              CXX="${CXX}" \
              CFLAGS="-Dinhibit_libc" \
              make ${MAKE_OPTS} all-gcc

  ts-touch-commit "${objdir}"

  spopd
}

#+ llvm-gcc-install    - Install GCC stage 1
llvm-gcc-install() {
  llvm-gcc-setup "$@"
  StepBanner "LLVM-GCC" "Install ${LLVM_GCC_ARCH}"

  local objdir="${LLVM_GCC_BUILD_DIR}"
  spushd "${objdir}"

  RunWithLog llvm-pregcc-${LLVM_GCC_ARCH}.install \
       env -i PATH=/usr/bin/:/bin \
              CC="${CC}" \
              CXX="${CXX}" \
              CFLAGS="-Dinhibit_libc" \
              make ${MAKE_OPTS} install

  spopd
}

#########################################################################
#########################################################################
#     < LIBGCC_EH >
#########################################################################
#########################################################################

#+ build-libgcc_eh - build/install libgcc_eh
build-libgcc_eh() {
  # TODO(pdox): This process needs some major renovation.
  # We are using the llvm-gcc ARM build directory, but varying '-arch'
  # to get different versions of libgcc_eh.
  # NOTE: For simplicity we piggyback the libgcc_eh build onto a preconfigured
  #       objdir. So, to be safe, you have to run gcc-stage1-make first
  local arch=$1
  local srcdir="${TC_SRC_LLVM_GCC}"
  local objdir="${TC_BUILD_LLVM_GCC}-arm"
  spushd "${objdir}"/gcc
  StepBanner "libgcc_eh-${arch}" "cleaning"
  RunWithLog libgcc_eh.clean \
      env -i PATH=/usr/bin/:/bin \
             make clean-target-libgcc
  rm -f "${objdir}"/gcc/libgcc_eh.a

  # NOTE: usually gcc/libgcc.mk is generate and invoked implicitly by
  #       gcc/Makefile.
  #       Since we are calling it directly we need to make up for some
  #       missing flags, e.g.  include paths ann defines like
  #       'ATTRIBUTE_UNUSED' which is used to mark unused function
  #       parameters.
  #       The arguments were gleaned from build logs.
  StepBanner "libgcc_eh-${arch}" "building ($1)"
  local flags
  flags="-arch ${arch} --pnacl-bias=${arch} --pnacl-allow-translate"
  flags+=" -DATTRIBUTE_UNUSED= -DHOST_BITS_PER_INT=32 -Dinhibit_libc"
  flags+=" -DIN_GCC -DCROSS_DIRECTORY_STRUCTURE "

  setup-libstdcpp-env
  RunWithLog libgcc_eh.${arch}.make \
       env -i PATH=/usr/bin/:/bin \
              "${STD_ENV_FOR_LIBSTDCPP[@]}" \
              "INCLUDES=-I${srcdir}/include -I${srcdir}/gcc -I." \
              "LIBGCC2_CFLAGS=${flags}" \
              "AR_CREATE_FOR_TARGET=${PNACL_AR} rc" \
              make ${MAKE_OPTS} -f libgcc.mk libgcc_eh.a
  spopd

  StepBanner "libgcc_eh-${arch}" "installing"
  mkdir -p "${INSTALL_LIB}-${arch}"
  cp "${objdir}"/gcc/libgcc_eh.a "${INSTALL_LIB}-${arch}"
}

#########################################################################
#########################################################################
#     < COMPILER-RT >
#########################################################################
#########################################################################

#+ build-compiler-rt - build/install llvm's replacement for libgcc.a
build-compiler-rt() {
  local src="${TC_SRC_COMPILER_RT}/compiler-rt/lib"
  mkdir -p "${TC_BUILD_COMPILER_RT}"
  spushd "${TC_BUILD_COMPILER_RT}"
  StepBanner "COMPILER-RT (LIBGCC)"
  for arch in arm x86-32 x86-64; do
    StepBanner "compiler rt" "build ${arch}"
    rm -rf "${arch}"
    mkdir -p "${arch}"
    spushd "${arch}"
    RunWithLog libgcc.${arch}.make \
        make -j ${UTMAN_CONCURRENCY} -f ${src}/Makefile-pnacl libgcc.a \
          CC="${PNACL_CC}" \
          AR="${PNACL_AR}" \
          "SRC_DIR=${src}" \
          "CFLAGS=-arch ${arch} --pnacl-allow-translate -O3 -fPIC"
    spopd
  done

  StepBanner "compiler rt" "install all"
  ls -l */libgcc.a

  mkdir -p "${INSTALL_LIB_ARM}"
  cp arm/libgcc.a "${INSTALL_LIB_ARM}/"

  mkdir -p "${INSTALL_LIB_X8632}"
  cp x86-32/libgcc.a "${INSTALL_LIB_X8632}/"

  mkdir -p "${INSTALL_LIB_X8664}"
  cp x86-64/libgcc.a "${INSTALL_LIB_X8664}/"
  spopd
}

#########################################################################
#########################################################################
#                          < LIBSTDCPP >
#########################################################################
#########################################################################

libstdcpp-setup() {
  # BUG=http://code.google.com/p/nativeclient/issues/detail?id=2289
  prefer-frontend llvm-gcc
}

libstdcpp() {
  libstdcpp-setup
  StepBanner "LIBSTDCPP (BITCODE)"

  if libstdcpp-needs-configure; then
    libstdcpp-clean
    libstdcpp-configure
  else
    SkipBanner "LIBSTDCPP" "configure"
  fi

  if libstdcpp-needs-make; then
    libstdcpp-make
  else
    SkipBanner "LIBSTDCPP" "make"
  fi

  libstdcpp-install
  reset-frontend
}

#+ libstdcpp-clean - clean libstdcpp in bitcode
libstdcpp-clean() {
  StepBanner "LIBSTDCPP" "Clean"
  rm -rf "${TC_BUILD_LIBSTDCPP}"
}

libstdcpp-needs-configure() {
  speculative-check "llvm-gcc" && return 0
  ts-newer-than "${TC_BUILD_LLVM_GCC}-${CROSS_TARGET_ARM}" \
                "${TC_BUILD_LIBSTDCPP}" && return 0
  [ ! -f "${TC_BUILD_LIBSTDCPP}/config.status" ]
  return #?
}

libstdcpp-configure() {
  libstdcpp-setup
  StepBanner "LIBSTDCPP" "Configure"
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="${TC_BUILD_LIBSTDCPP}"

  mkdir -p "${objdir}"
  spushd "${objdir}"

  local flags=""
  if ${LIBMODE_NEWLIB}; then
    flags+="--with-newlib --disable-shared"
  elif ${LIBMODE_GLIBC}; then
    # TODO(pdox): Fix right away.
    flags+="--disable-shared"
  else
    Fatal "Unknown library mode"
  fi

  setup-libstdcpp-env
  RunWithLog llvm-gcc.configure_libstdcpp \
      env -i PATH=/usr/bin/:/bin \
        "${STD_ENV_FOR_LIBSTDCPP[@]}" \
        "${srcdir}"/configure \
          --host="${CROSS_TARGET_ARM}" \
          --prefix="${LIBSTDCPP_INSTALL_DIR}" \
          --enable-llvm="${LLVM_INSTALL_DIR}" \
          ${flags} \
          --disable-libstdcxx-pch \
          --enable-languages=c,c++ \
          --target=${CROSS_TARGET_ARM} \
          --with-arch=${ARM_ARCH} \
          --srcdir="${srcdir}"
  spopd
}

libstdcpp-needs-make() {
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="${TC_BUILD_LIBSTDCPP}"

  ts-modified "${srcdir}" "${objdir}"
  return $?
}

libstdcpp-make() {
  libstdcpp-setup
  StepBanner "LIBSTDCPP" "Make"
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="${TC_BUILD_LIBSTDCPP}"

  ts-touch-open "${objdir}"

  spushd "${objdir}"
  setup-libstdcpp-env
  RunWithLog llvm-gcc.make_libstdcpp \
    env -i PATH=/usr/bin/:/bin \
        make \
        "${STD_ENV_FOR_LIBSTDCPP[@]}" \
        ${MAKE_OPTS}
  spopd

  ts-touch-commit "${objdir}"
}

libstdcpp-install() {
  libstdcpp-setup
  StepBanner "LIBSTDCPP" "Install"
  local objdir="${TC_BUILD_LIBSTDCPP}"

  spushd "${objdir}"

  # install headers (=install-data)
  # for good measure make sure we do not keep any old headers
  rm -rf "${INSTALL_ROOT}/include/c++"
  setup-libstdcpp-env
  RunWithLog llvm-gcc.install_libstdcpp \
    make \
    "${STD_ENV_FOR_LIBSTDCPP[@]}" \
    ${MAKE_OPTS} install-data

  # Install bitcode library
  mkdir -p "${INSTALL_LIB}"
  cp "${objdir}/src/.libs/libstdc++.a" "${INSTALL_LIB}"

  spopd
}

#+ misc-tools            - Build and install sel_ldr and validator for ARM.
misc-tools() {
  if ${UTMAN_BUILD_ARM} ; then
    StepBanner "MISC-ARM" "Building sel_ldr (ARM)"

    # TODO(robertm): revisit some of these options
    RunWithLog arm_sel_ldr \
      ./scons MODE=opt-host \
      platform=arm \
      sdl=none \
      naclsdk_validate=0 \
      sysinfo=0 \
      sel_ldr
    rm -rf  "${INSTALL_ROOT}/tools-arm"
    mkdir "${INSTALL_ROOT}/tools-arm"
    local sconsdir="scons-out/opt-${SCONS_BUILD_PLATFORM}-arm"
    cp "${sconsdir}/obj/src/trusted/service_runtime/sel_ldr" \
       "${INSTALL_ROOT}/tools-arm"
  else
    StepBanner "MISC-ARM" "Skipping ARM sel_ldr (No trusted ARM toolchain)"
  fi

  if ${BUILD_PLATFORM_LINUX} ; then
    StepBanner "MISC-ARM" "Building validator (ARM)"
    RunWithLog arm_ncval_core \
      ./scons MODE=opt-host \
      targetplatform=arm \
      sysinfo=0 \
      arm-ncval-core
    rm -rf  "${INSTALL_ROOT}/tools-x86"
    mkdir "${INSTALL_ROOT}/tools-x86"
    cp scons-out/opt-linux-x86-32-to-arm/obj/src/trusted/validator_arm/\
arm-ncval-core ${INSTALL_ROOT}/tools-x86
  else
    StepBanner "MISC-ARM" "Skipping ARM validator (Not yet supported on Mac)"
  fi
}


#########################################################################
#     < BINUTILS >
#########################################################################

#+-------------------------------------------------------------------------
#+ binutils          - Build and install binutils for ARM
binutils() {
  StepBanner "BINUTILS (HOST)"

  local srcdir="${TC_SRC_BINUTILS}"

  assert-dir "${srcdir}" "You need to checkout binutils."

  if binutils-needs-configure; then
    binutils-clean
    binutils-configure
  else
    SkipBanner "BINUTILS" "configure"
  fi

  if binutils-needs-make; then
    binutils-make
  else
    SkipBanner "BINUTILS" "make"
  fi

  binutils-install
}

#+ binutils-clean    - Clean binutils
binutils-clean() {
  StepBanner "BINUTILS" "Clean"
  local objdir="${TC_BUILD_BINUTILS}"
  rm -rf ${objdir}
}

#+ binutils-configure- Configure binutils for ARM
binutils-configure() {
  StepBanner "BINUTILS" "Configure"

  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS}"

  # enable multiple targets so that we can use the same ar with all .o files
  local targ="arm-pc-nacl,i686-pc-nacl,x86_64-pc-nacl"
  mkdir -p "${objdir}"
  spushd "${objdir}"

  # The --enable-gold and --enable-plugins options are on so that we
  # can use gold's support for plugin to link PNaCl modules.

  # TODO(pdox): Building binutils for nacl/nacl64 target currently requires
  # providing NACL_ALIGN_* defines. This should really be defined inside
  # binutils instead.
  RunWithLog binutils.configure \
    env -i \
    PATH="/usr/bin:/bin" \
    CC="${CC}" \
    CXX="${CXX}" \
    CFLAGS="-DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5" \
    ${srcdir}/binutils-2.20/configure --prefix="${BINUTILS_INSTALL_DIR}" \
                                      --target=${BINUTILS_TARGET} \
                                      --enable-targets=${targ} \
                                      --enable-gold=yes \
                                      --enable-ld=yes \
                                      --enable-plugins \
                                      --disable-werror \
                                      --with-sysroot="${NONEXISTENT_PATH}"
  # There's no point in setting the correct path as sysroot, because we
  # want the toolchain to be relocatable. The driver will use ld command-line
  # option --sysroot= to override this value and set it to the correct path.
  # However, we need to include --with-sysroot during configure to get this
  # option. So fill in a non-sense, non-existent path.

  spopd
}

binutils-needs-configure() {
  [ ! -f "${TC_BUILD_BINUTILS}/config.status" ]
  return $?
}

binutils-needs-make() {
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS}"
  local ret=1
  binutils-mess-hide
  ts-modified "$srcdir" "$objdir" && ret=0
  binutils-mess-unhide
  return ${ret}
}

#+ binutils-make     - Make binutils for ARM
binutils-make() {
  StepBanner "BINUTILS" "Make"
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS}"
  spushd "${objdir}"

  ts-touch-open "${objdir}"

  RunWithLog binutils.make \
    env -i PATH="/usr/bin:/bin" \
    make ${MAKE_OPTS}

  ts-touch-commit "${objdir}"

  spopd
}

#+ binutils-install  - Install binutils for ARM
binutils-install() {
  StepBanner "BINUTILS" "Install"
  local objdir="${TC_BUILD_BINUTILS}"
  spushd "${objdir}"

  RunWithLog binutils.install \
    env -i PATH="/usr/bin:/bin" \
    make \
      install ${MAKE_OPTS}

  spopd
}

#+-------------------------------------------------------------------------
#+ binutils-liberty      - Build native binutils libiberty
binutils-liberty() {
  local srcdir="${TC_SRC_BINUTILS}"

  assert-dir "${srcdir}" "You need to checkout binutils."

  if binutils-liberty-needs-configure; then
    binutils-liberty-clean
    binutils-liberty-configure
  else
    SkipBanner "BINUTILS-LIBERTY" "configure"
  fi

  if binutils-liberty-needs-make; then
    binutils-liberty-make
  else
    SkipBanner "BINUTILS-LIBERTY" "make"
  fi
}

binutils-liberty-needs-configure() {
  [ ! -f "${TC_BUILD_BINUTILS_LIBERTY}/config.status" ]
  return $?
}

#+ binutils-liberty-clean    - Clean binutils-liberty
binutils-liberty-clean() {
  StepBanner "BINUTILS-LIBERTY" "Clean"
  local objdir="${TC_BUILD_BINUTILS_LIBERTY}"
  rm -rf ${objdir}
}

#+ binutils-liberty-configure - Configure binutils-liberty
binutils-liberty-configure() {
  StepBanner "BINUTILS-LIBERTY" "Configure"

  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS_LIBERTY}"

  mkdir -p "${objdir}"
  spushd "${objdir}"
  RunWithLog binutils.liberty.configure \
      env -i \
      PATH="/usr/bin:/bin" \
      CC="${CC}" \
      CXX="${CXX}" \
      ${srcdir}/binutils-2.20/configure
  spopd
}

binutils-liberty-needs-make() {
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS_LIBERTY}"

  ts-modified "$srcdir" "$objdir"
  return $?
}

#+ binutils-liberty-make - Make binutils-liberty
binutils-liberty-make() {
  StepBanner "BINUTILS-LIBERTY" "Make"
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS_LIBERTY}"
  spushd "${objdir}"

  ts-touch-open "${objdir}"

  RunWithLog binutils.liberty.make \
      env -i \
      PATH="/usr/bin:/bin" \
      CC="${CC}" \
      CXX="${CXX}" \
      make ${MAKE_OPTS} all-libiberty

  ts-touch-commit "${objdir}"

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
  local arch=$1
  for valid_arch in x8632 x8664 arm universal ; do
    if [ "${arch}" == "${valid_arch}" ] ; then
      return
    fi
  done

  Fatal "ERROR: Unsupported arch. Choose one of: x8632, x8664, arm, universal"
}

LLVM_SB_SETUP=false
llvm-sb-setup() {
  local flags=""
  if ${SB_JIT}; then
    llvm-sb-setup-jit "$@"
    return
  fi

  if ${LLVM_SB_SETUP} && [ $# -eq 0 ]; then
    return 0
  fi

  if [ $# -ne 2 ] ; then
    Fatal "Please specify arch and mode"
  fi

  LLVM_SB_SETUP=true

  LLVM_SB_ARCH=$1
  LLVM_SB_MODE=$2
  check-sb-arch ${LLVM_SB_ARCH}
  check-sb-mode ${LLVM_SB_MODE}

  LLVM_SB_LOG_PREFIX="llvm.sb.${LLVM_SB_ARCH}.${LLVM_SB_MODE}"
  LLVM_SB_OBJDIR="${TC_BUILD}/llvm-sb-${LLVM_SB_ARCH}-${LLVM_SB_MODE}"
  if ${LIBMODE_NEWLIB}; then
    flags+=" -static"
  fi

  case ${LLVM_SB_MODE} in
    srpc)    flags+=" -DNACL_SRPC" ;;
    nonsrpc) ;;
  esac

  # Speed things up by avoiding an intermediate step
  flags+=" --pnacl-skip-ll"

  LLVM_SB_EXTRA_CONFIG_FLAGS="--disable-jit --enable-optimized \
  --target=${CROSS_TARGET_ARM}"

  if ${LIBMODE_GLIBC} ; then
    prefer-frontend clang
  else
    prefer-frontend llvm-gcc
  fi

  LLVM_SB_CONFIGURE_ENV=(
    AR="${PNACL_AR}" \
    AS="${PNACL_AS}" \
    CC="${PNACL_CC} ${flags}" \
    CXX="${PNACL_CXX} ${flags}" \
    LD="${PNACL_LD} ${flags}" \
    NM="${PNACL_NM}" \
    RANLIB="${PNACL_RANLIB}" \
    LDFLAGS="") # TODO(pdox): Support -s

  reset-frontend
}

llvm-sb-setup-jit() {
  local flags=""

  if ${LLVM_SB_SETUP} && [ $# -eq 0 ]; then
    return 0
  fi

  if [ $# -ne 2 ] ; then
    Fatal "Please specify arch and mode"
  fi

  LLVM_SB_SETUP=true

  LLVM_SB_ARCH=$1
  LLVM_SB_MODE=$2
  check-sb-arch ${LLVM_SB_ARCH}
  check-sb-mode ${LLVM_SB_MODE}

  LLVM_SB_LOG_PREFIX="llvm.sb.${LLVM_SB_ARCH}.${LLVM_SB_MODE}.jit"
  LLVM_SB_OBJDIR="${TC_BUILD}/llvm-sb-${LLVM_SB_ARCH}-${LLVM_SB_MODE}.jit"
  case ${LLVM_SB_MODE} in
    srpc)    flags+=" -DNACL_SRPC" ;;
    nonsrpc) ;;
  esac

  local naclgcc_root="";
  naclgcc_root="${NNACL_GLIBC_ROOT}"

  LLVM_SB_EXTRA_CONFIG_FLAGS="--enable-jit --disable-optimized \
  --target=${LLVM_SB_ARCH}-nacl"

  LLVM_SB_CONFIGURE_ENV=(
    AR="${naclgcc_root}/bin/i686-nacl-ar" \
    As="${naclgcc_root}/bin/i686-nacl-as" \
    CC="${naclgcc_root}/bin/i686-nacl-gcc ${flags}" \
    CXX="${naclgcc_root}/bin/i686-nacl-g++ ${flags}" \
    LD="${naclgcc_root}/bin/i686-nacl-ld" \
    NM="${naclgcc_root}/bin/i686-nacl-nm" \
    RANLIB="${naclgcc_root}/bin/i686-nacl-ranlib" \
    LDFLAGS="") # TODO(pdox): Support -s
}

#+-------------------------------------------------------------------------
#+ llvm-sb <arch> <mode> - Build and install llvm tools (sandboxed)
llvm-sb() {
  llvm-sb-setup "$@"
  local srcdir="${TC_SRC_LLVM}"
  assert-dir "${srcdir}" "You need to checkout llvm."

  if llvm-sb-needs-configure ; then
    llvm-sb-clean
    llvm-sb-configure
  else
    SkipBanner "LLVM-SB" "configure ${LLVM_SB_ARCH} ${LLVM_SB_MODE}"
  fi

  if llvm-sb-needs-make; then
    llvm-sb-make
  else
    SkipBanner "LLVM-SB" "make"
  fi

  llvm-sb-install
}

llvm-sb-needs-configure() {
  llvm-sb-setup "$@"
  [ ! -f "${LLVM_SB_OBJDIR}/config.status" ]
  return $?
}

# llvm-sb-clean          - Clean llvm tools (sandboxed)
llvm-sb-clean() {
  llvm-sb-setup "$@"
  StepBanner "LLVM-SB" "Clean ${LLVM_SB_ARCH} ${LLVM_SB_MODE}"
  local objdir="${LLVM_SB_OBJDIR}"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"
}

# llvm-sb-configure - Configure llvm tools (sandboxed)
llvm-sb-configure() {
  llvm-sb-setup "$@"

  StepBanner "LLVM-SB" "Configure ${LLVM_SB_ARCH} ${LLVM_SB_MODE}"
  local srcdir="${TC_SRC_LLVM}"
  local objdir="${LLVM_SB_OBJDIR}"
  local installdir="${INSTALL_SB_TOOLS}/${LLVM_SB_ARCH}/${LLVM_SB_MODE}"
  local targets=""
  case ${LLVM_SB_ARCH} in
    x8632) targets=x86 ;;
    x8664) targets=x86_64 ;;
    arm) targets=arm ;;
    universal) targets=x86,x86_64,arm ;;
  esac

  spushd "${objdir}"
  RunWithLog \
      ${LLVM_SB_LOG_PREFIX}.configure \
      env -i \
      PATH="/usr/bin:/bin" \
      ${srcdir}/configure \
        "${LLVM_SB_CONFIGURE_ENV[@]}" \
        --prefix=${installdir} \
        --host=nacl \
        --enable-targets=${targets} \
        --enable-pic=no \
        --enable-static \
        --enable-shared=no \
        ${LLVM_SB_EXTRA_CONFIG_FLAGS}
  spopd
}

llvm-sb-needs-make() {
  llvm-sb-setup "$@"
  ts-modified "${TC_SRC_LLVM}" "${LLVM_SB_OBJDIR}"
  return $?
}

# llvm-sb-make - Make llvm tools (sandboxed)
llvm-sb-make() {
  llvm-sb-setup "$@"

  StepBanner "LLVM-SB" "Make ${LLVM_SB_ARCH} ${LLVM_SB_MODE}"
  local objdir="${LLVM_SB_OBJDIR}"

  spushd "${objdir}"
  ts-touch-open "${objdir}"

  local build_with_srpc=0
  if [ "${LLVM_SB_MODE}" == "srpc" ]; then
    build_with_srpc=1
  fi

  local use_tcmalloc=0
  # TODO(dschuff): Decide whether we should switch back to tcmalloc
  #if ${LIBMODE_NEWLIB} && ! ${SB_JIT}; then
  #  # only use tcmalloc with newlib (glibc malloc should be pretty good)
  #  # (also, SB_JIT implies building with glibc, but for now it uses nacl-gcc)
  #  use_tcmalloc=1
  #fi

  RunWithLog ${LLVM_SB_LOG_PREFIX}.make \
      env -i PATH="/usr/bin:/bin" \
      ONLY_TOOLS="llc lli"\
      NACL_SANDBOX=1 \
      NACL_SRPC=${build_with_srpc} \
      KEEP_SYMBOLS=1 \
      VERBOSE=1 \
      make USE_TCMALLOC=$use_tcmalloc \
           ${MAKE_OPTS} tools-only

  ts-touch-commit "${objdir}"

  spopd
}

# llvm-sb-install - Install llvm tools (sandboxed)
llvm-sb-install() {
  llvm-sb-setup "$@"

  StepBanner "LLVM-SB" "Install ${LLVM_SB_ARCH} ${LLVM_SB_MODE}"
  local objdir="${LLVM_SB_OBJDIR}"
  spushd "${objdir}"

  RunWithLog ${LLVM_SB_LOG_PREFIX}.install \
      env -i PATH="/usr/bin:/bin" \
      ONLY_TOOLS="llc lli"\
      NACL_SANDBOX=1 \
      KEEP_SYMBOLS=1 \
      make ${MAKE_OPTS} install

  spopd

  if ! ${SB_JIT} ; then
    translate-and-install-sb-tool ${LLVM_SB_ARCH} ${LLVM_SB_MODE} llc
  else
    install-naclgcc-tool ${LLVM_SB_ARCH} ${LLVM_SB_MODE} llc
    install-naclgcc-tool ${LLVM_SB_ARCH} ${LLVM_SB_MODE} lli
  fi
}

install-naclgcc-tool() {
  local arch=$1
  local mode=$2
  local name=$3

  local bindir="${INSTALL_SB_TOOLS}/${arch}/${mode}/bin"
  local tarch=x8632
  mv "${bindir}/${name}" "${bindir}/${name}.${tarch}.nexe"

  local bindir_tarch="${INSTALL_SB_TOOLS}/${tarch}/${mode}/bin"
  local nexe="${bindir}/${name}.${tarch}.nexe"
  mkdir -p "${bindir_tarch}"
  cp -f "${nexe}" "${bindir_tarch}/${name}"
}

translate-and-install-sb-tool() {
  local arch=$1
  local mode=$2
  local name=$3

  # Translate bitcode program into an actual native executable.
  # If arch = universal, we need to translate and install multiple times.
  local bindir="${INSTALL_SB_TOOLS}/${arch}/${mode}/bin"
  local pexe="${bindir}/${name}.pexe"

  # Rename to .pexe
  mv "${bindir}/${name}" "${pexe}"

  local arches
  if [ "${arch}" == "universal" ]; then
    arches="${SBTC_BUILD_WITH_PNACL}"
  else
    arches="${arch}"
  fi

  local installer
  if [ "${arch}" == "universal" ]; then
    installer="cp -f"
  else
    installer="mv -f"
  fi

  # In universal/mode/bin directory, we'll end up with every translation:
  # e.g. ${name}.arm.nexe, ${name}.x8632.nexe, ${name}.x8664.nexe
  # In arch/mode/bin directories, we'll end up with just one copy
  local num_arches=$(wc -w <<< "${arches}")
  local extra=""
  if [ ${num_arches} -gt 1 ] && QueueConcurrent; then
    extra=" (background)"
  fi

  for tarch in ${arches}; do
    local nexe="${bindir}/${name}.${tarch}.nexe"
    StepBanner "TRANSLATE" "Translating ${name}.pexe to ${tarch}${extra}"
    "${PNACL_TRANSLATE}" -arch ${tarch} "${pexe}" -o "${nexe}" &
    QueueLastProcess
  done

  if [ ${num_arches} -gt 1 ] && ! QueueEmpty ; then
    StepBanner "TRANSLATE" "Waiting for processes to finish"
  fi
  QueueWait

  for tarch in ${arches}; do
    local nexe="${bindir}/${name}.${tarch}.nexe"
    local bindir_tarch="${INSTALL_SB_TOOLS}/${tarch}/${mode}/bin"
    mkdir -p "${bindir_tarch}"
    ${installer} "${nexe}" "${bindir_tarch}/${name}"
  done
}

google-perftools-clean() {
  rm -rf "${TC_BUILD_GOOGLE_PERFTOOLS}"
}

google-perftools-needs-configure() {
  [ ! -f "${TC_BUILD_GOOGLE_PERFTOOLS}/config.status" ]
  return $?
}

google-perftools-needs-make() {
  local srcdir="${TC_SRC_GOOGLE_PERFTOOLS}"
  local objdir="${TC_BUILD_GOOGLE_PERFTOOLS}"
  ts-modified "${srcdir}" "${objdir}"
}

#+ google-perftools-configure - conifgure tcmalloc-minimal for bitcode
google-perftools-configure() {
  local src="${TC_SRC_GOOGLE_PERFTOOLS}"/google-perftools
  local flags="-static"
  local configure_env=(
    CC="${PNACL_CC} ${flags}" \
    CXX="${PNACL_CXX} ${flags}" \
    LD="${PNACL_LD} ${flags}" \
    AR="${PNACL_AR}" \
    RANLIB="${PNACL_RANLIB}")
  local install="${TC_BUILD_GOOGLE_PERFTOOLS}"/install

  StepBanner "GOOGLE-PERFTOOLS" "Configure"
  mkdir -p "${TC_BUILD_GOOGLE_PERFTOOLS}"
  mkdir -p "${install}"
  spushd "${TC_BUILD_GOOGLE_PERFTOOLS}"
  RunWithLog google-perftools.configure \
      ${src}/configure --prefix="${install}" \
      --enable-minimal \
      --host=nacl \
      "${configure_env[@]}"
  spopd
}

#+ google-perftools-make - Make tcmalloc-minimal in bitcode
google-perftools-make() {
  local objdir="${TC_BUILD_GOOGLE_PERFTOOLS}"
  spushd "${objdir}"
  StepBanner "GOOGLE-PERFTOOLS" "Make"
  ts-touch-open "${objdir}"
  RunWithLog google-perftools.make \
    make -j ${UTMAN_CONCURRENCY}
  ts-touch-commit "${objdir}"
  spopd
}

#+ google-perftools-install - Install libtcmalloc_minimal.a into toolchain
google-perftools-install() {
  StepBanner "GOOGLE-PERFTOOLS" "Install"
  spushd "${TC_BUILD_GOOGLE_PERFTOOLS}"
  RunWithLog google-perftools.install \
    make install

  mkdir -p "${INSTALL_LIB}"
  cp "${TC_BUILD_GOOGLE_PERFTOOLS}"/install/lib/libtcmalloc_minimal.a \
    "${INSTALL_LIB}"
  spopd
}

#+ google-perftools - Build libtcmalloc_minimal for use with newlib
#                           sandboxed binaries
google-perftools() {
  StepBanner "GOOGLE-PERFTOOLS (tcmalloc)"

  if google-perftools-needs-configure; then
    google-perftools-clean
    google-perftools-configure
  else
    SkipBanner "GOOGLE-PERFTOOLS" "Configure"
  fi

  if google-perftools-needs-make; then
      google-perftools-make
  else
    SkipBanner "GOOGLE-PERFTOOLS" "Make"
  fi
  google-perftools-install
}

#---------------------------------------------------------------------

BINUTILS_SB_SETUP=false
binutils-sb-setup() {
  # TODO(jvoung): investigate if these are only needed by AS or not.
  local flags="-DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5 -DNACL_TOOLCHAIN_PATCH"

  if ${BINUTILS_SB_SETUP} && [ $# -eq 0 ]; then
    return 0
  fi

  if [ $# -ne 2 ] ; then
    Fatal "Please specify arch and mode"
  fi

  BINUTILS_SB_SETUP=true

  BINUTILS_SB_ARCH=$1
  BINUTILS_SB_MODE=$2
  check-sb-arch ${BINUTILS_SB_ARCH}
  check-sb-mode ${BINUTILS_SB_MODE}

  BINUTILS_SB_LOG_PREFIX="binutils.sb.${BINUTILS_SB_ARCH}.${BINUTILS_SB_MODE}"
  BINUTILS_SB_OBJDIR="${TC_BUILD}/"\
"binutils-sb-${BINUTILS_SB_ARCH}-${BINUTILS_SB_MODE}"
  if ${LIBMODE_NEWLIB}; then
    flags+=" -static"
  fi

  case ${BINUTILS_SB_MODE} in
    srpc)    flags+=" -DNACL_SRPC" ;;
    nonsrpc) ;;
  esac

  if [ ! -f "${TC_BUILD_BINUTILS_LIBERTY}/libiberty/libiberty.a" ] ; then
    echo "ERROR: Missing lib. Run this script with binutils-liberty option"
    exit -1
  fi

  # Speed things up by avoiding an intermediate step
  flags+=" --pnacl-skip-ll"

  if ${LIBMODE_GLIBC} ; then
    prefer-frontend clang
  else
    prefer-frontend llvm-gcc
  fi

  BINUTILS_SB_CONFIGURE_ENV=(
    AR="${PNACL_AR}" \
    AS="${PNACL_AS}" \
    CC="${PNACL_CC} ${flags}" \
    CXX="${PNACL_CXX} ${flags}" \
    CC_FOR_BUILD="${CC}" \
    CXX_FOR_BUILD="${CXX}" \
    LD="${PNACL_LD} ${flags}" \
    NM="${PNACL_NM}" \
    RANLIB="${PNACL_RANLIB}" \
    LDFLAGS_FOR_BUILD="-L${TC_BUILD_BINUTILS_LIBERTY}/libiberty/" \
    LDFLAGS="")

  reset-frontend
}

#+-------------------------------------------------------------------------
#+ binutils-sb <arch> <mode> - Build and install binutils (sandboxed)
binutils-sb() {
  binutils-sb-setup "$@"
  local srcdir="${TC_SRC_BINUTILS}"
  assert-dir "${srcdir}" "You need to checkout binutils."

  if binutils-sb-needs-configure ; then
    binutils-sb-clean
    binutils-sb-configure
  else
    SkipBanner "BINUTILS-SB" "configure ${BINUTILS_SB_ARCH} ${BINUTILS_SB_MODE}"
  fi

  if binutils-sb-needs-make; then
    binutils-sb-make
  else
    SkipBanner "BINUTILS-SB" "make"
  fi

  binutils-sb-install
}

binutils-sb-needs-configure() {
  binutils-sb-setup "$@"
  [ ! -f "${BINUTILS_SB_OBJDIR}/config.status" ]
  return $?
}

# binutils-sb-clean - Clean binutils (sandboxed)
binutils-sb-clean() {
  binutils-sb-setup "$@"
  StepBanner "BINUTILS-SB" "Clean ${BINUTILS_SB_ARCH} ${BINUTILS_SB_MODE}"
  local objdir="${BINUTILS_SB_OBJDIR}"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"
}

# binutils-sb-configure - Configure binutils (sandboxed)
binutils-sb-configure() {
  binutils-sb-setup "$@"

  StepBanner "BINUTILS-SB" "Configure ${BINUTILS_SB_ARCH} ${BINUTILS_SB_MODE}"
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${BINUTILS_SB_OBJDIR}"
  local installdir="${INSTALL_SB_TOOLS}/${BINUTILS_SB_ARCH}/${BINUTILS_SB_MODE}"

  case ${BINUTILS_SB_ARCH} in
    x8632) targets=i686-pc-nacl ;;
    x8664) targets=x86_64-pc-nacl ;;
    arm) targets=arm-pc-nacl ;;
    universal) targets=arm-pc-nacl,i686-pc-nacl,x86_64-pc-nacl ;;
  esac

  spushd "${objdir}"
  RunWithLog \
      ${BINUTILS_SB_LOG_PREFIX}.configure \
      env -i \
      PATH="/usr/bin:/bin" \
      ${srcdir}/binutils-2.20/configure \
        "${BINUTILS_SB_CONFIGURE_ENV[@]}" \
        --prefix=${installdir} \
        --host=nacl \
        --target=${BINUTILS_TARGET} \
        --enable-targets=${targets} \
        --disable-nls \
        --disable-werror \
        --enable-static \
        --enable-shared=no
  spopd
}

binutils-sb-needs-make() {
  binutils-sb-setup "$@"
  ts-modified "${TC_SRC_BINUTILS}" "${BINUTILS_SB_OBJDIR}"
  return $?
}

# binutils-sb-make - Make binutils (sandboxed)
binutils-sb-make() {
  binutils-sb-setup "$@"

  StepBanner "BINUTILS-SB" "Make ${BINUTILS_SB_ARCH} ${BINUTILS_SB_MODE}"
  local objdir="${BINUTILS_SB_OBJDIR}"

  spushd "${objdir}"
  ts-touch-open "${objdir}"

  local build_with_srpc=0
  if [ "${BINUTILS_SB_MODE}" == "srpc" ]; then
    build_with_srpc=1
  fi

  RunWithLog ${BINUTILS_SB_LOG_PREFIX}.make \
      env -i PATH="/usr/bin:/bin" \
      NACL_SRPC=${build_with_srpc} \
      make ${MAKE_OPTS} all-ld

  ts-touch-commit "${objdir}"

  spopd
}

# binutils-sb-install - Install binutils (sandboxed)
binutils-sb-install() {
  binutils-sb-setup "$@"

  StepBanner "BINUTILS-SB" "Install ${BINUTILS_SB_ARCH} ${BINUTILS_SB_MODE}"
  local objdir="${BINUTILS_SB_OBJDIR}"
  spushd "${objdir}"

  RunWithLog ${BINUTILS_SB_LOG_PREFIX}.install \
      env -i PATH="/usr/bin:/bin" \
      make install-ld

  spopd

  # First rename and *strip* the installed file. (Beware for debugging).
  local installdir="${INSTALL_SB_TOOLS}/${BINUTILS_SB_ARCH}/${BINUTILS_SB_MODE}"
  ${PNACL_STRIP} "${installdir}/bin/${BINUTILS_TARGET}-ld" \
    -o "${installdir}/bin/ld"
  # Remove old file plus a redundant file.
  rm "${installdir}/bin/${BINUTILS_TARGET}-ld"
  rm "${installdir}/bin/${BINUTILS_TARGET}-ld.bfd"

  # Then translate.
  translate-and-install-sb-tool ${BINUTILS_SB_ARCH} ${BINUTILS_SB_MODE} ld
}

#+ tools-sb {arch} {mode} - Build all sandboxed tools for arch, mode
tools-sb() {
  local arch=$1
  local mode=$2

  StepBanner "${arch}"    "Sandboxing"
  StepBanner "----------" "--------------------------------------"
  llvm-sb ${arch} ${mode}
  binutils-sb ${arch} ${mode}
}


#+--------------------------------------------------------------------------
#@ install-translators {srpc/nonsrpc} - Builds and installs sandboxed
#@                                      translator components
install-translators() {
  if [ $# -ne 1 ]; then
    echo "ERROR: Usage install-translators <srpc/nonsrpc>"
    exit -1
  fi

  if ! [ -d "${INSTALL_SDK_ROOT}" ]; then
    echo "ERROR: SDK must be installed to build translators."
    echo "You can install the SDK by running: $0 sdk"
    exit -1
  fi

  # TODO(dschuff): Decide whether we should switch back to tcmalloc
  #if ${LIBMODE_NEWLIB}; then
  #    google-perftools
  #fi
  local srpc_kind=$1
  check-sb-mode ${srpc_kind}

  StepBanner "INSTALL SANDBOXED TRANSLATORS (${srpc_kind})"

  binutils-liberty

  if ${SBTC_PRODUCTION}; then
    # Build each architecture separately.
    for arch in ${SBTC_BUILD_WITH_PNACL} ; do
      tools-sb ${arch} ${srpc_kind}
    done
  else
    # Using arch `universal` builds the sandboxed tools to a .pexe
    # which support all targets. This .pexe is then translated to
    # each architecture specified in ${SBTC_BUILD_WITH_PNACL}.
    tools-sb universal ${srpc_kind}
  fi

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

  spushd "${INSTALL_SB_TOOLS_X8632}/${srpc_kind}"
  rm -rf include lib nacl share
  rm -rf bin/llvm-config bin/tblgen
  spopd

  spushd "${INSTALL_SB_TOOLS_X8664}/${srpc_kind}"
  rm -rf include lib nacl64 share
  rm -rf bin/llvm-config bin/tblgen
  spopd

  if ! ${SBTC_PRODUCTION}; then
    rm -rf "${INSTALL_SB_TOOLS_UNIVERSAL}"
  fi

  echo "Stripping tools-sb nexes"
  for arch in ${SBTC_BUILD_WITH_PNACL} ; do
    ${PNACL_STRIP} "${INSTALL_SB_TOOLS}/${arch}/${srpc_kind}"/bin/*
  done

  echo "remove driver log"
  rm -f "${INSTALL_ROOT}"/driver.log

  echo "Done"
}

#########################################################################
#     < NEWLIB-BITCODE >
#########################################################################

#+ newlib - Build and install newlib in bitcode.
newlib() {
  StepBanner "NEWLIB (BITCODE)"

  if newlib-needs-configure; then
    newlib-clean
    newlib-configure
  else
    SkipBanner "NEWLIB" "configure"
  fi

  if newlib-needs-make; then
    newlib-make
  else
    SkipBanner "NEWLIB" "make"
  fi

  newlib-install
}

#+ newlib-clean  - Clean bitcode newlib.
newlib-clean() {
  StepBanner "NEWLIB" "Clean"
  rm -rf "${TC_BUILD_NEWLIB}"
}

newlib-needs-configure() {
  speculative-check "llvm-gcc" && return 0
  ts-newer-than "${TC_BUILD_LLVM_GCC}-${CROSS_TARGET_ARM}" \
                   "${TC_BUILD_NEWLIB}" && return 0

  [ ! -f "${TC_BUILD_NEWLIB}/config.status" ]
  return #?
}

#+ newlib-configure - Configure bitcode Newlib
newlib-configure() {
  StepBanner "NEWLIB" "Configure"

  local srcdir="${TC_SRC_NEWLIB}"
  local objdir="${TC_BUILD_NEWLIB}"
  mkdir -p "${objdir}"
  spushd "${objdir}"

  setup-newlib-env
  RunWithLog newlib.configure \
    env -i \
    PATH="/usr/bin:/bin" \
    "${STD_ENV_FOR_NEWLIB[@]}" \
    ${srcdir}/newlib-trunk/configure \
        --disable-multilib \
        --prefix="${NEWLIB_INSTALL_DIR}" \
        --disable-newlib-supplied-syscalls \
        --disable-texinfo \
        --disable-libgloss \
        --enable-newlib-iconv \
        --enable-newlib-io-long-long \
        --enable-newlib-io-long-double \
        --enable-newlib-io-c99-formats \
        --enable-newlib-io-mb \
        --target="${REAL_CROSS_TARGET}"
  spopd
}

newlib-needs-make() {
  local srcdir="${TC_SRC_NEWLIB}"
  local objdir="${TC_BUILD_NEWLIB}"

  ts-modified "$srcdir" "$objdir"
  return $?
}

#+ newlib-make           - Make bitcode Newlib
newlib-make() {
  StepBanner "NEWLIB" "Make"
  local srcdir="${TC_SRC_NEWLIB}"
  local objdir="${TC_BUILD_NEWLIB}"

  ts-touch-open "${objdir}"

  setup-newlib-env
  spushd "${objdir}"
  RunWithLog newlib.make \
    env -i PATH="/usr/bin:/bin" \
    make \
      "${STD_ENV_FOR_NEWLIB[@]}" \
      ${MAKE_OPTS}
  spopd

  ts-touch-commit "${objdir}"

}

#+ newlib-install        - Install Bitcode Newlib using build env.
newlib-install() {
  StepBanner "NEWLIB" "Install"
  local objdir="${TC_BUILD_NEWLIB}"

  spushd "${objdir}"

  # NOTE: we might be better off not using install, as we are already
  #       doing a bunch of copying of headers and libs further down
  setup-newlib-env
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

  StepBanner "NEWLIB" "Extra-install"
  local sys_include=${SYSROOT_DIR}/include
  # NOTE: we provide a new one via extra-sdk
  rm ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET_ARM}/include/pthread.h

  cp ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET_ARM}/include/machine/endian.h \
    ${sys_include}
  cp ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET_ARM}/include/sys/param.h \
    ${sys_include}
  cp ${NEWLIB_INSTALL_DIR}/${CROSS_TARGET_ARM}/include/newlib.h \
    ${sys_include}

  # NOTE: we provide our own pthread.h via extra-sdk
  StepBanner "NEWLIB" "Removing old pthreads headers"
  rm -f "${NEWLIB_INSTALL_DIR}/${CROSS_TARGET_ARM}/usr/include/pthread.h"
  rm -f "${sys_include}/pthread.h"

  StepBanner "NEWLIB" "copying libraries"
  local destdir="${INSTALL_LIB}"
  # We only install libc/libg/libm
  mkdir -p "${destdir}"
  cp ${objdir}/${REAL_CROSS_TARGET}/newlib/lib[cgm].a "${destdir}"

  spopd

  # Clang claims posix thread model, not single as llvm-gcc does.
  # It means that libstdcpp needs pthread.h to be in place.
  # This should go away when we properly import pthread.h with
  # the other newlib headers. This hack is tracked by
  # http://code.google.com/p/nativeclient/issues/detail?id=2333
  StepBanner "NEWLIB" "Copying pthreads headers ahead of time "\
  "(HACK. See http://code.google.com/p/nativeclient/issues/detail?id=2333)"
  sdk-headers
}

# TODO(pdox): Organize these objects better, so that this code is simpler.
libs-platform() {
  # There are currently no platform libs in the glibc build.
  if ${LIBMODE_GLIBC}; then
    return 0
  fi

  local pnacl_cc="${PNACL_CC} --pnacl-allow-native"
  local src="${PNACL_SUPPORT}"
  local tmpdir="${TC_BUILD}/libs-platform"
  rm -rf "${tmpdir}"
  mkdir -p "${tmpdir}"

  # Install crt1.o (linker script)
  StepBanner "LIBS-PLATFORM" "Install crt1.o"
  mkdir -p "${INSTALL_LIB}"
  cp "${src}"/crt1.x ${INSTALL_LIB}/crt1.o

  # Install nacl_startup.bc
  StepBanner "LIBS-PLATFORM" "Install nacl_startup.bc"
  ${pnacl_cc} -c "${src}"/nacl_startup.c -o "${tmpdir}"/nacl_startup.bc
  cp "${tmpdir}"/nacl_startup.bc "${INSTALL_LIB}"

  for platform in arm x86-32 x86-64; do
    StepBanner "LIBS-PLATFORM" "libcrt_platform.a for ${platform}"
    local dest="${INSTALL_LIB}-${platform}"
    local sources="setjmp_${platform/-/_}"
    mkdir -p "${dest}"

    # For ARM, also compile aeabi_read_tp.S
    if  [ ${platform} == arm ] ; then
      sources+=" aeabi_read_tp"
    fi

    local objs=""
    for name in ${sources}; do
      ${pnacl_cc} -arch ${platform} -c "${src}"/${name}.S -o ${tmpdir}/${name}.o
      objs+=" ${tmpdir}/${name}.o"
    done

    ${PNACL_AR} rc "${dest}"/libcrt_platform.a ${objs}
  done
}

#########################################################################
#     < SDK >
#########################################################################
SCONS_COMMON=(./scons
              MODE=nacl
              -j${UTMAN_CONCURRENCY}
              bitcode=1
              sdl=none
              disable_nosys_linker_warnings=1
              naclsdk_validate=0
              --verbose)

sdk() {
  StepBanner "SDK"
  sdk-clean
  sdk-headers
  sdk-libs
  sdk-verify
}

#+ sdk-clean             - Clean sdk stuff
sdk-clean() {
  StepBanner "SDK" "Clean"
  rm -rf "${INSTALL_SDK_ROOT}"

  # clean scons obj dirs
  rm -rf scons-out/nacl-*-pnacl*
}

sdk-headers() {
  mkdir -p "${INSTALL_SDK_INCLUDE}"

  local extra_flags=""
  local neutral_platform="x86-32"
  if ${LIBMODE_GLIBC}; then
    extra_flags="--nacl_glibc"
  fi

  StepBanner "SDK" "Install headers"
  RunWithLog "sdk.headers" \
      "${SCONS_COMMON[@]}" \
      ${extra_flags} \
      platform=${neutral_platform} \
      install_headers \
      includedir="$(PosixToSysPath "${INSTALL_SDK_INCLUDE}")"
}

sdk-libs() {
  StepBanner "SDK" "Install libraries"
  mkdir -p "${INSTALL_SDK_LIB}"

  local extra_flags=""
  local neutral_platform="x86-32"
  if ${LIBMODE_GLIBC}; then
    extra_flags="--nacl_glibc"
  fi

  RunWithLog "sdk.libs.bitcode" \
      "${SCONS_COMMON[@]}" \
      ${extra_flags} \
      platform=${neutral_platform} \
      install_lib \
      libdir="$(PosixToSysPath "${INSTALL_SDK_LIB}")"
}

sdk-verify() {
  # This avoids errors when *.o finds no matches.
  shopt -s nullglob

  StepBanner "SDK" "Verify"

  # Verify bitcode libraries
  SubBanner "VERIFY: ${INSTALL_SDK_LIB}"
  for i in ${INSTALL_SDK_LIB}/*.a ; do
    verify-archive-llvm "$i"
  done

  for i in ${INSTALL_SDK_LIB}/*.pso ; do
    verify-pso "$i"
  done

  shopt -u nullglob
}

newlib-nacl-headers-clean() {
  # Clean the include directory and revert it to its pure state
  if [ -d "${TC_SRC_NEWLIB}" ]; then
    rm -rf "${NEWLIB_INCLUDE_DIR}"
    # If the script is interrupted right here,
    # then NEWLIB_INCLUDE_DIR will not exist, and the repository
    # will be in a bad state. This will be fixed during the next
    # invocation by newlib-nacl-headers.

    # We jump into the parent directory and use a relative path so that
    # hg does not get confused by pathnames which contain a symlink.
    spushd "$(dirname "${NEWLIB_INCLUDE_DIR}")"
    RunWithLog "newlib-freshen" \
      hg-revert "$(basename "${NEWLIB_INCLUDE_DIR}")"
    spopd
  fi
}

#+ newlib-nacl-headers   - Install NaCl headers to newlib
newlib-nacl-headers() {
  StepBanner "newlib-nacl-headers" "Adding nacl headers to newlib"

  assert-dir "${TC_SRC_NEWLIB}" "Newlib is not checked out"

  # Make sure the headers directory has no local changes
  newlib-nacl-headers-check
  newlib-nacl-headers-clean

  # Install the headers
  "${EXPORT_HEADER_SCRIPT}" \
      "${NACL_SYS_HEADERS}" \
      "${NEWLIB_INCLUDE_DIR}"

  # Record the header install time
  ts-touch "${NACL_HEADERS_TS}"
}

#+ newlib-nacl-headers-check - Make sure the newlib nacl headers haven't
#+                             been modified since the last install.
newlib-nacl-headers-check() {
  # The condition where NEWLIB_INCLUDE_DIR does not exist may have been
  # caused by an incomplete call to newlib-nacl-headers-clean().
  # Let it pass this check so that the clean will be able to finish.
  # See the comment in newlib-nacl-headers-clean()
  if ! [ -d "${TC_SRC_NEWLIB}" ] ||
     ! [ -d "${NEWLIB_INCLUDE_DIR}" ]; then
    return 0
  fi

  # Already clean?
  if ! hg-has-changes "${NEWLIB_INCLUDE_DIR}" &&
     ! hg-has-untracked "${NEWLIB_INCLUDE_DIR}" ; then
    return 0
  fi

  if ts-dir-changed "${NACL_HEADERS_TS}" "${NEWLIB_INCLUDE_DIR}"; then
    echo ""
    echo "*******************************************************************"
    echo "*                            ERROR                                *"
    echo "*      The NewLib include directory has local modifications       *"
    echo "*******************************************************************"
    echo "* The NewLib include directory should not be modified directly.   *"
    echo "* Instead, modifications should be done from:                     *"
    echo "*   src/trusted/service_runtime/include                           *"
    echo "*                                                                 *"
    echo "* To destroy the local changes to newlib, run:                    *"
    echo "*  tools/llvm/utman.sh newlib-nacl-headers-clean                  *"
    echo "*******************************************************************"
    echo ""
    if ${UTMAN_BUILDBOT} ; then
      newlib-nacl-headers-clean
    else
      exit -1
    fi
  fi
}

#+-------------------------------------------------------------------------
#+ driver                - Install driver scripts.
driver() {
  StepBanner "DRIVER"
  driver-install
}

# The driver is a simple python script which changes its behavior
# depending on the name it is invoked as.
driver-install() {
  StepBanner "DRIVER" "Installing driver adaptors to ${INSTALL_BIN}"
  mkdir -p "${INSTALL_BIN}"
  rm -f "${INSTALL_BIN}"/pnacl-*

  spushd "${DRIVER_DIR}"
  cp driver_tools.py "${INSTALL_BIN}"
  cp artools.py "${INSTALL_BIN}"
  cp ldtools.py "${INSTALL_BIN}"
  cp pathtools.py "${INSTALL_BIN}"
  for t in pnacl-*; do
    local name=$(basename "$t")
    cp "${t}" "${INSTALL_BIN}/${name/.py}"
    if ${BUILD_PLATFORM_WIN}; then
      cp redirect.bat "${INSTALL_BIN}/${name/.py}.bat"
    fi
  done
  spopd

  # Tell the driver the library mode
  touch "${INSTALL_BIN}"/${LIBMODE}.cfg

  # Install readelf and size
  cp -a "${BINUTILS_INSTALL_DIR}/bin/${BINUTILS_TARGET}-readelf" \
        "${INSTALL_BIN}/readelf"
  cp -a "${BINUTILS_INSTALL_DIR}/bin/${BINUTILS_TARGET}-size" \
        "${INSTALL_BIN}/size"

  # On windows, copy the cygwin DLLs needed by the driver tools
  if ${BUILD_PLATFORM_WIN}; then
    StepBanner "DRIVER" "Copying cygwin libraries"
    local deps="gcc_s-1 iconv-2 win1 intl-8 stdc++-6 z"
    for name in ${deps}; do
      cp "/bin/cyg${name}.dll" "${INSTALL_BIN}"
    done
  fi
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
  svn info > "${INSTALL_ROOT}/REV"
}

######################################################################
######################################################################
#     < VERIFY >
######################################################################
######################################################################

readonly LLVM_DIS=${LLVM_INSTALL_DIR}/bin/llvm-dis
readonly LLVM_BCANALYZER=${LLVM_INSTALL_DIR}/bin/llvm-bcanalyzer
readonly LLVM_OPT=${LLVM_INSTALL_DIR}/bin/opt

# Note: we could replace this with a modified version of tools/elf_checker.py
#       if we do not want to depend on binutils
readonly NACL_OBJDUMP=${BINUTILS_INSTALL_DIR}/bin/${BINUTILS_TARGET}-objdump

# Usage: VerifyArchive <checker> <pattern> <filename>
ExtractAndCheck() {
  local checker="$1"
  local pattern="$2"
  local archive="$3"
  local tmp="/tmp/ar-verify-${RANDOM}"
  rm -rf ${tmp}
  mkdir -p ${tmp}
  cp "${archive}" "${tmp}"
  spushd ${tmp}
  ${PNACL_AR} x $(basename ${archive})
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

IsLinkerScript() {
  local fname="$1"
  local type="$(file --brief --mime-type "${fname}")"
  case "$type" in
    text/x-c)
      # A linker script with C comments looks like C to "file".
      return 0
      ;;
    text/plain)
      return 0
      ;;
  esac
  return 1
}

# Usage: VerifyLinkerScript <filename>
VerifyLinkerScript() {
  local archive="$1"
  # Use cpp to strip the C-style comments.
  ${PNACL_CC} -E -xc "${archive}" | awk -v archive="$(basename ${archive})" '
    BEGIN { status = 0 }
    NF == 0 || $1 == "#" { next }
    $1 == "INPUT" && $2 == "(" && $NF == ")" { next }
    {
      print "FAIL - unexpected linker script(?) contents:", archive
      status = 1
      exit(status)
    }
    END { if (status == 0) print "PASS  (trivial linker script)" }
' || exit -1
}

# Usage: VerifyArchive <checker> <pattern> <filename>
VerifyArchive() {
  local checker="$1"
  local pattern="$2"
  local archive="$3"
  echo -n "verify $(basename "${archive}"): "
  if IsLinkerScript "${archive}"; then
    VerifyLinkerScript "${archive}"
  else
    ExtractAndCheck "$checker" "$pattern" "$archive"
  fi
}

#
# verify-object-llvm <obj>
#
#   Verifies that a given .o file is bitcode and free of ASMSs
verify-object-llvm() {
  # BUG: http://code.google.com/p/nativeclient/issues/detail?id=2344
  if ${LLVM_DIS} "$1" -o - | grep asm | grep -v sideeffect ; then
    echo
    echo "ERROR asm in $1"
    echo
    exit -1
  fi
  if [ ${PIPESTATUS[0]} -ne 0 ]; then
    exit -1
  fi
}



check-elf-abi() {
  # Temporarily disable ELF abi check until DEPS roll
  return 0

  local arch_info="$(${NACL_OBJDUMP} -f $1)"
  if ! grep -q $2 <<< ${arch_info} ; then
    echo "ERROR $1 - bad file format: $2 vs ${arch_info}\n"
    echo ${arch_info}
    exit -1
  fi
}


# verify-object-arm <obj>
#
#   Ensure that the ARCH properties are what we expect, this is a little
#   fragile and needs to be updated when tools change
verify-object-arm() {
  check-elf-abi $1 "elf32-littlearm"
  arch_info="$("${PNACL_READELF}" -A "$1")"
  #TODO(robertm): some refactoring and cleanup needed
  if ! grep -q "Tag_FP_arch: VFPv2" <<< ${arch_info} ; then
    echo "ERROR $1 - bad Tag_FP_arch\n"
    #TODO(robertm): figure out what the right thing to do is here, c.f.
    # http://code.google.com/p/nativeclient/issues/detail?id=966
    "${PNACL_READELF}" -A $1 | grep  Tag_FP_arch
    exit -1
  fi

  if ! grep -q "Tag_CPU_arch: v7" <<< ${arch_info} ; then
    echo "FAIL bad $1 Tag_CPU_arch\n"
    "${PNACL_READELF}" -A $1 | grep Tag_CPU_arch
    exit -1
  fi
}


# verify-object-x86-32 <obj>
#
verify-object-x86-32() {
  check-elf-abi $1 "elf32-i386"
}

# verify-object-x86-64 <obj>
#
verify-object-x86-64() {
  check-elf-abi $1 "elf64-x86-64"
}

#
# verify-pso <psofile>
#
verify-pso() {
  local psofile="$1"
  echo -n "verify $(basename "${psofile}"): "
  if IsLinkerScript "${psofile}"; then
    VerifyLinkerScript "${psofile}"
  else
    verify-object-llvm "$1"
    echo "PASS"
    # TODO(pdox): Add a call to pnacl-meta to check for the "shared" property.
  fi
}

#
# verify-archive-llvm <archive>
# Verifies that a given archive is bitcode and free of ASMSs
#
verify-archive-llvm() {
  if ${LLVM_BCANALYZER} "$1" 2> /dev/null ; then
    # This fires only when we build in single-bitcode-lib mode
    echo -n "verify $(basename "$1"): "
    verify-object-llvm "$1"
    echo "PASS (single-bitcode)"
  else
    # Currently all the files are .o in the llvm archives.
    # Eventually more and more should be .bc.
    VerifyArchive verify-object-llvm '*.bc *.o' "$@"
  fi
}

#
# verify-archive-arm <archive>
# Verifies that a given archive is a proper arm achive
#
verify-archive-arm() {
  VerifyArchive verify-object-arm '*.o *.ons' "$@"
}

#
# verify-archive-x86-32 <archive>
# Verifies that a given archive is a proper x86-32 achive
#
verify-archive-x86-32() {
  VerifyArchive verify-object-x86-32 '*.o *.ons' "$@"
}

#
# verify-archive-x86-64 <archive>
# Verifies that a given archive is a proper x86-64 achive
#
verify-archive-x86-64() {
  VerifyArchive verify-object-x86-64 '*.o *.ons' "$@"
}

#@-------------------------------------------------------------------------
#+ verify                - Verifies that toolchain/pnacl-untrusted ELF files
#+                         are of the correct architecture.
verify() {
  StepBanner "VERIFY"

  # Verify bitcode libraries in lib/
  # The GLibC build does not currently have any bitcode
  # libraries in this location.
  if ${LIBMODE_NEWLIB}; then
    SubBanner "VERIFY: ${INSTALL_LIB}"
    for i in ${INSTALL_LIB}/*.a ; do
      verify-archive-llvm "$i"
    done
  fi

  # Verify platform libraries
  for platform in arm x86-32 x86-64; do
    if [ "${platform}" == "arm" ] && ${LIBMODE_GLIBC}; then
      continue
    fi

    SubBanner "VERIFY: ${INSTALL_LIB}-${platform}"
    # There are currently no .o files here
    #for i in "${INSTALL_LIB}-${platform}"/*.o ; do
    #  verify-object-${platform} "$i"
    #done

    for i in "${INSTALL_LIB}-${platform}"/*.a ; do
      verify-archive-${platform} "$i"
    done
  done
}

#@ verify-triple-build <arch> - Verify that the sandboxed translator produces
#@                              an identical translation of itself (llc.pexe)
#@                              as the unsandboxed translator.
verify-triple-build() {
  if [ $# -eq 0 ]; then
    local arch
    for arch in ${SBTC_BUILD_WITH_PNACL} ; do
      verify-triple-build ${arch}
    done
    return
  fi

  local arch=${1/-/}  # Get rid of dashes
  local mode=srpc

  check-sb-arch ${arch}
  check-sb-mode ${mode}

  StepBanner "VERIFY" "Verifying triple build for ${arch}"

  local archdir="${INSTALL_SB_TOOLS}/${arch}/${mode}"
  local archllc="${archdir}/bin/llc"
  local pexe

  if ${SBTC_PRODUCTION} ; then
    pexe="${archdir}/bin/llc.pexe"
  else
    pexe="${INSTALL_SB_TOOLS}/universal/${mode}/bin/llc.pexe"
  fi
  assert-file "${archllc}" "sandboxed llc for ${arch} does not exist"
  assert-file "${pexe}"    "llc.pexe does not exist"

  local flags="--pnacl-sb --pnacl-driver-verbose"
  if [ ${mode} == "srpc" ] ; then
    flags+=" --pnacl-driver-set-SRPC=1"
  else
    flags+=" --pnacl-driver-set-SRPC=0"
  fi

  if [ ${arch} == "arm" ] ; then
    # Use emulator if we are not on ARM
    local hostarch=$(uname -m)
    if ! [[ "${hostarch}" =~ arm ]]; then
      flags+=" --pnacl-use-emulator"
    fi
  fi

  local objdir="${TC_BUILD}/triple-build"
  local newllc="${objdir}/llc.${arch}.rebuild.nexe"
  mkdir -p "${objdir}"

  StepBanner "VERIFY" "Translating llc.pexe to ${arch} using sandboxed tools"
  RunWithLog "verify.triple.build" \
    "${PNACL_TRANSLATE}" ${flags} -arch ${arch} "${pexe}" -o "${newllc}"

  if ! cmp --silent "${archllc}" "${newllc}" ; then
    Banner "TRIPLE BUILD VERIFY FAILED"
    echo "Expected these files to be identical, but they are not:"
    echo "  ${archllc}"
    echo "  ${newllc}"
    exit -1
  fi
  StepBanner "VERIFY" "Verified ${arch} OK"
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
  echo "UTMAN_BUILDBOT:    ${UTMAN_BUILDBOT}"
  echo "UTMAN_CONCURRENCY: ${UTMAN_CONCURRENCY}"
  echo "UTMAN_DEBUG:       ${UTMAN_DEBUG}"
  echo "UTMAN_PRUNE:       ${UTMAN_PRUNE}"
  echo "UTMAN_VERBOSE:     ${UTMAN_VERBOSE}"
  echo "LIBMODE:           ${LIBMODE}"
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
  if ! ${UTMAN_BUILD_ARM} ; then
    return
  fi

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
    if ${UTMAN_BUILDBOT} ; then
      echo "Building on bots --> need ARM trusted toolchain to run tests!"
      exit -1
    elif trusted-tc-confirm ; then
      echo "Continuing without ARM trusted toolchain"
      UTMAN_BUILD_ARM=false
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
  if ${UTMAN_DEBUG} || ${UTMAN_BUILDBOT}; then
    "$@"
  fi
}

######################################################################
# Generate chromium perf bot logs for tracking the size of
# translator binaries.

track-translator-size() {
  local platforms="$@"
  for platform in ${platforms}; do
    print-size-of-sb-tool ${platform} llc
    print-size-of-sb-tool ${platform} ld
  done
}

print-size-of-sb-tool() {
  local platform=$1
  local tool=$2
  local bin_dir="${INSTALL_SB_TOOLS}/${platform}/srpc/bin"
  local tool_size_string=$(${PNACL_SIZE} -B "${bin_dir}/${tool}" | \
    grep '[0-9]\+')
  set -- ${tool_size_string}
  echo "RESULT ${tool}_${platform}_size: text= $1 bytes"
  echo "RESULT ${tool}_${platform}_size: data= $2 bytes"
  echo "RESULT ${tool}_${platform}_size: bss= $3 bytes"
  echo "RESULT ${tool}_${platform}_size: total= $4 bytes"
}

######################################################################
######################################################################
#
#                           < TIME STAMPING >
#
######################################################################
######################################################################

ts-dir-changed() {
  local tsfile="$1"
  local dir="$2"

  if [ -f "${tsfile}" ]; then
    local MODIFIED=$(find "${dir}" -type f -newer "${tsfile}")
    [ ${#MODIFIED} -gt 0 ]
    ret=$?
  else
    true
    ret=$?
  fi
  return $ret
}

# Check if the source for a given build has been modified
ts-modified() {
  local srcdir="$1"
  local objdir="$2"
  local tsfile="${objdir}/${TIMESTAMP_FILENAME}"

  ts-dir-changed "${tsfile}" "${srcdir}"
  return $?
}

ts-touch() {
  local tsfile="$1"
  touch "${tsfile}"
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


# Don't define any functions after this or they won't show up in completions
function-completions() {
  if [ $# = 0 ]; then set -- ""; fi
  compgen -A function -- $1
  exit 0
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

# Setup the initial frontend configuration
reset-frontend

if [ $# = 0 ]; then set -- help; fi  # Avoid reference to undefined $1.

# Accept one -- argument for some compatibility with google3
if [ $1 = "--tab_completion_word" ]; then
  set -- function-completions $2
fi

if [ "$(type -t $1)" != "function" ]; then
  #Usage
  echo "ERROR: unknown function '$1'." >&2
  echo "For help, try:"
  echo "    $0 help"
  exit 1
fi

"$@"
