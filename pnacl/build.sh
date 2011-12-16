#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#@                 Untrusted Toolchain Manager
#@-------------------------------------------------------------------
#@ This script builds the ARM and PNaCl untrusted toolchains.
#@ It MUST be run from the native_client/ directory.
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

set -o nounset
set -o errexit

PWD_ON_ENTRY="$(pwd)"

# The script is located in "native_client/pnacl/".
# Set pwd to pnacl/
cd "$(dirname "$0")"
if [[ $(basename "$(pwd)") != "pnacl" ]] ; then
  echo "ERROR: cannot find pnacl/ directory"
  exit -1
fi

source scripts/common-tools.sh

readonly PNACL_ROOT="$(pwd)"
readonly NACL_ROOT="$(GetAbsolutePath ..)"
readonly SCONS_OUT="${NACL_ROOT}/scons-out"

SetScriptPath "${PNACL_ROOT}/build.sh"
SetLogDirectory "${PNACL_ROOT}/build/log"

# For different levels of make parallelism change this in your env
readonly PNACL_CONCURRENCY=${PNACL_CONCURRENCY:-8}
readonly PNACL_MERGE_TESTING=${PNACL_MERGE_TESTING:-false}
PNACL_PRUNE=${PNACL_PRUNE:-false}
PNACL_BUILD_ARM=true

if ${BUILD_PLATFORM_MAC} || ${BUILD_PLATFORM_WIN}; then
  # We don't yet support building ARM tools for mac or windows.
  PNACL_BUILD_ARM=false
fi

# Set the library mode
readonly LIBMODE=${LIBMODE:-newlib}

LIBMODE_NEWLIB=false
LIBMODE_GLIBC=false
if [ ${LIBMODE} == "newlib" ]; then
  LIBMODE_NEWLIB=true
elif [ ${LIBMODE} == "glibc" ]; then
  LIBMODE_GLIBC=true
  PNACL_BUILD_ARM=false
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

readonly DRIVER_DIR="${PNACL_ROOT}/driver"
readonly ARM_ARCH=armv7-a
readonly ARM_FPU=vfp

readonly TOOLCHAIN_ROOT="${NACL_ROOT}/toolchain"

readonly NNACL_BASE="${TOOLCHAIN_ROOT}/${SCONS_BUILD_PLATFORM}_x86"
readonly NNACL_NEWLIB_ROOT="${NNACL_BASE}_newlib"
readonly NNACL_GLIBC_ROOT="${NNACL_BASE}"

readonly MAKE_OPTS="-j${PNACL_CONCURRENCY} VERBOSE=1"

readonly NONEXISTENT_PATH="/going/down/the/longest/road/to/nowhere"

# For speculative build status output. ( see status function )
# Leave this blank, it will be filled during processing.
SPECULATIVE_REBUILD_SET=""

readonly PNACL_SUPPORT="${PNACL_ROOT}/support"

readonly THIRD_PARTY="${NACL_ROOT}"/../third_party

readonly GMP_VER=gmp-5.0.2
readonly THIRD_PARTY_GMP="${THIRD_PARTY}/gmp/${GMP_VER}.tar.bz2"

readonly MPFR_VER=mpfr-3.0.1
readonly THIRD_PARTY_MPFR="${THIRD_PARTY}/mpfr/${MPFR_VER}.tar.bz2"

readonly MPC_VER=mpc-0.9
readonly THIRD_PARTY_MPC="${THIRD_PARTY}/mpc/${MPC_VER}.tar.gz"

# The location of Mercurial sources (absolute)
readonly TC_SRC="${PNACL_ROOT}/src"
readonly TC_SRC_UPSTREAM="${TC_SRC}/upstream"
readonly TC_SRC_LLVM="${TC_SRC_UPSTREAM}/llvm"
readonly TC_SRC_BINUTILS="${TC_SRC}/binutils"
readonly TC_SRC_NEWLIB="${TC_SRC}/newlib"
readonly TC_SRC_COMPILER_RT="${TC_SRC}/compiler-rt"

# LLVM sources (svn)
readonly TC_SRC_LLVM_MASTER="${TC_SRC}/llvm-master"
readonly TC_SRC_CLANG="${TC_SRC}/clang"
readonly TC_SRC_DRAGONEGG="${TC_SRC}/dragonegg"

# Git sources
readonly PNACL_GIT_ROOT="${PNACL_ROOT}/git"
readonly TC_SRC_GCC="${PNACL_GIT_ROOT}/gcc"
readonly TC_SRC_GLIBC="${PNACL_GIT_ROOT}/glibc"
readonly TC_SRC_GMP="${PNACL_ROOT}/third_party/gmp"
readonly TC_SRC_MPFR="${PNACL_ROOT}/third_party/mpfr"
readonly TC_SRC_MPC="${PNACL_ROOT}/third_party/mpc"
readonly TC_SRC_LIBSTDCPP="${TC_SRC_GCC}/libstdc++-v3"

# Unfortunately, binutils/configure generates this untracked file
# in the binutils source directory
readonly BINUTILS_MESS="${TC_SRC_BINUTILS}/binutils-2.20/opcodes/i386-tbl.h"

readonly SERVICE_RUNTIME_SRC="${NACL_ROOT}/src/trusted/service_runtime"
readonly EXPORT_HEADER_SCRIPT="${SERVICE_RUNTIME_SRC}/export_header.py"
readonly NACL_SYS_HEADERS="${SERVICE_RUNTIME_SRC}/include"
readonly NACL_HEADERS_TS="${TC_SRC}/nacl.sys.timestamp"
readonly NEWLIB_INCLUDE_DIR="${TC_SRC_NEWLIB}/newlib-trunk/newlib/libc/include"

# The location of each project. These should be absolute paths.
# If a tool below depends on a certain libc, then the build
# directory should have ${LIBMODE} in it to distinguish them.
readonly TC_BUILD="${PNACL_ROOT}/build/${LIBMODE}"
readonly TC_BUILD_LLVM="${TC_BUILD}/llvm"
readonly TC_BUILD_BINUTILS="${TC_BUILD}/binutils"
readonly TC_BUILD_BINUTILS_LIBERTY="${TC_BUILD}/binutils-liberty"
readonly TC_BUILD_NEWLIB="${TC_BUILD}/newlib"
readonly TC_BUILD_LIBSTDCPP="${TC_BUILD}/libstdcpp"
readonly TC_BUILD_COMPILER_RT="${TC_BUILD}/compiler_rt"
readonly TC_BUILD_GCC="${TC_BUILD}/gcc"
readonly TC_BUILD_GMP="${TC_BUILD}/gmp"
readonly TC_BUILD_MPFR="${TC_BUILD}/mpfr"
readonly TC_BUILD_MPC="${TC_BUILD}/mpc"
readonly TC_BUILD_DRAGONEGG="${TC_BUILD}/dragonegg"

readonly TIMESTAMP_FILENAME="make-timestamp"

# PNaCl toolchain installation directories (absolute paths)
readonly TOOLCHAIN_LABEL="pnacl_${BUILD_PLATFORM}_${HOST_ARCH}_${LIBMODE}"
readonly INSTALL_ROOT="${TOOLCHAIN_ROOT}/${TOOLCHAIN_LABEL}"
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

# Component installation directories
readonly INSTALL_PKG="${INSTALL_ROOT}/pkg"
readonly NEWLIB_INSTALL_DIR="${INSTALL_PKG}/newlib"
readonly GLIBC_INSTALL_DIR="${INSTALL_PKG}/glibc"
readonly LLVM_INSTALL_DIR="${INSTALL_PKG}/llvm"
readonly GCC_INSTALL_DIR="${INSTALL_PKG}/gcc"
readonly GMP_INSTALL_DIR="${INSTALL_PKG}/gmp"
readonly MPFR_INSTALL_DIR="${INSTALL_PKG}/mpfr"
readonly MPC_INSTALL_DIR="${INSTALL_PKG}/mpc"
readonly LIBSTDCPP_INSTALL_DIR="${INSTALL_PKG}/libstdcpp"
readonly BINUTILS_INSTALL_DIR="${INSTALL_PKG}/binutils"
readonly BFD_PLUGIN_DIR="${BINUTILS_INSTALL_DIR}/lib/bfd-plugins"
readonly SYSROOT_DIR="${INSTALL_ROOT}/sysroot"
readonly LDSCRIPTS_DIR="${INSTALL_ROOT}/ldscripts"
readonly FAKE_INSTALL_DIR="${INSTALL_PKG}/fake"

# Location of PNaCl gcc/g++/as
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

# Set the default frontend.
# Can be default, clang, or dragonegg
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
  if ${PNACL_IN_CROS_CHROOT}; then
    SBTC_BUILD_WITH_PNACL="arm"
  fi
else
  SBTC_BUILD_WITH_PNACL="x8632 x8664"
fi

# Current milestones in each repo
readonly UPSTREAM_REV=${UPSTREAM_REV:-f01c8acb4ea4}

readonly NEWLIB_REV=c6358617f3fd
readonly BINUTILS_REV=fd474ae59e3d
readonly COMPILER_RT_REV=1a3a6ffb31ea

readonly LLVM_PROJECT_REV=${LLVM_PROJECT_REV:-146722}
readonly LLVM_MASTER_REV=${LLVM_PROJECT_REV}
readonly CLANG_REV=${LLVM_PROJECT_REV}
readonly DRAGONEGG_REV=${LLVM_PROJECT_REV}

# Repositories
readonly REPO_UPSTREAM="nacl-llvm-branches.upstream"
readonly REPO_NEWLIB="nacl-llvm-branches.newlib"
readonly REPO_BINUTILS="nacl-llvm-branches.binutils"
readonly REPO_COMPILER_RT="nacl-llvm-branches.compiler-rt"

# LLVM repos (svn)
readonly REPO_LLVM_MASTER="http://llvm.org/svn/llvm-project/llvm/trunk"
readonly REPO_CLANG="http://llvm.org/svn/llvm-project/cfe/trunk"
readonly REPO_DRAGONEGG="http://llvm.org/svn/llvm-project/dragonegg/trunk"

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
  CC="${PNACL_ROOT}/scripts/mygcc32"
  CXX="${PNACL_ROOT}/scripts/myg++32"
fi

select-frontend() {
  local frontend="$1"
  if [ "${frontend}" == default ] ; then
    frontend="${DEFAULT_FRONTEND}"
  fi
  case "${frontend}" in
    clang)
      PNACL_CC="${PNACL_CLANG}"
      PNACL_CXX="${PNACL_CLANGXX}"
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
}

update-all() {
  hg-update-upstream
  svn-update-clang
  svn-update-dragonegg
  hg-update-newlib
  hg-update-binutils
  hg-update-compiler-rt
}

# TODO(pdox): Remove after completely moved to new git pnacl repository
# Move hg/ --> pnacl/src
hg-migrate() {
  if ! [ -d "${NACL_ROOT}"/hg ] ; then
    # Nothing to do
    return 0
  fi
  if [ -d "${TC_SRC}" ] ; then
    Fatal "Error: Trying to move hg/ to pnacl/src, but destination" \
          "directory already exists. Please move manually."
  fi
  if ! ${PNACL_BUILDBOT} ; then
    Banner "Migration needed: Repository paths have changed. This step will:" \
           "   1) Move hg/ to pnacl/src/" \
           "   2) Wipe the old build directories (toolchain/hg-build-*)" \
           "   3) Wipe the old log directories (toolchain/log)" \
           "These have moved to pnacl/build and pnacl/build/log respectively." \
           "If you wish to stop and do the move manually, type N."
    if ! confirm-yes "Proceed" ; then
      Fatal "Aborted"
      exit -1
    fi
  fi

  mv "${NACL_ROOT}"/hg "${TC_SRC}"
  rm -rf "${TOOLCHAIN_ROOT}"/hg-build-*
  rm -rf "${TOOLCHAIN_ROOT}"/hg-log
}

crostarball() {
  local hgdir=$1
  local reporev=$2
  local naclrev_=$3
  local tardir_=$4

  StepBanner archive file list ${hgdir} ${reporev}
  archive="${tardir}/pnacl-src-${naclrev_}-${hgdir}.tbz2"
  hg archive -R "${TC_SRC}/${hgdir}" -t tbz2 -p "${hgdir}" "${archive}"
}

cros-tarball-all() {
  local tardir="${1:-$(pwd)/toolchain/pnacl-src-tarballs}"
  local naclrev=0
  if [ -d .git ]; then
    naclrev=$(git log | grep git-svn | \
      head -1 | cut -d@ -f2 | cut -d\  -f1)
  elif [ -d .svn ]; then
    naclrev=$(svn info | grep Revision: | cut -d\  -f2)
  fi

  StepBanner "Src Tarballs for CrOS (NaCl ${naclrev}) to $(pwd)/${tardir}"

  readonly manifestprefix="cros-manifest-${naclrev}"
  # Keep things small..
  # Skip dragonegg and google-perftools

  rm -rf "${tardir}"
  mkdir -p "${tardir}"

  # TODO(jasonwkim): Keep this list updated
  tar -C ${NACL_ROOT} -cvjf "${tardir}/pnacl-src-${naclrev}-build.tar.bz2" \
    pnacl/build.sh \
    pnacl/test.sh \
    pnacl/DEPS \
    pnacl/driver/ \
    pnacl/scripts/ \
    pnacl/support/ \
    &

  crostarball upstream ${UPSTREAM_REV} ${naclrev} ${tardir} &
  crostarball binutils ${BINUTILS_REV} ${naclrev} ${tardir} &
  crostarball newlib ${NEWLIB_REV} ${naclrev} ${tardir} &
  crostarball compiler-rt ${COMPILER_RT_REV} ${naclrev} ${tardir} &

  StepBanner generating archive for clang at ${CLANG_REV}

  svn export -q -r ${CLANG_REV} ${TC_SRC}/clang "${tardir}/clang"
  tar -C ${tardir} -cjf "${tardir}/pnacl-src-${naclrev}-clang.tar.bz2" clang &

  wait
  rm -rf "${tardir}/clang"
  StepBanner Generated the following:
  ls -l ${tardir}
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
    " Repository '${name}' needs to be updated to the stable " \
    " revision but has local modifications.                  " \
    "                                                        " \
    " If your repository is behind stable, update it using:  " \
    "                                                        " \
    "   cd pnacl/src/${name}; hg update ${rev}               " \
    "   (you may need to resolve conflicts)                  " \
    "                                                        " \
    " If your repository is ahead of stable, then modify:    " \
    "   ${defstr}_REV   (in pnacl/build.sh)                  " \
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
    " Repository '${name}' needs to be updated to the stable " \
    " revision but has local modifications.                  " \
    "                                                        " \
    " If your repository is behind stable, update it using:  " \
    "                                                        " \
    "   cd pnacl/src/${name}; svn update -r ${rev}           " \
    "   (you may need to resolve conflicts)                  " \
    "                                                        " \
    " If your repository is ahead of stable, then modify:    " \
    "   ${defstr}_REV   (in pnacl/build.sh)                  " \
    " to suppress this error message.                        "
  exit -1
}

hg-bot-sanity() {
  local name="$1"
  local dir="$2"

  if ! ${PNACL_BUILDBOT} ; then
    return 0
  fi

  if ! hg-on-branch "${dir}" pnacl-sfi ||
     hg-has-changes "${dir}" ||
     hg-has-untracked "${dir}" ; then
    Banner "WARNING: ${name} repository is in an illegal state." \
           "         Wiping and trying again."
    rm -rf "${dir}"
    hg-checkout-${name}
  fi
}

svn-bot-sanity() {
  local name="$1"
  local dir="$2"

  if ! ${PNACL_BUILDBOT} ; then
    return 0
  fi

  if svn-has-changes "${dir}" ; then
    Banner "WARNING: ${name} repository is in an illegal state." \
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
  if ! ${PNACL_MERGE_TESTING} ; then
    hg-update-common "upstream" ${UPSTREAM_REV} "${TC_SRC_UPSTREAM}"
  fi
  llvm-link-clang
}

svn-update-llvm-master() {
  svn-update-common "llvm-master" ${LLVM_MASTER_REV} "${TC_SRC_LLVM_MASTER}"
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

#@ hg-pull-all           - Pull all repos. (but do not update working copy)
#@ hg-pull-REPO          - Pull repository REPO.
#@                         (REPO can be llvm, newlib, binutils)
hg-pull-all() {
  StepBanner "HG-PULL" "Running 'hg pull' in all repos..."
  hg-pull-upstream
  hg-pull-newlib
  hg-pull-binutils
  hg-pull-compiler-rt
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
  if ${PNACL_IN_CROS_CHROOT}; then
    git-sync-no-gclient
  else
    git-sync
  fi
}

hg-checkout-upstream() {
  if ! ${PNACL_MERGE_TESTING} ; then
    hg-checkout ${REPO_UPSTREAM} "${TC_SRC_UPSTREAM}" ${UPSTREAM_REV}
  fi
  llvm-link-clang
}

svn-checkout-llvm-master() {
  svn-checkout "${REPO_LLVM_MASTER}" "${TC_SRC_LLVM_MASTER}" ${LLVM_MASTER_REV}
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

git-grab() {
  local baseurl="http://git.chromium.org/native_client/"
  local srcdir=$1
  local repo=$2
  local label=$3
  local rev=$(cat "${PNACL_ROOT}"/DEPS | tr -d '",' |
              grep ${label}: | awk '{print $2}')
  StepBanner "GIT-SYNC" "Checking out ${repo} at ${rev}"
  if ! [ -d "${srcdir}" ]; then
    mkdir -p "${PNACL_GIT_ROOT}"
    git clone ${baseurl}/${repo} "${srcdir}"
  fi
  spushd "${srcdir}"
  git fetch
  git reset --hard "${rev}"
  spopd
}

# Used for grabbing git repositories in CrOS sandbox
git-sync-no-gclient() {
  git-grab "${TC_SRC_GCC}" pnacl-gcc.git pnacl_gcc_rev
  git-grab "${TC_SRC_GLIBC}" nacl-glibc.git glibc_rev
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
    # NOTE: glibc steals libc, libstdc++ from the other toolchain
    glibc
  fi
}

#@ libs            - install native libs and build bitcode libs
libs() {
  libs-clean
  libs-support
  libc
  compiler-rt-all
  libgcc_eh-all
  if ${LIBMODE_NEWLIB}; then
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
  if ${PNACL_IN_CROS_CHROOT}; then
    # TODO: http://code.google.com/p/nativeclient/issues/detail?id=135
    Banner "You are running in a ChromiumOS Chroot."
  fi

  checkout-all
  StepBanner "Updating upstreaming repository"
  update-all
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
  if ${PNACL_PRUNE}; then
    prune
  fi
  sdk
  install-translators srpc
  if ${PNACL_PRUNE}; then
    prune-translator-install srpc
    if ${LIBMODE_NEWLIB}; then
      track-translator-size ${SBTC_BUILD_WITH_PNACL}
    fi
  fi
}

# Builds crt1.bc for GlibC, which is just sysdeps/nacl/start.c and csu/init.c
glibc-crt1() {
  StepBanner "GLIBC" "Building crt1.bc"
  local tmpdir="${TC_BUILD}/glibc-crt1"
  local flags="-no-save-temps -DUSE_IN_LIBIO -I${TC_SRC_GLIBC}/sysdeps/gnu"

  rm -rf "${tmpdir}"
  mkdir -p "${tmpdir}"
  spushd "${tmpdir}"
  ${PNACL_CC} ${flags} -c "${TC_SRC_GLIBC}"/sysdeps/nacl/start.c -o start.bc
  ${PNACL_CC} ${flags} -c "${TC_SRC_GLIBC}"/csu/init.c -o init.bc
  ${PNACL_LD} -r -nostdlib -no-save-temps start.bc init.bc -o crt1.bc
  mkdir -p "${INSTALL_LIB}"
  cp crt1.bc "${INSTALL_LIB}"
  spopd
}

glibc() {
  glibc-copy
  glibc-crt1
}

glibc-copy() {
  StepBanner "GLIBC" "Copying glibc from NNaCl toolchain"

  mkdir -p "${INSTALL_LIB_X8632}"
  mkdir -p "${INSTALL_LIB_X8664}"
  mkdir -p "${GLIBC_INSTALL_DIR}"

  # Files in: ${NACL64_TARGET}/lib[32]/
  local LIBS_TO_COPY="libstdc++.a libstdc++.so* \
                      libc.a libc_nonshared.a \
                      libc-2.9.so libc.so libc.so.* \
                      libm-2.9.so libm.a libm.so libm.so.* \
                      libdl-2.9.so libdl.so.* libdl.so libdl.a \
                      libpthread-2.9.so libpthread.a libpthread.so \
                      libpthread.so.* libpthread_nonshared.a \
                      runnable-ld.so \
                      ld-2.9.so"

  for lib in ${LIBS_TO_COPY} ; do
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
  install-unwind-header
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
  local backup_dir="${PNACL_ROOT}/build/llvm-${LIBMODE}-backup"

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
  local messtmp="${TC_SRC}/binutils.tmp"
  if [ -f "${BINUTILS_MESS}" ] ; then
    mv "${BINUTILS_MESS}" "${messtmp}"
  fi
}

binutils-mess-unhide() {
  local messtmp="${TC_SRC}/binutils.tmp"
  if [ -f "${messtmp}" ] ; then
    mv "${messtmp}" "${BINUTILS_MESS}"
  fi
}

#+ clean-scons           - Clean scons-out directory
clean-scons() {
  rm -rf "${SCONS_OUT}"
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
  local tgzname=$(cd "${PWD_ON_ENTRY}"; GetAbsolutePath "$1")
  clean
  everything-translator

  # Remove the SDK so it doesn't end up in the tarball
  sdk-clean

  if ${PNACL_PRUNE}; then
    prune
  fi
  tarball "${tgzname}"
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

  echo "stripping binaries (binutils)"
  strip "${BINUTILS_INSTALL_DIR}"/bin/*

  echo "stripping binaries (llvm)"
  if ! strip "${LLVM_INSTALL_DIR}"/bin/* ; then
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
  # Symbolic link named : ${TC_SRC}/upstream/llvm/tools/clang
  # Needs to point to   : ${TC_SRC}/clang
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

libgcc_eh-all() {
  StepBanner "LIBGCC_EH (from GCC 4.6)"
  if ! ${LIBMODE_GLIBC}; then
    libgcc_eh arm
  fi
  libgcc_eh x86-32
  libgcc_eh x86-64
}

libgcc_eh() {
  local arch=$1
  local objdir="${TC_BUILD}/libgcc_eh-${arch}"
  local subdir="${objdir}/fake-target/libgcc"
  local installdir="${INSTALL_LIB}-${arch}"
  mkdir -p "${installdir}"
  rm -rf "${installdir}"/libgcc_s*
  rm -rf "${installdir}"/libgcc_eh*
  rm -rf "${objdir}"

  # Setup fake gcc build directory.
  mkdir -p "${objdir}"/gcc
  cp -a "${PNACL_ROOT}"/scripts/libgcc-${LIBMODE}.mvars \
        "${objdir}"/gcc/libgcc.mvars
  cp -a "${PNACL_ROOT}"/scripts/libgcc-tconfig.h "${objdir}"/gcc/tconfig.h
  touch "${objdir}"/gcc/tm.h

  install-unwind-header

  mkdir -p "${subdir}"
  spushd "${subdir}"
  local flags="-arch ${arch} --pnacl-bias=${arch} --pnacl-allow-translate"
  flags+=" --pnacl-allow-native"
  if ${LIBMODE_GLIBC}; then
    # Enable thread safety using pthreads
    flags+=" -D_PTHREADS -D_GNU_SOURCE"
  fi
  flags+=" -DENABLE_RUNTIME_CHECKING"
  StepBanner "LIBGCC_EH" "Configure ${arch}"
  RunWithLog libgcc.${arch}.configure \
    env -i \
      PATH="/usr/bin:/bin" \
      CC="${PNACL_CC} ${flags}" \
      CXX="${PNACL_CXX} ${flags}" \
      AR="${PNACL_AR}" \
      NM="${PNACL_NM}" \
      RANLIB="${PNACL_RANLIB}" \
      /bin/sh \
      "${TC_SRC_GCC}"/libgcc/configure \
        --prefix="${FAKE_INSTALL_DIR}" \
        --enable-shared \
        --host=i686-nacl
        # --enable-shared is needed because the EH functions end
        # up in libgcc.a if shared libs are disabled.
        #
        # The "--host" field is needed to enable cross-compiling.
        # It introduces extra configuration files in libgcc/config, but these
        # appear to affect libgcc, not libgcc_eh.
        # TODO(pdox): Create a fake target to get rid of i686 here.

  StepBanner "LIBGCC_EH" "Make ${arch}"
  RunWithLog libgcc.${arch}.make \
    make libgcc_eh.a
  if ${LIBMODE_GLIBC}; then
    RunWithLog libgcc_s.${arch}.make \
      make disable_libgcc_base=yes libgcc_s.so
  fi
  spopd

  StepBanner "LIBGCC_EH" "Install ${arch}"
  cp ${subdir}/libgcc_eh.a "${installdir}"
  if ${LIBMODE_GLIBC}; then
    cp ${subdir}/libgcc_s.so.1 "${installdir}"
    ln -s libgcc_s.so.1 "${installdir}"/libgcc_s.so
  fi
}

#+ sysroot               - setup initial sysroot
sysroot() {
  StepBanner "SYSROOT" "Setting up initial sysroot"

  local sys_include="${SYSROOT_DIR}/include"
  local sys_include2="${SYSROOT_DIR}/sys-include"

  rm -rf "${sys_include}" "${sys_include2}"
  mkdir -p "${sys_include}"
  ln -sf "${sys_include}" "${sys_include2}"
  cp -r "${NEWLIB_INCLUDE_DIR}"/* "${sys_include}"
}

install-unwind-header() {
  # TODO(pdox):
  # We need to establish an unwind ABI, since this is part of the ABI
  # exposed to the bitcode by the translator. This header should not vary
  # across compilers or C libraries.
  INSTALL="/usr/bin/install -c -m 644"
  ${INSTALL} ${TC_SRC_GCC}/gcc/unwind-generic.h \
             ${LLVM_INSTALL_DIR}/lib/clang/3.1/include/unwind.h
}

#########################################################################
#########################################################################
#     < COMPILER-RT >
#########################################################################
#########################################################################

compiler-rt-all() {
  StepBanner "COMPILER-RT (LIBGCC)"
  if ! ${LIBMODE_GLIBC}; then
    compiler-rt arm
  fi
  compiler-rt x86-32
  compiler-rt x86-64
}


#+ compiler-rt           - build/install llvm's replacement for libgcc.a
compiler-rt() {
  local arch=$1
  local src="${TC_SRC_COMPILER_RT}/compiler-rt/lib"
  local objdir="${TC_BUILD_COMPILER_RT}-${arch}"
  local installdir="${INSTALL_LIB}-${arch}"
  StepBanner "compiler rt" "build (${arch})"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"

  spushd "${objdir}"
  RunWithLog libgcc.${arch}.make \
      make -j ${PNACL_CONCURRENCY} -f ${src}/Makefile-pnacl libgcc.a \
        CC="${PNACL_CC}" \
        AR="${PNACL_AR}" \
        "SRC_DIR=${src}" \
        "CFLAGS=-arch ${arch} --pnacl-allow-translate -O3 -fPIC"

  StepBanner "compiler rt" "install (${arch})"
  mkdir -p "${installdir}"
  cp libgcc.a "${installdir}"
  spopd
}

#########################################################################
#########################################################################
#                          < LIBSTDCPP >
#########################################################################
#########################################################################

libstdcpp() {
  StepBanner "LIBSTDCPP 4.6 (BITCODE)"

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
}

#+ libstdcpp-clean - clean libstdcpp in bitcode
libstdcpp-clean() {
  StepBanner "LIBSTDCPP" "Clean"
  rm -rf "${TC_BUILD_LIBSTDCPP}"
}

libstdcpp-needs-configure() {
  ts-newer-than "${TC_BUILD_LLVM}" \
                "${TC_BUILD_LIBSTDCPP}" && return 0
  [ ! -f "${TC_BUILD_LIBSTDCPP}/config.status" ]
  return #?
}

libstdcpp-configure() {
  StepBanner "LIBSTDCPP" "Configure"
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="${TC_BUILD_LIBSTDCPP}"
  local subdir="${TC_BUILD_LIBSTDCPP}/pnacl-target"

  mkdir -p "${subdir}"
  spushd "${subdir}"

  local flags=""
  if ${LIBMODE_NEWLIB}; then
    flags+="--with-newlib --disable-shared --disable-rpath"
  elif ${LIBMODE_GLIBC}; then
    # TODO(pdox): Fix right away.
    flags+="--disable-shared"
  else
    Fatal "Unknown library mode"
  fi

  setup-libstdcpp-env
  RunWithLog libstdcpp.configure \
      env -i PATH=/usr/bin/:/bin \
        "${STD_ENV_FOR_LIBSTDCPP[@]}" \
        "${srcdir}"/configure \
          --host="${CROSS_TARGET_ARM}" \
          --prefix="${LIBSTDCPP_INSTALL_DIR}" \
          --enable-cxx-flags="-D__SIZE_MAX__=4294967295" \
          --disable-multilib \
          --disable-linux-futex \
          --disable-libstdcxx-time \
          --disable-sjlj-exceptions \
          --disable-libstdcxx-pch \
          ${flags}
  spopd
}

libstdcpp-needs-make() {
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="${TC_BUILD_LIBSTDCPP}"

  ts-modified "${srcdir}" "${objdir}"
  return $?
}

libstdcpp-make() {
  StepBanner "LIBSTDCPP" "Make"
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="${TC_BUILD_LIBSTDCPP}"

  ts-touch-open "${objdir}"

  spushd "${objdir}/pnacl-target"
  setup-libstdcpp-env
  RunWithLog libstdcpp.make \
    env -i PATH=/usr/bin/:/bin \
        make \
        "${STD_ENV_FOR_LIBSTDCPP[@]}" \
        ${MAKE_OPTS}
  spopd

  ts-touch-commit "${objdir}"
}

libstdcpp-install() {
  StepBanner "LIBSTDCPP" "Install"
  local objdir="${TC_BUILD_LIBSTDCPP}"

  spushd "${objdir}/pnacl-target"

  # install headers (=install-data)
  # for good measure make sure we do not keep any old headers
  rm -rf "${INSTALL_ROOT}/include/c++"
  rm -rf "${LIBSTDCPP_INSTALL_DIR}"
  setup-libstdcpp-env
  RunWithLog libstdcpp.install \
    make \
    "${STD_ENV_FOR_LIBSTDCPP[@]}" \
    ${MAKE_OPTS} install-data

  # Install bitcode library
  mkdir -p "${INSTALL_LIB}"
  cp "${objdir}/pnacl-target/src/.libs/libstdc++.a" "${INSTALL_LIB}"

  spopd
}

#+ misc-tools            - Build and install sel_ldr and validator for ARM.
misc-tools() {
  if ${PNACL_IN_CROS_CHROOT}; then
    Banner "In CrOS chroot. Not building misc-tools"
    return
  fi


  if ${PNACL_BUILD_ARM} ; then
    StepBanner "MISC-ARM" "Building sel_ldr (ARM)"

    # TODO(robertm): revisit some of these options
    spushd "${NACL_ROOT}"
    RunWithLog arm_sel_ldr \
      ./scons MODE=opt-host \
      platform=arm \
      sdl=none \
      naclsdk_validate=0 \
      sysinfo=0 \
      sel_ldr
    rm -rf  "${INSTALL_ROOT}/tools-arm"
    mkdir "${INSTALL_ROOT}/tools-arm"
    local sconsdir="${SCONS_OUT}/opt-${SCONS_BUILD_PLATFORM}-arm"
    cp "${sconsdir}/obj/src/trusted/service_runtime/sel_ldr" \
       "${INSTALL_ROOT}/tools-arm"
    spopd
  else
    StepBanner "MISC-ARM" "Skipping ARM sel_ldr (No trusted ARM toolchain)"
  fi

  if ${BUILD_PLATFORM_LINUX} ; then
    StepBanner "MISC-ARM" "Building validator (ARM)"
    spushd "${NACL_ROOT}"
    RunWithLog arm_ncval_core \
      ./scons MODE=opt-host \
      targetplatform=arm \
      sysinfo=0 \
      arm-ncval-core
    rm -rf  "${INSTALL_ROOT}/tools-x86"
    mkdir "${INSTALL_ROOT}/tools-x86"
    cp ${SCONS_OUT}/opt-linux-x86-32-to-arm/obj/src/trusted/validator_arm/\
arm-ncval-core ${INSTALL_ROOT}/tools-x86
    spopd
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

  Fatal "ERROR: Unsupported arch [$1]. Must be: x8632, x8664, arm, or universal"
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

  LLVM_SB_CONFIGURE_ENV=(
    AR="${PNACL_AR}" \
    AS="${PNACL_AS}" \
    CC="${PNACL_CC} ${flags}" \
    CXX="${PNACL_CXX} ${flags}" \
    LD="${PNACL_LD} ${flags}" \
    NM="${PNACL_NM}" \
    RANLIB="${PNACL_RANLIB}" \
    LDFLAGS="") # TODO(pdox): Support -s
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
  StepBanner "LLVM-SB" "Sandboxed llc + lli [$*]"
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

  RunWithLog ${LLVM_SB_LOG_PREFIX}.make \
      env -i PATH="/usr/bin:/bin" \
      ONLY_TOOLS="llc lli"\
      NACL_SANDBOX=1 \
      NACL_SRPC=${build_with_srpc} \
      KEEP_SYMBOLS=1 \
      VERBOSE=1 \
      make ${MAKE_OPTS} tools-only

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

  if ${LIBMODE_GLIBC}; then
    # SCons SDK install is currently broken, in that it produces
    # X86-32 .so files instead of .pso files.
    # BUG= http://code.google.com/p/nativeclient/issues/detail?id=2420
    # And translation requires libsrpc, so we cannot correctly translate these.
    # BUG= http://code.google.com/p/nativeclient/issues/detail?id=2451
    return 0
  fi

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
    "${PNACL_TRANSLATE}" -arch ${tarch} -Wl,-L"${INSTALL_SDK_LIB}" \
                         "${pexe}" -o "${nexe}" &
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

#---------------------------------------------------------------------

BINUTILS_SB_SETUP=false
binutils-sb-setup() {
  # TODO(jvoung): investigate if these are only needed by AS or not.
  local flags="-DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5 -DNACL_TOOLCHAIN_PATCH"

  if ${BINUTILS_SB_SETUP} && [ $# -eq 0 ]; then
    return 0
  fi

  if [ $# -ne 1 ] ; then
    Fatal "Please specify arch"
  fi

  BINUTILS_SB_SETUP=true

  BINUTILS_SB_ARCH=$1
  check-sb-arch ${BINUTILS_SB_ARCH}
  BINUTILS_SB_LOG_PREFIX="binutils.sb.${BINUTILS_SB_ARCH}"
  BINUTILS_SB_OBJDIR="${TC_BUILD}/binutils-sb-${BINUTILS_SB_ARCH}"
  BINUTILS_SB_INSTALL_DIR="${INSTALL_SB_TOOLS}/${arch}/srpc"

  if ${LIBMODE_NEWLIB}; then
    flags+=" -static"
  fi

  # we do not support the non-srpc mode anymore
  flags+=" -DNACL_SRPC"

  if [ ! -f "${TC_BUILD_BINUTILS_LIBERTY}/libiberty/libiberty.a" ] ; then
    echo "ERROR: Missing lib. Run this script with binutils-liberty option"
    exit -1
  fi

  # Speed things up by avoiding an intermediate step
  flags+=" --pnacl-skip-ll"

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


  case ${BINUTILS_SB_ARCH} in
    x8632)     BINUTILS_SB_ELF_TARGETS=i686-pc-nacl ;;
    x8664)     BINUTILS_SB_ELF_TARGETS=x86_64-pc-nacl ;;
    arm)       BINUTILS_SB_ELF_TARGETS=arm-pc-nacl ;;
    universal) BINUTILS_SB_ELF_TARGETS=arm-pc-nacl,i686-pc-nacl,x86_64-pc-nacl ;;
  esac


}

#+-------------------------------------------------------------------------
#+ binutils-sb <arch> <mode> - Build and install binutils (sandboxed)
binutils-sb() {
  binutils-sb-setup "$@"
  StepBanner "BINUTILS-SB" "Sandboxed ld [$*]"

  local srcdir="${TC_SRC_BINUTILS}"
  assert-dir "${srcdir}" "You need to checkout binutils."

  if binutils-sb-needs-configure ; then
    binutils-sb-clean
    binutils-sb-configure
  else
    SkipBanner "BINUTILS-SB" "configure ${BINUTILS_SB_ARCH}"
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
  StepBanner "BINUTILS-SB" "Clean"
  local objdir="${BINUTILS_SB_OBJDIR}"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"
}

# binutils-sb-configure - Configure binutils (sandboxed)
binutils-sb-configure() {
  binutils-sb-setup "$@"

  StepBanner "BINUTILS-SB" "Configure"
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${BINUTILS_SB_OBJDIR}"
  local installdir="${BINUTILS_SB_INSTALL_DIR}"

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
        --enable-targets=${BINUTILS_SB_ELF_TARGETS} \
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

  StepBanner "BINUTILS-SB" "Make"
  local objdir="${BINUTILS_SB_OBJDIR}"

  spushd "${objdir}"
  ts-touch-open "${objdir}"

  RunWithLog ${BINUTILS_SB_LOG_PREFIX}.make \
      env -i PATH="/usr/bin:/bin" \
      NACL_SRPC=1 \
      make ${MAKE_OPTS} all-ld

  ts-touch-commit "${objdir}"

  spopd
}

# binutils-sb-install - Install binutils (sandboxed)
binutils-sb-install() {
  binutils-sb-setup "$@"

  StepBanner "BINUTILS-SB" "Install"
  local objdir="${BINUTILS_SB_OBJDIR}"
  spushd "${objdir}"

  RunWithLog ${BINUTILS_SB_LOG_PREFIX}.install \
      env -i PATH="/usr/bin:/bin" \
      make install-ld

  spopd

  # First rename and *strip* the installed file. (Beware for debugging).
  local installdir="${BINUTILS_SB_INSTALL_DIR}"
  ${PNACL_STRIP} "${installdir}/bin/${BINUTILS_TARGET}-ld" \
    -o "${installdir}/bin/ld"
  # Remove old file plus a redundant file.
  rm "${installdir}/bin/${BINUTILS_TARGET}-ld"
  rm "${installdir}/bin/${BINUTILS_TARGET}-ld.bfd"

  # Then translate.
  translate-and-install-sb-tool ${BINUTILS_SB_ARCH} srpc ld
}

#+ tools-sb {arch} {mode} - Build all sandboxed tools for arch, mode
tools-sb() {
  local arch=$1
  local mode=$2

  llvm-sb ${arch} ${mode}
  binutils-sb ${arch}
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

  StepBanner "PRUNE" "Pruning translator installs [${srpc_kind}]"
  for arch in ${SBTC_BUILD_WITH_PNACL} ; do
    StepBanner "PRUNE" "llvm cruft [${arch}]"
    if [ -d "${INSTALL_SB_TOOLS}/${arch}/${srpc_kind}" ]; then
      spushd "${INSTALL_SB_TOOLS}/${arch}/${srpc_kind}"
      rm -rf include lib nacl share
      rm -rf bin/llvm-config bin/tblgen
      spopd
    fi
  done

  if ! ${SBTC_PRODUCTION}; then
    rm -rf "${INSTALL_SB_TOOLS}/universal"
  fi

  StepBanner "PRUNE" "Stripping tools-sb nexes"
  for arch in ${SBTC_BUILD_WITH_PNACL} ; do
    if [ -d "${INSTALL_SB_TOOLS}/${arch}/${srpc_kind}" ]; then
      ${PNACL_STRIP} "${INSTALL_SB_TOOLS}/${arch}/${srpc_kind}"/bin/*
    fi
  done

  StepBanner "PRUNE" "remove driver log"
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
  ts-newer-than "${TC_BUILD_LLVM}" \
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

libs-support() {
  StepBanner "LIBS-SUPPORT"
  if ${LIBMODE_NEWLIB}; then
    libs-support-newlib
  fi
  libs-support-bitcode
  local arch
  for arch in arm x86-32 x86-64; do
    libs-support-native ${arch}
  done
}

libs-support-newlib() {
  mkdir -p "${INSTALL_LIB}"
  spushd "${PNACL_SUPPORT}"
  # Install crt1.x (linker script)
  StepBanner "LIBS-SUPPORT-NEWLIB" "Install crt1.x (linker script)"
  cp crt1.x "${INSTALL_LIB}"/crt1.x
  spopd
}

libs-support-bitcode() {
  local flags="-no-save-temps"
  mkdir -p "${INSTALL_LIB}"

  spushd "${PNACL_SUPPORT}"

  # Install crti.bc (empty _init/_fini)
  StepBanner "LIBS-SUPPORT" "Install crti.bc"
  ${PNACL_CC} ${flags} -c crti.c -o "${INSTALL_LIB}"/crti.bc

  # Install crtbegin bitcode (__dso_handle/__cxa_finalize for C++)
  StepBanner "LIBS-SUPPORT" "Install crtbegin.bc / crtbeginS.bc"
  ${PNACL_CC} ${flags} -c bitcode/crtdummy.c -o "${INSTALL_LIB}"/crtdummy.bc
  ${PNACL_CC} ${flags} -c bitcode/crtbegin.c -o "${INSTALL_LIB}"/crtbegin.bc
  ${PNACL_CC} ${flags} -c bitcode/crtbegin.c -o "${INSTALL_LIB}"/crtbeginS.bc \
                       -DSHARED

  # Install pnacl_abi.bc
  StepBanner "LIBS-SUPPORT" "Install pnacl_abi.bc (stub pso)"
  ${PNACL_CC} ${flags} -Wno-builtin-requires-header -nostdlib -shared \
              -Wl,-soname="" pnacl_abi.c -o "${INSTALL_LIB}"/pnacl_abi.bc
  spopd
}

libs-support-native() {
  local arch=$1
  local destdir="${INSTALL_LIB}"-${arch}
  local label="LIBS-SUPPORT (${arch})"
  mkdir -p "${destdir}"

  local flags="-arch ${arch} --pnacl-allow-native --pnacl-allow-translate \
               -no-save-temps"

  spushd "${PNACL_SUPPORT}"

  # Compile crtbegin.o / crtend.o
  StepBanner "${label}" "Install crtbegin.o / crtend.o"
  ${PNACL_CC} ${flags} -c crtbegin.c -o "${destdir}"/crtbegin.o
  ${PNACL_CC} ${flags} -c crtend.c -o "${destdir}"/crtend.o

  # TODO(pdox): Use this for shared objects when we build libgcc_s.so ourselves
  # Compile crtbeginS.o / crtendS.o
  StepBanner "${label}" "Install crtbeginS.o / crtendS.o"
  ${PNACL_CC} ${flags} -c crtbegin.c -fPIC -DSHARED -o "${destdir}"/crtbeginS.o
  ${PNACL_CC} ${flags} -c crtend.c -fPIC -DSHARED -o "${destdir}"/crtendS.o

  # Make libcrt_platform.a
  StepBanner "${label}" "Install libcrt_platform.a"
  local tmpdir="${TC_BUILD}/libs-support-native"
  rm -rf "${tmpdir}"
  mkdir -p "${tmpdir}"
  ${PNACL_CC} ${flags} -c setjmp_${arch/-/_}.S -o "${tmpdir}"/setjmp.o

  # For ARM, also compile aeabi_read_tp.S
  if  [ ${arch} == arm ] ; then
    ${PNACL_CC} ${flags} -c aeabi_read_tp.S -o "${tmpdir}"/aeabi_read_tp.o
  fi
  spopd

  ${PNACL_AR} rc "${destdir}"/libcrt_platform.a "${tmpdir}"/*.o
}

#########################################################################
#     < SDK >
#########################################################################
SCONS_ARGS=(MODE=nacl
            -j${PNACL_CONCURRENCY}
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
  if ${PNACL_IN_CROS_CHROOT}; then
    Banner "NOT COMPILING THE IRT SHIM on CrOS chroot"
  else
    sdk-irt-shim
  fi
  sdk-verify
}

#+ sdk-clean             - Clean sdk stuff
sdk-clean() {
  StepBanner "SDK" "Clean"
  rm -rf "${INSTALL_SDK_ROOT}"

  # clean scons obj dirs
  rm -rf "${SCONS_OUT}"/nacl-*-pnacl*
}

sdk-headers() {
  mkdir -p "${INSTALL_SDK_INCLUDE}"

  local extra_flags=""
  local neutral_platform="x86-32"
  if ${LIBMODE_GLIBC}; then
    extra_flags="--nacl_glibc"
  fi

  StepBanner "SDK" "Install headers"
  spushd "${NACL_ROOT}"
  RunWithLog "sdk.headers" \
      ./scons \
      "${SCONS_ARGS[@]}" \
      ${extra_flags} \
      platform=${neutral_platform} \
      install_headers \
      includedir="$(PosixToSysPath "${INSTALL_SDK_INCLUDE}")"
  spopd
}

sdk-libs() {
  StepBanner "SDK" "Install libraries"
  mkdir -p "${INSTALL_SDK_LIB}"

  local extra_flags=""
  local neutral_platform="x86-32"
  if ${LIBMODE_GLIBC}; then
    extra_flags="--nacl_glibc"
  fi

  spushd "${NACL_ROOT}"
  RunWithLog "sdk.libs.bitcode" \
      ./scons \
      "${SCONS_ARGS[@]}" \
      ${extra_flags} \
      platform=${neutral_platform} \
      install_lib \
      libdir="$(PosixToSysPath "${INSTALL_SDK_LIB}")"
  spopd
}

sdk-irt-shim() {
  # NOTE: This uses the nacl-gcc toolchain, causing
  #       the pnacl toolchain to depend on it.
  StepBanner "SDK" "IRT Shim"
  spushd "${NACL_ROOT}"
  # NOTE: We specify bitcode=1, but it is really using nacl-gcc to build
  # the library (it's only bitcode=1 because it's part of the pnacl sdk).
  RunWithLog "sdk.irt.shim" \
    ./scons -j${PNACL_CONCURRENCY} \
            bitcode=1 \
            platform=x86-64 \
            naclsdk_validate=0 \
            --verbose \
            pnacl_irt_shim
  local out_dir_prefix="${SCONS_OUT}"/nacl-x86-64-pnacl-clang
  local outdir="${out_dir_prefix}"/obj/src/untrusted/pnacl_irt_shim
  mkdir -p "${INSTALL_LIB_X8664}"
  cp "${outdir}"/libpnacl_irt_shim.a "${INSTALL_LIB_X8664}"
  spopd
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
    echo "*  pnacl/build.sh newlib-nacl-headers-clean                       *"
    echo "*******************************************************************"
    echo ""
    if ${PNACL_BUILDBOT} ; then
      newlib-nacl-headers-clean
    else
      exit -1
    fi
  fi
}

#+-------------------------------------------------------------------------
#@ driver                - Install driver scripts.
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
  cp driver_*.py "${INSTALL_BIN}"
  cp artools.py "${INSTALL_BIN}"
  cp ldtools.py "${INSTALL_BIN}"
  cp shelltools.py "${INSTALL_BIN}"
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
  if [ -d .svn ]; then
    svn info > "${INSTALL_ROOT}/REV"
  elif [ -d .git ]; then
    git log | grep git-svn | head -1 > "${INSTALL_ROOT}/REV"
  fi
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
  if ${LLVM_DIS} "$1" -o - | grep asm ; then
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
  echo "PNACL_BUILDBOT:    ${PNACL_BUILDBOT}"
  echo "PNACL_CONCURRENCY: ${PNACL_CONCURRENCY}"
  echo "PNACL_DEBUG:       ${PNACL_DEBUG}"
  echo "PNACL_PRUNE:       ${PNACL_PRUNE}"
  echo "PNACL_VERBOSE:     ${PNACL_VERBOSE}"
  echo "LIBMODE:           ${LIBMODE}"
  Banner "Your Environment:"
  env | grep PNACL
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
  if [ -f ${TOOLCHAIN_ROOT}/linux_arm-trusted/ld_script_arm_trusted ]; then
    return 0
  else
    return 1
  fi
}

check-for-trusted() {
  if ! ${PNACL_BUILD_ARM} ; then
    return
  fi

  if ${PNACL_IN_CROS_CHROOT}; then
    # TODO: http://code.google.com/p/nativeclient/issues/detail?id=135
    Banner "You are running in a ChromiumOS Chroot." \
      " You should make sure that the PNaCl sources are properly checked out " \
      " And updated outside of the chroot"
    return
  fi

  if ! has-trusted-toolchain; then
    echo '*******************************************************************'
    echo '*   The ARM trusted toolchain does not appear to be installed yet *'
    echo '*   It is needed to run ARM tests.                                *'
    echo '*                                                                 *'
    echo '*   To download and install the trusted toolchain, run:           *'
    echo '*                                                                 *'
    echo '*       $ pnacl/build.sh download-trusted                         *'
    echo '*                                                                 *'
    echo '*   To compile the trusted toolchain, use:                        *'
    echo '*                                                                 *'
    echo '*       $ tools/llvm/trusted-toolchain-creator.sh trusted_sdk     *'
    echo '*               (warning: this takes a while)                     *'
    echo '*******************************************************************'

    # If building on the bots, do not continue since it needs to run ARM tests.
    if ${PNACL_BUILDBOT} ; then
      echo "Building on bots --> need ARM trusted toolchain to run tests!"
      exit -1
    elif trusted-tc-confirm ; then
      echo "Continuing without ARM trusted toolchain"
      PNACL_BUILD_ARM=false
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
  if ${PNACL_DEBUG} || ${PNACL_BUILDBOT}; then
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
select-frontend "${FRONTEND}"

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

hg-migrate

"$@"
