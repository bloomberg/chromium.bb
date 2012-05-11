#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
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
# If true, we are in the middle of a merge.  Do not update the llvm branch
# (except for clang, which has no local mods).
readonly PNACL_MERGE_TESTING=${PNACL_MERGE_TESTING:-false}
PNACL_PRUNE=${PNACL_PRUNE:-false}
PNACL_BUILD_ARM=true

if ${BUILD_PLATFORM_MAC} || ${BUILD_PLATFORM_WIN}; then
  # We don't yet support building ARM tools for mac or windows.
  PNACL_BUILD_ARM=false
fi

readonly SB_JIT=${SB_JIT:-false}

# TODO(pdox): Decide what the target should really permanently be
readonly CROSS_TARGET_ARM=arm-none-linux-gnueabi
readonly BINUTILS_TARGET=arm-pc-nacl
readonly REAL_CROSS_TARGET=le32-unknown-nacl
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

# The location of Mercurial sources (absolute)
readonly TC_SRC="${PNACL_ROOT}/src"
readonly TC_SRC_UPSTREAM="${TC_SRC}/upstream"
readonly TC_SRC_LLVM="${TC_SRC_UPSTREAM}/llvm"
readonly TC_SRC_BINUTILS="${TC_SRC}/binutils"
readonly TC_SRC_GOLD="${TC_SRC}/gold"
readonly TC_SRC_COMPILER_RT="${TC_SRC}/compiler-rt"

# LLVM sources (svn)
readonly TC_SRC_LLVM_MASTER="${TC_SRC}/llvm-master"
readonly TC_SRC_CLANG="${TC_SRC}/clang"

# Git sources
readonly PNACL_GIT_ROOT="${PNACL_ROOT}/git"
readonly TC_SRC_GCC="${PNACL_GIT_ROOT}/gcc"
readonly TC_SRC_GLIBC="${PNACL_GIT_ROOT}/glibc"
readonly TC_SRC_NEWLIB="${PNACL_GIT_ROOT}/nacl-newlib"
readonly TC_SRC_LIBSTDCPP="${TC_SRC_GCC}/libstdc++-v3"

# Unfortunately, binutils/configure generates this untracked file
# in the binutils source directory
readonly BINUTILS_MESS="${TC_SRC_BINUTILS}/binutils-2.20/opcodes/i386-tbl.h"

readonly SERVICE_RUNTIME_SRC="${NACL_ROOT}/src/trusted/service_runtime"
readonly EXPORT_HEADER_SCRIPT="${SERVICE_RUNTIME_SRC}/export_header.py"
readonly NACL_SYS_HEADERS="${SERVICE_RUNTIME_SRC}/include"
readonly NACL_HEADERS_TS="${TC_SRC}/nacl.sys.timestamp"
readonly NEWLIB_INCLUDE_DIR="${TC_SRC_NEWLIB}/newlib/libc/include"

# The location of each project. These should be absolute paths.
readonly TC_BUILD="${PNACL_ROOT}/build"
readonly TC_BUILD_LLVM="${TC_BUILD}/llvm"
readonly TC_BUILD_BINUTILS="${TC_BUILD}/binutils"
readonly TC_BUILD_GOLD="${TC_BUILD}/gold"
readonly TC_BUILD_BINUTILS_LIBERTY="${TC_BUILD}/binutils-liberty"
readonly TC_BUILD_NEWLIB="${TC_BUILD}/newlib"
readonly TC_BUILD_COMPILER_RT="${TC_BUILD}/compiler_rt"
readonly TC_BUILD_GCC="${TC_BUILD}/gcc"
readonly TC_BUILD_LIBELF="${TC_BUILD}/libelf"
readonly TC_BUILD_LIBELF_SB="${TC_BUILD}/libelf-sb"

readonly TIMESTAMP_FILENAME="make-timestamp"

# PNaCl toolchain installation directories (absolute paths)
readonly TOOLCHAIN_LABEL="pnacl_${BUILD_PLATFORM}_${HOST_ARCH}"
readonly INSTALL_ROOT="${TOOLCHAIN_ROOT}/${TOOLCHAIN_LABEL}"
readonly INSTALL_NEWLIB="${INSTALL_ROOT}/newlib"
readonly INSTALL_GLIBC="${INSTALL_ROOT}/glibc"
readonly INSTALL_NEWLIB_BIN="${INSTALL_NEWLIB}/bin"
readonly INSTALL_GLIBC_BIN="${INSTALL_GLIBC}/bin"


# Place links into glibc/lib-<arch> so that the driver can find
# the native libraries for bitcode linking.
# See make-glibc-link() for the primary use of this value.
# TODO(pdox): Replace with .pso stubs in glibc/lib.
readonly INSTALL_GLIBC_LIB_ARCH="${INSTALL_GLIBC}/lib-"

# Bitcode lib directories. These will soon be split apart.
# BUG= http://code.google.com/p/nativeclient/issues/detail?id=2452
readonly INSTALL_LIB_NEWLIB="${INSTALL_NEWLIB}/lib"
readonly INSTALL_LIB_GLIBC="${INSTALL_GLIBC}/lib"

# Native file library directories
# The pattern `${INSTALL_LIB_NATIVE}${arch}' is used in many places.
readonly INSTALL_LIB_NATIVE="${INSTALL_ROOT}/lib-"
readonly INSTALL_LIB_ARM="${INSTALL_LIB_NATIVE}arm"
readonly INSTALL_LIB_X8632="${INSTALL_LIB_NATIVE}x86-32"
readonly INSTALL_LIB_X8664="${INSTALL_LIB_NATIVE}x86-64"

# PNaCl client-translators (sandboxed) binary locations
readonly INSTALL_TRANSLATOR="${TOOLCHAIN_ROOT}/pnacl_translator"

# Component installation directories
readonly INSTALL_PKG="${INSTALL_ROOT}/pkg"
readonly NEWLIB_INSTALL_DIR="${INSTALL_NEWLIB}/usr"
readonly GLIBC_INSTALL_DIR="${INSTALL_GLIBC}/usr"
readonly LLVM_INSTALL_DIR="${INSTALL_PKG}/llvm"
readonly GCC_INSTALL_DIR="${INSTALL_PKG}/gcc"
readonly BINUTILS_INSTALL_DIR="${INSTALL_PKG}/binutils"
readonly BFD_PLUGIN_DIR="${BINUTILS_INSTALL_DIR}/lib/bfd-plugins"
readonly SYSROOT_DIR="${INSTALL_NEWLIB}/sysroot"
readonly FAKE_INSTALL_DIR="${INSTALL_PKG}/fake"

readonly LLVM_GOLD_PLUGIN_DIR="${LLVM_INSTALL_DIR}"/plugin
readonly LLVM_GOLD_PLUGIN="${LLVM_GOLD_PLUGIN_DIR}"/gold-plugin-static.o

# TODO(pdox): Consider getting rid of pnacl-ld libmode bias,
#             and merging these two.
readonly PNACL_LD_NEWLIB="${INSTALL_NEWLIB_BIN}/pnacl-ld"
readonly PNACL_LD_GLIBC="${INSTALL_GLIBC_BIN}/pnacl-ld"

# These driver tools are always pulled from newlib/bin,
# but this should not introduce a bias.
readonly PNACL_PP="${INSTALL_NEWLIB_BIN}/pnacl-clang -E"
readonly PNACL_AR="${INSTALL_NEWLIB_BIN}/pnacl-ar"
readonly PNACL_RANLIB="${INSTALL_NEWLIB_BIN}/pnacl-ranlib"
readonly PNACL_AS="${INSTALL_NEWLIB_BIN}/pnacl-as"
readonly PNACL_NM="${INSTALL_NEWLIB_BIN}/pnacl-nm"
readonly PNACL_TRANSLATE="${INSTALL_NEWLIB_BIN}/pnacl-translate"
readonly PNACL_READELF="${INSTALL_NEWLIB_BIN}/pnacl-readelf"
readonly PNACL_SIZE="${INSTALL_NEWLIB_BIN}/size"
readonly PNACL_STRIP="${INSTALL_NEWLIB_BIN}/pnacl-strip"
readonly ILLEGAL_TOOL="${INSTALL_NEWLIB_BIN}"/pnacl-illegal

# ELF -> PSO stub generator.
readonly PSO_STUB_GEN="${LLVM_INSTALL_DIR}/bin/pso-stub"

# PNACL_CC_NEUTRAL is pnacl-cc without LibC bias (newlib vs. glibc)
# This should only be used in conjunction with -E, -c, or -S.
readonly PNACL_CC_NEUTRAL="${INSTALL_NEWLIB_BIN}/pnacl-clang -nodefaultlibs"

# Set the frontend.
readonly FRONTEND="${FRONTEND:-clang}"

# Location of PNaCl gcc/g++
readonly PNACL_CC_NEWLIB="${INSTALL_NEWLIB_BIN}/pnacl-${FRONTEND}"
readonly PNACL_CXX_NEWLIB="${INSTALL_NEWLIB_BIN}/pnacl-${FRONTEND}++"
readonly PNACL_CC_GLIBC="${INSTALL_GLIBC_BIN}/pnacl-${FRONTEND}"
readonly PNACL_CXX_GLIBC="${INSTALL_GLIBC_BIN}/pnacl-${FRONTEND}++"

GetTool() {
  local tool=$1
  local libmode=$2
  case ${tool}-${libmode} in
  cc-newlib) echo ${PNACL_CC_NEWLIB} ;;
  cxx-newlib) echo ${PNACL_CXX_NEWLIB} ;;
  cc-glibc) echo ${PNACL_CC_GLIBC} ;;
  cxx-glibc) echo ${PNACL_CXX_GLIBC} ;;
  ld-newlib) echo ${PNACL_LD_NEWLIB} ;;
  ld-glibc) echo ${PNACL_LD_GLIBC} ;;
  *) Fatal "Invalid call to GetTool: $*" ;;
  esac
}

# GetInstallDir <libmode>
GetInstallDir() {
  local libmode=$1
  case ${libmode} in
  newlib) echo "${INSTALL_NEWLIB}" ;;
  glibc) echo "${INSTALL_GLIBC}" ;;
  *) Fatal "Unknown library mode in GetInstallDir: ${libmode}" ;;
  esac
}

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
SBTC_BUILD_WITH_PNACL="armv7 i686 x86_64"
if ${PNACL_IN_CROS_CHROOT}; then
  SBTC_BUILD_WITH_PNACL="armv7"
fi

# Current milestones in each repo
readonly UPSTREAM_REV=${UPSTREAM_REV:-67d1949cdcb1}

readonly NEWLIB_REV=346ea38d142f
readonly BINUTILS_REV=5ccab9d0bb73
readonly GOLD_REV=2a4cafa72ca0
readonly COMPILER_RT_REV=1a3a6ffb31ea

readonly LLVM_PROJECT_REV=${LLVM_PROJECT_REV:-154816}
readonly LLVM_MASTER_REV=${LLVM_PROJECT_REV}
readonly CLANG_REV=${LLVM_PROJECT_REV}

# Repositories
readonly REPO_UPSTREAM="nacl-llvm-branches.upstream"
readonly REPO_BINUTILS="nacl-llvm-branches.binutils"
# NOTE: this is essentially another binutils repo but a much more
#       recent revision to pull in all the latest gold changes
# TODO(robertm): merge the two repos -- ideally when we migrate to git
readonly REPO_GOLD="nacl-llvm-branches.gold"
readonly REPO_COMPILER_RT="nacl-llvm-branches.compiler-rt"

# LLVM repos (svn)
readonly REPO_LLVM_MASTER="http://llvm.org/svn/llvm-project/llvm/trunk"
readonly REPO_CLANG="http://llvm.org/svn/llvm-project/cfe/trunk"

CC=${CC:-gcc}
CXX=${CXX:-g++}
AR=${AR:-ar}
RANLIB=${RANLIB:-ranlib}
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

setup-libstdcpp-env() {
  # NOTE: we do not expect the assembler or linker to be used for libs
  #       hence the use of ILLEGAL_TOOL.
  # TODO: the arm bias should be eliminated
  # BUG: http://code.google.com/p/nativeclient/issues/detail?id=865
  local pnacl_cc=$(GetTool cc ${LIBSTDCPP_LIBMODE})
  local pnacl_cxx=$(GetTool cxx ${LIBSTDCPP_LIBMODE})
  STD_ENV_FOR_LIBSTDCPP=(
    CC_FOR_BUILD="${CC}"
    CC="${pnacl_cc}"
    CXX="${pnacl_cxx}"
    RAW_CXX_FOR_TARGET="${pnacl_cxx}"
    LD="${ILLEGAL_TOOL}"
    CFLAGS="--pnacl-arm-bias"
    CPPFLAGS="--pnacl-arm-bias"
    CXXFLAGS="--pnacl-arm-bias"
    CFLAGS_FOR_TARGET="--pnacl-arm-bias"
    CPPFLAGS_FOR_TARGET="--pnacl-arm-bias"
    CC_FOR_TARGET="${pnacl_cc}"
    GCC_FOR_TARGET="${pnacl_cc}"
    CXX_FOR_TARGET="${pnacl_cxx}"
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
    CC_FOR_TARGET="${PNACL_CC_NEWLIB}"
    GCC_FOR_TARGET="${PNACL_CC_NEWLIB}"
    CXX_FOR_TARGET="${PNACL_CXX_NEWLIB}"
    AR_FOR_TARGET="${PNACL_AR}"
    NM_FOR_TARGET="${PNACL_NM}"
    RANLIB_FOR_TARGET="${PNACL_RANLIB}"
    READELF_FOR_TARGET="${PNACL_READELF}"
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
  hg-info "${TC_SRC_BINUTILS}"   ${BINUTILS_REV}
  hg-info "${TC_SRC_GOLD}"       ${GOLD_REV}
  hg-info "${TC_SRC_COMPILER_RT}" ${COMPILER_RT_REV}
}

update-all() {
  hg-update-upstream
  svn-update-clang
  hg-update-binutils
  hg-update-gold
  hg-update-compiler-rt
}

# TODO(pdox): Remove after completely moved to new git pnacl repository
# Move hg/ --> pnacl/src
hg-migrate() {
  if ! [ -d "${NACL_ROOT}"/hg ] ; then
    # Nothing to do
    return 0
  fi
  if ! ${PNACL_BUILDBOT} ; then
    Banner "Migration needed: Repository paths have changed. This step will:" \
           "   1) Move hg/* to pnacl/src" \
           "   2) Wipe the old build directories (toolchain/hg-build-*)" \
           "   3) Wipe the old log directories (toolchain/log)" \
           "These have moved to pnacl/build and pnacl/build/log respectively." \
           "If you wish to stop and do the move manually, type N."
    if ! confirm-yes "Proceed" ; then
      Fatal "Aborted"
      exit -1
    fi
  fi

  mkdir -p "${TC_SRC}"
  mv "${NACL_ROOT}"/hg/* "${TC_SRC}"
  rm -rf "${TOOLCHAIN_ROOT}"/hg-build-*
  rm -rf "${TOOLCHAIN_ROOT}"/hg-log
  rmdir "${NACL_ROOT}"/hg
}

# Convert a path given on the command-line to an absolute path.
# This takes into account the fact that we changed directories at the
# beginning of this script. PWD_ON_ENTRY is used to remember the
# initial working directory.
ArgumentToAbsolutePath() {
  local relpath="$1"
  local savepwd="$(pwd)"
  cd "${PWD_ON_ENTRY}"
  GetAbsolutePath "${relpath}"
  cd "${savepwd}"
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
     hg-has-changes "${dir}" ; then
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
  # hack to work around a bug in clang that prevents building gold:
  # http://llvm.org/bugs/show_bug.cgi?id=12480
  local parser="${TC_SRC_CLANG}/include/clang/Parse/Parser.h"
  if [ -f ${parser} ] ; then
      svn revert ${parser}
  fi
  svn-update-common "clang" ${CLANG_REV} "${TC_SRC_CLANG}"
  # continuation of the hack
  sed -i -e "s/MaxDepth = 256/MaxDepth = 512/" ${parser}
}

#@ hg-update-binutils    - Update BINUTILS to the stable revision
hg-update-binutils() {
  # Clean the binutils generated file first, so that sanity checks
  # inside hg-update-common do not see any local modifications.
  binutils-mess-hide
  hg-update-common "binutils" ${BINUTILS_REV} "${TC_SRC_BINUTILS}"
  binutils-mess-unhide
}

#@ hg-update-gold    - Update GOLD to the stable revision
hg-update-gold() {
  hg-update-common "gold" ${GOLD_REV} "${TC_SRC_GOLD}"
}

#@ hg-update-compiler-rt - Update compiler-rt to the stable revision
hg-update-compiler-rt() {
  hg-update-common "compiler-rt" ${COMPILER_RT_REV} "${TC_SRC_COMPILER_RT}"
}

#@ hg-pull-all           - Pull all repos. (but do not update working copy)
#@ hg-pull-REPO          - Pull repository REPO.
#@                         (REPO can be llvm, binutils)
hg-pull-all() {
  StepBanner "HG-PULL" "Running 'hg pull' in all repos..."
  hg-pull-upstream
  hg-pull-binutils
  hg-pull-gold
  hg-pull-compiler-rt
}

hg-pull-upstream() {
  hg-pull "${TC_SRC_UPSTREAM}"
}

hg-pull-binutils() {
  hg-pull "${TC_SRC_BINUTILS}"
}

hg-pull-gold() {
  hg-pull "${TC_SRC_GOLD}"
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
  hg-checkout-binutils
  hg-checkout-gold
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

hg-checkout-binutils() {
  hg-checkout ${REPO_BINUTILS} "${TC_SRC_BINUTILS}" ${BINUTILS_REV}
}

hg-checkout-gold() {
  hg-checkout ${REPO_GOLD} "${TC_SRC_GOLD}" ${GOLD_REV}
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
  git-grab "${TC_SRC_NEWLIB}" nacl-newlib.git newlib_rev
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

  newlib-nacl-headers-clean
  cp "${PNACL_ROOT}"/DEPS "${gitbase}"/dummydir
  spushd "${gitbase}"
  gclient update
  spopd

  # Copy nacl headers into newlib tree.
  newlib-nacl-headers
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

#@ libs            - install native libs and build bitcode libs
libs() {
  libs-clean
  libs-support
  newlib
  # NOTE: glibc steals libc, libstdc++ from the other toolchain
  glibc
  compiler-rt-all
  libgcc_eh-all
  libstdcpp newlib
}

#@ everything            - Build and install untrusted SDK. no translator
everything() {
  everything-hg

  everything-post-hg
}

#@ everything-hg         - Checkout everything from the repositories
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

  llvm
  binutils
  binutils-gold

  driver

  libs

  # NOTE: we delay the tool building till after the sdk is essentially
  #      complete, so that sdk sanity checks don't fail
  misc-tools
  verify

  if ${PNACL_PRUNE}; then
    prune
  fi
}

#@ everything-translator   - Build and install untrusted SDK AND translator
everything-translator() {
  everything
  translator-all
  driver-install-translator
}

#+ translator-clean-all  - Clean all translator install/build directories
translator-clean-all() {
  StepBanner "TRANSLATOR" "Clean all"
  rm -rf "${TC_BUILD}"/translator*
  rm -rf "${INSTALL_TRANSLATOR}"*
}

translator-all() {
  local srpc_kind=srpc
  local libmode=newlib

  # Build the SDK if it not already present.
  if ! [ -d "$(GetInstallDir ${libmode})/sdk/lib" ]; then
    sdk newlib
  fi

  StepBanner "SANDBOXED TRANSLATORS"
  binutils-liberty
  if ${SBTC_PRODUCTION}; then
    # Build each architecture separately.
    local arch
    for arch in ${SBTC_BUILD_WITH_PNACL} ; do
      translator ${arch} srpc newlib
    done
  else
    # Using arch `universal` builds the sandboxed tools from a single
    # .pexe which support all targets.
    translator universal srpc newlib
  fi

  # Copy native libs to translator install dir
  cp -a ${INSTALL_LIB_NATIVE}* ${INSTALL_TRANSLATOR}

  driver-install-translator

  if ${PNACL_PRUNE}; then
    sdk-clean newlib
  fi
}

#+ translator-clean <arch> <srpcmode> <libmode> -
#+     Clean one translator install/build
translator-clean() {
  sb-setup "$@"
  StepBanner "TRANSLATOR" "Clean ${SB_LABEL}"
  rm -rf "${SB_INSTALL_DIR}"
  rm -rf "${SB_OBJDIR}"
}

#+ translator            - Build a sandboxed translator.
translator() {
  sb-setup "$@"

  # Building the sandboxed tools requires the SDK
  # For now, only build the newlib-based translator in the combined build.
  if ! [ -d "$(GetInstallDir ${SB_LIBMODE})/sdk/lib" ]; then
    echo "ERROR: SDK must be installed to build translators."
    echo "You can install the SDK by running: $0 sdk ${libmode}"
    exit -1
  fi

  llvm-sb
  binutils-sb
  binutils-gold-sb

  if ${PNACL_PRUNE}; then
    if [ "${SB_ARCH}" == universal ]; then
      # The universal pexes have already been translated.
      # They don't need to stick around.
      rm -rf "${SB_INSTALL_DIR}"
    fi
  fi
}


# Builds crt1.bc for GlibC, which is just sysdeps/nacl/start.c and csu/init.c
glibc-crt1() {
  StepBanner "GLIBC" "Building crt1.bc"
  local tmpdir="${TC_BUILD}/glibc-crt1"
  local flags="-no-save-temps -DUSE_IN_LIBIO -I${TC_SRC_GLIBC}/sysdeps/gnu"
  local cc_cmd="${PNACL_CC_GLIBC} ${flags}"
  local ld_cmd="${PNACL_LD_GLIBC}"

  rm -rf "${tmpdir}"
  mkdir -p "${tmpdir}"
  spushd "${tmpdir}"
  ${cc_cmd} -c "${TC_SRC_GLIBC}"/sysdeps/nacl/start.c -o start.bc
  ${cc_cmd} -c "${TC_SRC_GLIBC}"/csu/init.c -o init.bc
  ${ld_cmd} -r -nostdlib -no-save-temps start.bc init.bc -o crt1.bc
  mkdir -p "${INSTALL_LIB_GLIBC}"
  cp crt1.bc "${INSTALL_LIB_GLIBC}"
  spopd
}

glibc() {
  StepBanner "GLIBC"
  glibc-copy
  glibc-crt1
}

glibc-copy() {
  StepBanner "GLIBC" "Copying glibc from NNaCl toolchain"

  mkdir -p "${INSTALL_LIB_X8632}"
  mkdir -p "${INSTALL_LIB_X8664}"
  mkdir -p "${GLIBC_INSTALL_DIR}"

  # Figure out the GlibC version number.
  local naclgcc_base="${NNACL_GLIBC_ROOT}/${NACL64_TARGET}"
  local ver
  spushd "${naclgcc_base}/lib"
  ver=`echo libc.so.*`
  ver=${ver/libc\.so\./}
  spopd
  StepBanner "GLIBC" "GLibC version ID is ${ver}"

  ######################################################################
  # Set up libs for native linking.

  # Files to copy from ${naclgcc_base} into the translator library directories
  local LIBS_TO_COPY="libstdc++.so.6 \
                      libc_nonshared.a \
                      libc.so.${ver} \
                      libm.so.${ver} \
                      libdl.so.${ver} \
                      librt.so.${ver} \
                      libmemusage.so \
                      libpthread_nonshared.a \
                      libpthread.so.${ver} \
                      runnable-ld.so \
                      ld-2.9.so"
  local lib
  for lib in ${LIBS_TO_COPY} ; do
    cp -a "${naclgcc_base}/lib32/"${lib} "${INSTALL_LIB_X8632}"
    cp -a "${naclgcc_base}/lib/"${lib} "${INSTALL_LIB_X8664}"
  done

  ######################################################################
  # Set up libs for bitcode linking.

  # libc.so and libpthread.so are linker scripts
  # Place them in the GLibC arch-specific directory only.
  # They don't need to be in the translator directory.
  # TODO(pdox): These should go in the architecture-independent glibc/lib
  # directory, but they have an OUTPUT_FORMAT statement. Gold might already
  # ignore this.
  # TODO(jvoung): Perhaps we should just make our own version of these
  # linker scripts that are architecture independent and refer to the
  # bitcode stub files and bitcode archives (for libx_nonshared.a).
  # Also, we need to make a differently named libc.pso and libpthread.pso
  # that do not clash with the linker scripts / have the linker scripts
  # refer to these pso stubs instead of the native libs.
  mkdir -p "${INSTALL_GLIBC_LIB_ARCH}"x86-32
  mkdir -p "${INSTALL_GLIBC_LIB_ARCH}"x86-64
  local lib
  for lib in libc libpthread; do
    cp -a "${naclgcc_base}"/lib32/${lib}.so "${INSTALL_GLIBC_LIB_ARCH}"x86-32
    cp -a "${naclgcc_base}"/lib/${lib}.so "${INSTALL_GLIBC_LIB_ARCH}"x86-64
    make-glibc-pso-stubs ${lib}.so.${ver} ${lib}-2.9.so
    make-glibc-pso-stubs ${lib}.so.${ver} ${lib}.so.${ver}
  done

  # Make pso stubs for other libraries.
  local lib
  for lib in libdl libm librt; do
    make-glibc-pso-stubs ${lib}.so.${ver} ${lib}.pso
    make-glibc-pso-stubs ${lib}.so.${ver} ${lib}-2.9.pso
  done
  make-glibc-pso-stubs libstdc++.so.6 libstdc++.pso
  make-glibc-pso-stubs libstdc++.so.6 libstdc++.pso.6.0.13
  make-glibc-pso-stubs libmemusage.so libmemusage.pso

  # BUG= http://code.google.com/p/nativeclient/issues/detail?id=2615
  make-glibc-link all libc_nonshared.a libc_nonshared.a
  make-glibc-link all libpthread_nonshared.a libpthread_nonshared.a

  # Copy linker scripts
  # We currently only depend on elf[64]_nacl.x,
  # elf[64]_nacl.xs, and elf[64]_nacl.x.static.
  # TODO(pdox): Remove the need for these, by making the built-in
  #             binutils linker script work.
  local ldscripts="${NNACL_GLIBC_ROOT}/${NACL64_TARGET}/lib/ldscripts"
  local ext
  for ext in .x .xs .x.static; do
    cp -a "${ldscripts}"/elf_nacl${ext}   "${INSTALL_LIB_X8632}"
    make-glibc-link x86-32 elf_nacl${ext} elf_nacl${ext}

    cp -a "${ldscripts}"/elf64_nacl${ext} "${INSTALL_LIB_X8664}"
    make-glibc-link x86-64 elf64_nacl${ext} elf64_nacl${ext}
  done

  # ld-nacl have different sonames across 32/64.
  # Create symlinks to make them look the same.
  # TODO(pdox): Make the sonames match in GlibC.
  #             Also, replace these links with PSO stubs.
  # This is referred to by the libc.so linker script.  Perhaps we could
  # make a platform agnostic linker script that just points to ld-nacl.pso
  # and the libc-2.9.pso.  We also need to handle libc_nonshared.a,
  # as mentioned above.  That could perhaps be built as a bitcode archive.
  # It just contains fstat, fstat64, etc. anyway.
  for arch in x86-32 x86-64; do
    make-glibc-link ${arch} ld-2.9.so ld-nacl-${arch}.so.1
  done

  # Copy the glibc headers
  mkdir -p "${GLIBC_INSTALL_DIR}"/include
  cp -a "${NNACL_GLIBC_ROOT}"/${NACL64_TARGET}/include/* \
        "${GLIBC_INSTALL_DIR}"/include
  install-unwind-header
}


#+ make-glibc-link <arch> <target> <linkname> -
#+     Create a symbolic link from the GLibC arch-specific lib directory
#+     to the translator lib directory.
#+     <arch> can be a single arch, a list, or 'all'.
make-glibc-link() {
  local arches="$1"
  local target="$2"
  local linkname="$3"

  if [ "${arches}" == "all" ]; then
    arches="x86-32 x86-64"
  fi

  local arch
  for arch in ${arches}; do
    local dest="${INSTALL_GLIBC_LIB_ARCH}"${arch}
    mkdir -p "${dest}"
    ln -sf ../../lib-${arch}/"${target}" "${dest}"/"${linkname}"
  done
}

#+ make-glibc-pso-stubs <src_native_basename> <dest_stub_basename> -
#+     Create a pso-stub for GLibC bitcode lib directory based on the native
#+     .so files in the translator lib directory.
make-glibc-pso-stubs() {
  local src_lib=$1
  local target_lib=$2

  local dest_dir="${INSTALL_LIB_GLIBC}"
  # For now, pick x86-64 .so as the baseline for our .pso files.
  local neutral_arch="x86-64"
  local src_dir="${INSTALL_LIB_NATIVE}${neutral_arch}"

  mkdir -p "${dest_dir}"
  RunWithLog "glibc.pso_stub_gen" \
    "${PSO_STUB_GEN}" "${src_dir}/${src_lib}" -o "${dest_dir}/${target_lib}"
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
  local backup_dir="${PNACL_ROOT}/fast-clean-llvm-backup"

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
  rm -rf "${INSTALL_LIB_NEWLIB}"/*
  rm -rf "${INSTALL_LIB_GLIBC}"/*
  rm -rf "${INSTALL_LIB_NATIVE}"*
}


#@-------------------------------------------------------------------------

#+ prune                 - Prune toolchain
prune() {
  StepBanner "PRUNE" "Pruning toolchain"
  # ACCEPTABLE_SIZE should be much lower for real release,
  # but we are currently doing a debug build and not pruning
  # as aggressively as we could.
  local ACCEPTABLE_SIZE=300
  local dir_size_before=$(get_dir_size_in_mb ${INSTALL_ROOT})

  SubBanner "Size before: ${INSTALL_ROOT} ${dir_size_before}MB"
  echo "stripping binaries (binutils)"
  strip "${BINUTILS_INSTALL_DIR}"/bin/*

  echo "stripping binaries (llvm)"
  if ! strip "${LLVM_INSTALL_DIR}"/bin/* ; then
    echo "NOTE: some failures during stripping are expected"
  fi

  echo "removing llvm headers"
  rm -rf "${LLVM_INSTALL_DIR}"/include/llvm*

  echo "removing .pyc files"
  rm -f "${INSTALL_NEWLIB_BIN}"/pydir/*.pyc
  rm -f "${INSTALL_GLIBC_BIN}"/pydir/*.pyc

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
  local tarball="$(ArgumentToAbsolutePath "$1")"
  RecordRevisionInfo
  StepBanner "TARBALL" "Creating tar ball ${tarball}"
  tar zcf "${tarball}" -C "${INSTALL_ROOT}" .
}

translator-tarball() {
  if [ ! -n "${1:-}" ]; then
    echo "Error: tarball needs a tarball name." >&2
    exit 1
  fi
  local tarball="$(ArgumentToAbsolutePath "$1")"
  StepBanner "TARBALL" "Creating translator tar ball ${tarball}"
  tar zcf "${tarball}" -C "${INSTALL_TRANSLATOR}" .
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

  libelf-host
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

  # Provide libelf (host version)
  local cppflags
  cppflags+=" -I${TC_BUILD_LIBELF}/install/include"
  cppflags+=" -L${TC_BUILD_LIBELF}/install/lib"

  llvm-link-clang
  # The --with-binutils-include is to allow llvm to build the gold plugin
  local binutils_include="${TC_SRC_BINUTILS}/binutils-2.20/include"
  RunWithLog "llvm.configure" \
      env -i PATH=/usr/bin/:/bin \
             MAKE_OPTS=${MAKE_OPTS} \
             CC="${CC} ${cppflags}" \
             CXX="${CXX} ${cppflags}" \
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
  # Provide libelf (host version)
  local cppflags
  cppflags+=" -I${TC_BUILD_LIBELF}/install/include"
  cppflags+=" -L${TC_BUILD_LIBELF}/install/lib"

  RunWithLog llvm.make \
    env -i PATH=/usr/bin/:/bin \
           MAKE_OPTS="${MAKE_OPTS}" \
           NACL_SANDBOX=0 \
           NACL_SB_JIT=0 \
           CC="${CC} ${cppflags}" \
           CXX="${CXX} ${cppflags}" \
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
    env -i PATH=/usr/bin/:/bin \
           MAKE_OPTS="${MAKE_OPTS}" \
           NACL_SANDBOX=0 \
           NACL_SB_JIT=0 \
           CC="${CC}" \
           CXX="${CXX}" \
           make ${MAKE_OPTS} install
  spopd

  llvm-install-plugin
}

# Produces gold-plugin-static.o, which is the gold plugin
# with all LLVM dependencies statically linked in.
llvm-install-plugin() {
  StepBanner "LLVM" "Installing plugin"
  mkdir -p "${LLVM_GOLD_PLUGIN_DIR}"

  # This pulls every LLVM dependency needed for gold-plugin.o and libLTO.
  local components="x86 arm linker analysis ipo bitwriter"
  local libpath="${TC_BUILD_LLVM}"/Release+Asserts/lib

  ${CC} -r -nostdlib \
         "${TC_BUILD_LLVM}"/tools/gold/Release+Asserts/gold-plugin.o \
        "${libpath}"/libLTO.a \
        $("${LLVM_INSTALL_DIR}"/bin/llvm-config --libfiles ${components}) \
        -o "${LLVM_GOLD_PLUGIN}"

  # Force re-link of ld, gold, ar, ranlib, nm
  rm -f "${TC_BUILD_BINUTILS}"/binutils/ar
  rm -f "${TC_BUILD_BINUTILS}"/binutils/nm-new
  rm -f "${TC_BUILD_BINUTILS}"/binutils/ranlib
  rm -f "${TC_BUILD_BINUTILS}"/gold/ld-new
  rm -f "${TC_BUILD_BINUTILS}"/ld/ld-new
}

#########################################################################
#########################################################################
#     < LIBGCC_EH >
#########################################################################
#########################################################################

libgcc_eh-all() {
  StepBanner "LIBGCC_EH (from GCC 4.6)"

  # Build libgcc_eh.a using Newlib, and libgcc_s.so using GlibC.
  # This is a temporary situation. Eventually, libgcc_eh won't depend
  # on LibC, and it will be built once for each architecture in a neutral way.
  libgcc_eh arm    newlib
  libgcc_eh x86-32 newlib
  libgcc_eh x86-64 newlib
  # ARM GlibC can't be built because libc.so is missing.
  libgcc_eh x86-32 glibc
  libgcc_eh x86-64 glibc
}

libgcc_eh() {
  local arch=$1
  local libmode=$2
  check-libmode ${libmode}

  local objdir="${TC_BUILD}/libgcc_eh-${arch}-${libmode}"
  local subdir="${objdir}/fake-target/libgcc"
  local installdir="${INSTALL_LIB_NATIVE}${arch}"
  if [ ${libmode} == "glibc" ]; then
    local label="(${arch} libgcc_s.so)"
  else
    local label="(${arch} libgcc_eh.a)"
  fi

  mkdir -p "${installdir}"
  if [ ${libmode} == "glibc" ]; then
    rm -rf "${installdir}"/libgcc_s*
  else
    rm -rf "${installdir}"/libgcc_eh*
  fi
  rm -rf "${objdir}"

  # Setup fake gcc build directory.
  mkdir -p "${objdir}"/gcc
  cp -a "${PNACL_ROOT}"/scripts/libgcc-${libmode}.mvars \
        "${objdir}"/gcc/libgcc.mvars
  cp -a "${PNACL_ROOT}"/scripts/libgcc-tconfig.h "${objdir}"/gcc/tconfig.h
  touch "${objdir}"/gcc/tm.h

  install-unwind-header

  mkdir -p "${subdir}"
  spushd "${subdir}"
  local flags="-arch ${arch} --pnacl-bias=${arch} --pnacl-allow-translate"
  flags+=" --pnacl-allow-native"
  if [ "${libmode}" == "glibc" ]; then
    # Enable thread safety using pthreads
    # Thread safety requires pthread_mutex_*. For the newlib case,
    # these functions won't be available in the bitcode unless
    # explicitly preserved, but we don't really want to do that.
    # Instead, this problem will go away once libgcc_eh is no
    # longer dependent on libc.
    # BUG= http://code.google.com/p/nativeclient/issues/detail?id=2492
    flags+=" -D_PTHREADS -D_GNU_SOURCE"
  fi
  flags+=" -DENABLE_RUNTIME_CHECKING"

  StepBanner "LIBGCC_EH" "Configure ${label}"
  RunWithLog libgcc.${arch}.configure \
    env -i \
      PATH="/usr/bin:/bin" \
      CC="$(GetTool cc ${libmode}) ${flags}" \
      CXX="$(GetTool cxx ${libmode}) ${flags}" \
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

  StepBanner "LIBGCC_EH" "Make ${label}"
  if [ "${libmode}" == "newlib" ]; then
    RunWithLog libgcc.${arch}.make \
      make libgcc_eh.a
  elif [ "${libmode}" == "glibc" ]; then
    # disable_libgcc_base=yes is a custom option which
    # removes the non-unwind functions from libgcc_s.so
    RunWithLog libgcc_s.${arch}.make \
      make disable_libgcc_base=yes libgcc_s.so
  fi
  spopd

  StepBanner "LIBGCC_EH" "Install ${label}"
  if [ "${libmode}" == "newlib" ]; then
    cp ${subdir}/libgcc_eh.a "${installdir}"
  elif [ "${libmode}" == "glibc" ]; then
    cp ${subdir}/libgcc_s.so.1 "${installdir}"
    make-glibc-link ${arch} libgcc_s.so.1 libgcc_s.so
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
  compiler-rt arm
  compiler-rt x86-32
  compiler-rt x86-64
}


#+ compiler-rt           - build/install llvm's replacement for libgcc.a
compiler-rt() {
  local arch=$1
  local src="${TC_SRC_COMPILER_RT}/compiler-rt/lib"
  local objdir="${TC_BUILD_COMPILER_RT}-${arch}"
  local installdir="${INSTALL_LIB_NATIVE}${arch}"
  StepBanner "compiler rt" "build (${arch})"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"

  spushd "${objdir}"
  RunWithLog libgcc.${arch}.make \
      make -j ${PNACL_CONCURRENCY} -f ${src}/Makefile-pnacl libgcc.a \
        CC="${PNACL_CC_NEUTRAL}" \
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

check-libmode() {
  local libmode=$1
  if [ ${libmode} != "newlib" ] && [ ${libmode} != "glibc" ]; then
    echo "ERROR: Unsupported library mode. Choose one of: newlib, glibc"
    exit -1
  fi
}

LIBSTDCPP_IS_SETUP=false
libstdcpp-setup() {
  if ${LIBSTDCPP_IS_SETUP} && [ $# -eq 0 ]; then
    return 0
  fi
  if [ $# -ne 1 ]; then
    Fatal "Please specify library mode: newlib or glibc"
  fi
  check-libmode "$1"
  LIBSTDCPP_LIBMODE=$1
  LIBSTDCPP_IS_SETUP=true
  LIBSTDCPP_BUILD="${TC_BUILD}/libstdcpp-${LIBSTDCPP_LIBMODE}"
  LIBSTDCPP_INSTALL_DIR="$(GetInstallDir ${LIBSTDCPP_LIBMODE})/usr"
}

libstdcpp() {
  libstdcpp-setup "$@"
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
  libstdcpp-setup "$@"
  StepBanner "LIBSTDCPP" "Clean"
  rm -rf "${LIBSTDCPP_BUILD}"
}

libstdcpp-needs-configure() {
  libstdcpp-setup "$@"
  ts-newer-than "${TC_BUILD_LLVM}" \
                "${LIBSTDCPP_BUILD}" && return 0
  [ ! -f "${LIBSTDCPP_BUILD}/config.status" ]
  return #?
}

libstdcpp-configure() {
  libstdcpp-setup "$@"
  StepBanner "LIBSTDCPP" "Configure"
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="${LIBSTDCPP_BUILD}"
  local subdir="${LIBSTDCPP_BUILD}/pnacl-target"

  mkdir -p "${subdir}"
  spushd "${subdir}"

  local flags=""
  if [ ${LIBSTDCPP_LIBMODE} == "newlib" ]; then
    flags+="--with-newlib --disable-shared --disable-rpath"
  elif [ ${LIBSTDCPP_LIBMODE} == "glibc" ]; then
    Fatal "libstdcpp glibc not yet supported"
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
  libstdcpp-setup "$@"
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="${LIBSTDCPP_BUILD}"

  ts-modified "${srcdir}" "${objdir}"
  return $?
}

libstdcpp-make() {
  libstdcpp-setup "$@"
  StepBanner "LIBSTDCPP" "Make"
  local srcdir="${TC_SRC_LIBSTDCPP}"
  local objdir="${LIBSTDCPP_BUILD}"

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
  libstdcpp-setup "$@"
  StepBanner "LIBSTDCPP" "Install"
  local objdir="${LIBSTDCPP_BUILD}"

  spushd "${objdir}/pnacl-target"

  # Clean the existing installation
  rm -rf "${LIBSTDCPP_INSTALL_DIR}"/include/c++
  rm -rf "${LIBSTDCPP_INSTALL_DIR}"/lib/libstdc++*

  # install headers (=install-data)
  # for good measure make sure we do not keep any old headers
  setup-libstdcpp-env
  RunWithLog libstdcpp.install \
    make \
    "${STD_ENV_FOR_LIBSTDCPP[@]}" \
    ${MAKE_OPTS} install-data

  # Install bitcode library
  mkdir -p "${LIBSTDCPP_INSTALL_DIR}/lib"
  cp "${objdir}/pnacl-target/src/.libs/libstdc++.a" \
     "${LIBSTDCPP_INSTALL_DIR}/lib"
  spopd

  # libstdc++ installs a file with an abnormal name: "libstdc++*-gdb.py"
  # The asterisk may be due to a bug in libstdc++ Makefile/configure.
  # This causes problems on the Windows bot (during cleanup, toolchain
  # directory delete fails due to the bad character).
  # Rename it to get rid of the asterisk.
  spushd "${LIBSTDCPP_INSTALL_DIR}/lib"
  mv -f libstdc++'*'-gdb.py libstdc++-gdb.py
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
#     < LIBELF >
#########################################################################
#+ libelf-host           - Build and install libelf (using the host CC)
libelf-host() {
  StepBanner "LIBELF-HOST" "Building and installing libelf"

  local extra_headers=false
  local ranlib="${RANLIB}"
  if ${BUILD_PLATFORM_MAC}; then
    extra_headers=true
    # There is a common symbol that must be treated as a definition.
    # In particular "__libelf_fill_byte".
    ranlib="${RANLIB} -c"
  elif ${BUILD_PLATFORM_WIN}; then
    # This might be overkill. We really only need to stub-out libintl.h
    # to avoid a dependency on libintl when linking LLVM with libelf.
    # Note: this is (a) disable language translation of error messages, and
    # (b) was only due to --disable-nls not working.
    # Anyway, including consistent headers hopefully does not harm.
    extra_headers=true
  fi

  RunWithLog "libelf-host" \
    env -i \
      PATH="/usr/bin:/bin" \
      DEST_DIR="${TC_BUILD_LIBELF}" \
      CC="${CC}" \
      AR="${AR}" \
      RANLIB="${ranlib}" \
      EXTRA_HEADERS=${extra_headers} \
      "${PNACL_ROOT}"/libelf/build-libelf.sh
}

#+ libelf-sb            - Build and install libelf (using pnacl-clang)
libelf-sb() {
  StepBanner "LIBELF-SB" "Building and installing sandboxed libelf"
  RunWithLog "libelf-sb" \
    env -i \
      PATH="/usr/bin:/bin" \
      DEST_DIR="${TC_BUILD_LIBELF_SB}" \
      CC="${PNACL_CC_NEWLIB}" \
      AR="${PNACL_AR}" \
      RANLIB="${PNACL_RANLIB}" \
      EXTRA_HEADERS=true \
      "${PNACL_ROOT}"/libelf/build-libelf.sh
}

#########################################################################
#     < BINUTILS >
#########################################################################

#+-------------------------------------------------------------------------
#+ binutils          - Build and install binutils (cross-tools)
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

  binutils-dlwrap
  binutils-make
  binutils-install
}

#+ binutils-clean    - Clean binutils
binutils-clean() {
  StepBanner "BINUTILS" "Clean"
  local objdir="${TC_BUILD_BINUTILS}"
  rm -rf ${objdir}
}

binutils-dlwrap() {
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="${TC_BUILD_BINUTILS}"

  if ! [ -f "${LLVM_GOLD_PLUGIN}" ]; then
    Fatal "LLVM must be installed before building binutils."
  fi

  # Build libdlwrap.a
  mkdir -p "${objdir}/dlwrap"
  spushd "${objdir}/dlwrap"
  ${CC} -c "${srcdir}"/dlwrap/dlwrap.c -o dlwrap.o
  cp "${LLVM_GOLD_PLUGIN}" gold_plugin_static.o
  ${AR} crs libdlwrap.a dlwrap.o gold_plugin_static.o
  spopd
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

  # We llvm's mc for asseblemy so we no longer build gas

  # TODO(pdox): Building binutils for nacl/nacl64 target currently requires
  # providing NACL_ALIGN_* defines. This should really be defined inside
  # binutils instead.
  local flags="-DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5 \
               -I${TC_SRC_BINUTILS}/dlwrap -DENABLE_DLWRAP"
  RunWithLog binutils.configure \
    env -i \
    PATH="/usr/bin:/bin" \
    CC="${CC}" \
    CXX="${CXX}" \
    LDFLAGS="-L${objdir}/dlwrap" \
    CFLAGS="${flags}" \
    CXXFLAGS="${flags}" \
    ${srcdir}/binutils-2.20/configure --prefix="${BINUTILS_INSTALL_DIR}" \
                                      --target=${BINUTILS_TARGET} \
                                      --enable-targets=${targ} \
                                      --enable-gold=yes \
                                      --enable-ld=yes \
                                      --enable-plugins \
                                      --disable-werror \
                                      --without-gas \
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

check-srpcmode() {
  local mode=$1
  if [ ${mode} != "srpc" ] && [ ${mode} != "nonsrpc" ]; then
    echo "ERROR: Unsupported mode. Choose one of: srpc, nonsrpc"
    exit -1
  fi
}

check-arch() {
  local arch=$1
  for valid_arch in i686 x86_64 armv7 universal ; do
    if [ "${arch}" == "${valid_arch}" ] ; then
      return
    fi
  done

  Fatal "ERROR: Unsupported arch [$1]. Must be: i686, x86_64, armv7, universal"
}

SB_SETUP=false
sb-setup() {
  if ${SB_SETUP} && [ $# -eq 0 ]; then
    return 0
  fi
  if [ $# -ne 3 ] ; then
    Fatal "Please specify arch, mode(srpc/nonsrpc) and libmode(newlib/glibc)"
  fi

  SB_SETUP=true
  SB_ARCH=$1
  SB_SRPCMODE=$2
  SB_LIBMODE=$3
  check-arch ${SB_ARCH}
  check-srpcmode ${SB_SRPCMODE}
  check-libmode ${SB_LIBMODE}

  SB_LOG_PREFIX="${SB_ARCH}.${SB_SRPCMODE}.${SB_LIBMODE}"
  SB_LABEL="${SB_ARCH}_${SB_SRPCMODE}_${SB_LIBMODE}"
  SB_OBJDIR="${TC_BUILD}/translator-${SB_LABEL//_/-}"
  SB_INSTALL_DIR="$(GetTranslatorInstallDir ${SB_ARCH})"
}

LLVM_SB_SETUP=false
llvm-sb-setup() {
  sb-setup "$@"
  if ${SB_JIT}; then
    llvm-sb-setup-jit "$@"
    return
  fi
  local flags=""
  LLVM_SB_LOG_PREFIX="llvm.sb.${SB_LOG_PREFIX}"
  LLVM_SB_OBJDIR="${SB_OBJDIR}/llvm-sb"

  if [ ${SB_LIBMODE} == newlib ]; then
    flags+=" -static"
  fi

  case ${SB_SRPCMODE} in
    srpc)    flags+=" -DNACL_SRPC" ;;
    nonsrpc) ;;
  esac

  # Provide libelf (sandboxed version)
  local cppflags
  cppflags+=" -I${TC_BUILD_LIBELF_SB}/install/include"
  cppflags+=" -L${TC_BUILD_LIBELF_SB}/install/lib"

  LLVM_SB_EXTRA_CONFIG_FLAGS="--disable-jit --enable-optimized \
  --target=${CROSS_TARGET_ARM}"

  LLVM_SB_CONFIGURE_ENV=(
    AR="${PNACL_AR}" \
    AS="${PNACL_AS}" \
    CC="$(GetTool cc ${SB_LIBMODE}) ${flags} ${cppflags}" \
    CXX="$(GetTool cxx ${SB_LIBMODE}) ${flags} ${cppflags}" \
    LD="$(GetTool ld ${SB_LIBMODE}) ${flags}" \
    NM="${PNACL_NM}" \
    RANLIB="${PNACL_RANLIB}" \
    LDFLAGS="") # TODO(pdox): Support -s
}

# TODO(pdox): Unify with llvm-sb-setup above.
llvm-sb-setup-jit() {
  sb-setup "$@"
  if ${LLVM_SB_SETUP}; then
    return 0
  fi
  LLVM_SB_SETUP=true
  LLVM_SB_LOG_PREFIX="llvm.sb.${SB_LOG_PREFIX}"
  LLVM_SB_OBJDIR="${SB_OBJDIR}/llvm-sb"

  local flags=""
  case ${SB_SRPCMODE} in
    srpc)    flags+=" -DNACL_SRPC" ;;
    nonsrpc) ;;
  esac

  local naclgcc_root="";
  case ${SB_LIBMODE} in
    newlib)   naclgcc_root="${NNACL_NEWLIB_ROOT}";;
    glibc)    naclgcc_root="${NNACL_GLIBC_ROOT}";;
  esac
  local gcc_arch=""
  case ${SB_ARCH} in
    i686)  gcc_arch="i686";;
    x86_64)  gcc_arch="x86_64";;
    default) Fatal "Can't build universal/arm translator with nacl-gcc";;
  esac

  LLVM_SB_EXTRA_CONFIG_FLAGS="--enable-jit --disable-optimized \
  --target=${gcc_arch}-nacl"

  LLVM_SB_CONFIGURE_ENV=(
    AR="${naclgcc_root}/bin/${gcc_arch}-nacl-ar" \
    As="${naclgcc_root}/bin/${gcc_arch}-nacl-as" \
    CC="${naclgcc_root}/bin/${gcc_arch}-nacl-gcc ${flags}" \
    CXX="${naclgcc_root}/bin/${gcc_arch}-nacl-g++ ${flags}" \
    LD="${naclgcc_root}/bin/${gcc_arch}-nacl-ld" \
    NM="${naclgcc_root}/bin/${gcc_arch}-nacl-nm" \
    RANLIB="${naclgcc_root}/bin/${gcc_arch}-nacl-ranlib" \
    LDFLAGS="") # TODO(pdox): Support -s
}

#+-------------------------------------------------------------------------
#+ llvm-sb <arch> <mode> - Build and install llvm tools (sandboxed)
llvm-sb() {
  llvm-sb-setup "$@"
  StepBanner "LLVM-SB" "Sandboxed llc + lli [${SB_LABEL}]"
  local srcdir="${TC_SRC_LLVM}"
  assert-dir "${srcdir}" "You need to checkout llvm."

  libelf-sb
  if llvm-sb-needs-configure ; then
    llvm-sb-clean
    llvm-sb-configure
  else
    SkipBanner "LLVM-SB" "configure ${SB_ARCH} ${SB_SRPCMODE}"
  fi

  llvm-sb-make
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
  StepBanner "LLVM-SB" "Clean ${SB_ARCH} ${SB_SRPCMODE}"
  local objdir="${LLVM_SB_OBJDIR}"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"
}

# llvm-sb-configure - Configure llvm tools (sandboxed)
llvm-sb-configure() {
  llvm-sb-setup "$@"

  StepBanner "LLVM-SB" "Configure ${SB_ARCH} ${SB_SRPCMODE}"
  local srcdir="${TC_SRC_LLVM}"
  local objdir="${LLVM_SB_OBJDIR}"
  local installdir="${SB_INSTALL_DIR}"
  local targets=""
  case ${SB_ARCH} in
    i686) targets=x86 ;;
    x86_64) targets=x86_64 ;;
    armv7) targets=arm ;;
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

# llvm-sb-make - Make llvm tools (sandboxed)
llvm-sb-make() {
  llvm-sb-setup "$@"

  StepBanner "LLVM-SB" "Make ${SB_ARCH} ${SB_SRPCMODE}"
  local objdir="${LLVM_SB_OBJDIR}"

  spushd "${objdir}"
  ts-touch-open "${objdir}"

  local tools_to_build="llc"
  local isjit=0
  if ${SB_JIT}; then
    isjit=1
    tools_to_build="llc lli"
  fi
  RunWithLog ${LLVM_SB_LOG_PREFIX}.make \
      env -i PATH="/usr/bin:/bin" \
      ONLY_TOOLS="${tools_to_build}" \
      NACL_SANDBOX=1 \
      NACL_SB_JIT=${isjit} \
      KEEP_SYMBOLS=1 \
      VERBOSE=1 \
      make ${MAKE_OPTS} tools-only

  ts-touch-commit "${objdir}"

  spopd
}

# llvm-sb-install - Install llvm tools (sandboxed)
llvm-sb-install() {
  llvm-sb-setup "$@"

  StepBanner "LLVM-SB" "Install ${SB_ARCH} ${SB_SRPCMODE}"
  local objdir="${LLVM_SB_OBJDIR}"

  # Install only llc or lli
  local toolname="llc"
  if ${SB_JIT}; then
    toolname="lli"
  fi
  mkdir -p "${SB_INSTALL_DIR}"/bin
  spushd "${SB_INSTALL_DIR}"/bin
  cp -f "${objdir}"/Release*/bin/${toolname} .
  if ${SB_JIT} ; then
    # JIT is always built as .nexe
    mv -f ${toolname} ${toolname}.nexe
  else
    mv -f ${toolname} ${toolname}.pexe
    translate-sb-tool ${toolname}
    install-sb-tool ${toolname}
  fi
  spopd
}

# translate-tool <toolname>
#
# Translate <toolname>.pexe to <toolname>.<arch>.nexe in the current directory.
# If SB_ARCH is universal, translates to every arch in SBTC_BUILD_WITH_PNACL.
translate-sb-tool() {
  local toolname=$1
  local arches
  if [ "${SB_ARCH}" == "universal" ]; then
    arches="${SBTC_BUILD_WITH_PNACL}"
  else
    arches="${SB_ARCH}"
  fi
  local pexe="${toolname}.pexe"
  ${PNACL_STRIP} "${pexe}"

  local tarch
  for tarch in ${arches}; do
    local nexe="${toolname}.${tarch}.nexe"
    StepBanner "TRANSLATE" \
               "Translating ${toolname}.pexe to ${tarch} (background)"
    # TODO(pdox): Get the SDK location based on libmode.
    "${PNACL_TRANSLATE}" -arch ${tarch} "${pexe}" -o "${nexe}" &
    QueueLastProcess
  done
  StepBanner "TRANSLATE" "Waiting for translation processes to finish"
  QueueWait

  # Strip the nexes
  for tarch in ${arches}; do
    local nexe="${toolname}.${tarch}.nexe"
    ${PNACL_STRIP} "${nexe}"
  done
  StepBanner "TRANSLATE" "Done."
}

# Install <toolname>.<arch>.nexe from the current directory
# into the right location (as <toolname>.nexe)
install-sb-tool() {
  local toolname="$1"
  local arches
  if [ "${SB_ARCH}" == "universal" ]; then
    arches="${SBTC_BUILD_WITH_PNACL}"
  else
    arches="${SB_ARCH}"
  fi
  local tarch
  for tarch in ${arches}; do
    local installbin="$(GetTranslatorInstallDir ${tarch})"/bin
    mkdir -p "${installbin}"
    mv -f ${toolname}.${tarch}.nexe "${installbin}"/${toolname}.nexe
    if ${PNACL_BUILDBOT}; then
      spushd "${installbin}"
      print-size-of-sb-tool ${tarch} ${toolname}
      spopd
    fi
  done
}

GetTranslatorInstallDir() {
  local arch="$1"
  local extra=""
  if ${SB_JIT}; then
    extra+="_jit"
  elif [ ${SB_SRPCMODE} == "nonsrpc" ]; then
    extra+="_nonsrpc"
  fi
  echo "${INSTALL_TRANSLATOR}"${extra}/${arch}
}

#---------------------------------------------------------------------

BINUTILS_SB_SETUP=false
binutils-sb-setup() {
  sb-setup "$@"

  # TODO(jvoung): investigate if these are only needed by AS or not.
  local flags="-DNACL_ALIGN_BYTES=32 -DNACL_ALIGN_POW2=5 -DNACL_TOOLCHAIN_PATCH"
  if ${BINUTILS_SB_SETUP}; then
    return 0
  fi
  BINUTILS_SB_SETUP=true
  BINUTILS_SB_LOG_PREFIX="binutils.sb.${SB_LOG_PREFIX}"
  BINUTILS_SB_OBJDIR="${SB_OBJDIR}/binutils-sb"

  if [ ${SB_LIBMODE} == newlib ]; then
    flags+=" -static"
  fi

  # we do not support the non-srpc mode anymore
  flags+=" -DNACL_SRPC"

  if [ ! -f "${TC_BUILD_BINUTILS_LIBERTY}/libiberty/libiberty.a" ] ; then
    echo "ERROR: Missing lib. Run this script with binutils-liberty option"
    exit -1
  fi

  BINUTILS_SB_CONFIGURE_ENV=(
    AR="${PNACL_AR}" \
    AS="${PNACL_AS}" \
    CC="$(GetTool cc ${SB_LIBMODE}) ${flags}" \
    CXX="$(GetTool cxx ${SB_LIBMODE}) ${flags}" \
    CC_FOR_BUILD="${CC}" \
    CXX_FOR_BUILD="${CXX}" \
    LD="$(GetTool ld ${SB_LIBMODE}) ${flags}" \
    NM="${PNACL_NM}" \
    RANLIB="${PNACL_RANLIB}" \
    LDFLAGS_FOR_BUILD="-L${TC_BUILD_BINUTILS_LIBERTY}/libiberty/" \
    LDFLAGS="")


  case ${SB_ARCH} in
    i686)      BINUTILS_SB_ELF_TARGETS=i686-pc-nacl ;;
    x86_64)    BINUTILS_SB_ELF_TARGETS=x86_64-pc-nacl ;;
    armv7)     BINUTILS_SB_ELF_TARGETS=arm-pc-nacl ;;
    universal) BINUTILS_SB_ELF_TARGETS=arm-pc-nacl,i686-pc-nacl,x86_64-pc-nacl ;;
  esac
}

#+-------------------------------------------------------------------------
#+ binutils-sb <arch> <mode> - Build and install binutils (sandboxed)
binutils-sb() {
  binutils-sb-setup "$@"
  StepBanner "BINUTILS-SB" "Sandboxed ld [${SB_LABEL}]"

  local srcdir="${TC_SRC_BINUTILS}"
  assert-dir "${srcdir}" "You need to checkout binutils."

  if binutils-sb-needs-configure ; then
    binutils-sb-clean
    binutils-sb-configure
  else
    SkipBanner "BINUTILS-SB" "configure ${SB_ARCH}"
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
  local installdir="${SB_INSTALL_DIR}"

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
        --without-gas \
        --disable-plugins \
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
  local objdir="${BINUTILS_SB_OBJDIR}"
  local installbin="${SB_INSTALL_DIR}/bin"

  StepBanner "BINUTILS-SB" "Install [${installbin}]"

  mkdir -p "${installbin}"
  spushd "${installbin}"

  # Install just "ld"
  cp "${objdir}"/ld/ld-new ld.pexe

  # Translate and install
  translate-sb-tool ld
  install-sb-tool ld
  spopd
}

#+-------------------------------------------------------------------------
#+ binutils-gold - Build and install gold (unsandboxed)
#+                 This is the replacement for the current
#+                 final linker which is bfd based.
#+                 It has nothing to do with the bitcode linker
#+                 which is also gold based.
binutils-gold() {
  StepBanner "GOLD-NATIVE (libiberty + gold)"

  local srcdir="${TC_SRC_GOLD}"
  assert-dir "${srcdir}" "You need to checkout gold."

  binutils-gold-clean
  binutils-gold-configure
  binutils-gold-make
  binutils-gold-install
}

# binutils-gold-clean - Clean gold
binutils-gold-clean() {
  StepBanner "GOLD-NATIVE" "Clean"
  local objdir="${TC_BUILD_GOLD}"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"
}

# binutils-gold-configure - Configure binutils for gold (unsandboxed)
binutils-gold-configure() {
  local srcdir="${TC_SRC_GOLD}"
  local objdir="${TC_BUILD_GOLD}"

  local flags="-fno-exceptions"
  StepBanner "GOLD-NATIVE" "Configure (libiberty)"
  # Gold depends on liberty only for a few functions:
  # xrealloc, lbasename, etc.
  # we could remove these if necessary
  mkdir -p "${objdir}/libiberty"
  spushd "${objdir}/libiberty"
  RunWithLog gold.configure \
    env -i \
    PATH="/usr/bin:/bin" \
    CC="${CC} ${flags}" \
    CXX="${CXX} ${flags}" \
    ${srcdir}/libiberty/configure --prefix="${BINUTILS_INSTALL_DIR}"

  spopd

  StepBanner "GOLD-NATIVE" "Configure (gold)"
  # NOTE: we are still building one unnecessary target: "32bit big-endian"
  # which is dragged in by targ_extra_big_endian=true in
  # pnacl/src/gold/gold/configure.tgt
  # removing it causes undefined symbols during linking of gold.
  # The potential savings are guesstimated to be 300kB in binary size
  local gold_targets="i686-pc-nacl,x86_64-pc-nacl,arm-pc-nacl"

  mkdir -p "${objdir}/gold"
  spushd "${objdir}/gold"
  RunWithLog gold.configure \
    env -i \
    PATH="/usr/bin:/bin" \
    CC="${CC}" \
    CXX="${CXX}" \
    ac_cv_search_zlibVersion=no \
    ac_cv_header_sys_mman_h=no \
    ac_cv_func_mmap=no \
    ac_cv_func_mallinfo=no \
    ${srcdir}/gold/configure --prefix="${BINUTILS_INSTALL_DIR}" \
                                      --enable-targets=${gold_targets} \
                                      --disable-nls \
                                      --enable-plugins=no \
                                      --disable-werror \
                                      --with-sysroot="${NONEXISTENT_PATH}"
  # Note: the extra ac_cv settings:
  # * eliminate unnecessary use of zlib
  # * eliminate use of mmap
  # (those should not have much impact on the non-sandboxed
  # version but help in the sandboxed case)

  # There's no point in setting the correct path as sysroot, because we
  # want the toolchain to be relocatable. The driver will use ld command-line
  # option --sysroot= to override this value and set it to the correct path.
  # However, we need to include --with-sysroot during configure to get this
  # option. So fill in a non-sense, non-existent path.
  spopd
}

# binutils-gold-make - Make binutils (unsandboxed)
binutils-gold-make() {
  local objdir="${TC_BUILD_GOLD}"
  ts-touch-open "${objdir}/"

  StepBanner "GOLD-NATIVE" "Make (liberty)"
  spushd "${objdir}/libiberty"

  RunWithLog gold.make \
      env -i PATH="/usr/bin:/bin" \
      make ${MAKE_OPTS}
  spopd

  StepBanner "GOLD-NATIVE" "Make (gold)"
  spushd "${objdir}/gold"
  RunWithLog gold.make \
      env -i PATH="/usr/bin:/bin" \
      make ${MAKE_OPTS} ld-new${EXEC_EXT}
  spopd

  ts-touch-commit "${objdir}"
}

# binutils-gold-install - Install gold
binutils-gold-install() {
  StepBanner "GOLD-NATIVE" "Install [${BINUTILS_INSTALL_DIR}]"
  local src=${TC_BUILD_GOLD}/gold/ld-new
  local dst=${BINUTILS_INSTALL_DIR}/bin/arm-pc-nacl-ld.alt
  # Note, the "*" is for windows where ld-new is actually ld-new.exe
  ls -l  ${src}*
  # Note, this does the right thing on windows:
  # "cp" has built-in smarts to deal with the ".exe" extension of ld-new
  cp ${src} ${dst}
}

### Sandboxed version of gold.

#+-------------------------------------------------------------------------
#+ binutils-gold-sb - Build and install gold (sandboxed)
#+                 This is the replacement for the current
#+                 final linker which is bfd based.
#+                 It has nothing to do with the bitcode linker
#+                 which is also gold based.
binutils-gold-sb() {
  StepBanner "GOLD-NATIVE-SB (libiberty + gold)"

  local srcdir="${TC_SRC_GOLD}"
  assert-dir "${srcdir}" "You need to checkout gold."

  binutils-gold-sb-setup "$@"
  binutils-gold-sb-clean
  binutils-gold-sb-configure
  binutils-gold-sb-make
  binutils-gold-sb-install
}

# Internal setup function for binutils-sb-gold.
# This defines a few global variables like the objdir.
BINUTILS_GOLD_SB_SETUP=false
binutils-gold-sb-setup() {
  sb-setup "$@"

  if ${BINUTILS_GOLD_SB_SETUP}; then
    return 0
  fi
  BINUTILS_GOLD_SB_SETUP=true
  BINUTILS_GOLD_SB_LOG_PREFIX="binutils-gold.sb.${SB_LOG_PREFIX}"
  BINUTILS_GOLD_SB_OBJDIR="${SB_OBJDIR}/binutils-gold-sb"

  local flags="-static -DNACL_SRPC -fno-exceptions"

  BINUTILS_GOLD_SB_CONFIGURE_ENV=(
    AR="${PNACL_AR}" \
    AS="${PNACL_AS}" \
    CC="$(GetTool cc ${SB_LIBMODE}) ${flags}" \
    CXX="$(GetTool cxx ${SB_LIBMODE}) ${flags}" \
    CC_FOR_BUILD="${CC}" \
    CXX_FOR_BUILD="${CXX}" \
    LD="$(GetTool ld ${SB_LIBMODE}) ${flags}" \
    NM="${PNACL_NM}" \
    RANLIB="${PNACL_RANLIB}"
  )

  case ${SB_ARCH} in
    i686)      BINUTILS_GOLD_SB_ELF_TARGETS=i686-pc-nacl ;;
    x86_64)    BINUTILS_GOLD_SB_ELF_TARGETS=x86_64-pc-nacl ;;
    armv7)     BINUTILS_GOLD_SB_ELF_TARGETS=arm-pc-nacl ;;
    universal)
      BINUTILS_GOLD_SB_ELF_TARGETS=i686-pc-nacl,x86_64-pc-nacl,arm-pc-nacl ;;
  esac
}


# binutils-gold-sb-clean - Clean gold
binutils-gold-sb-clean() {
  StepBanner "GOLD-NATIVE-SB" "Clean"
  local objdir="${BINUTILS_GOLD_SB_OBJDIR}"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"
}

# binutils-gold-sb-configure - Configure binutils for gold (unsandboxed)
binutils-gold-sb-configure() {
  local srcdir="${TC_SRC_GOLD}"
  local objdir="${BINUTILS_GOLD_SB_OBJDIR}"
  local installbin="${SB_INSTALL_DIR}/bin"

  StepBanner "GOLD-NATIVE-SB" "Configure (libiberty)"
  # Gold depends on liberty only for a few functions:
  # xrealloc, lbasename, etc.
  # we could remove these if necessary.
  # TODO(jvoung): fix the --host= and --target= ... they should be the same.

  mkdir -p "${objdir}/libiberty"
  spushd "${objdir}/libiberty"
  RunWithLog "${BINUTILS_GOLD_SB_LOG_PREFIX}".configure \
    env -i \
    PATH="/usr/bin:/bin" \
    "${BINUTILS_GOLD_SB_CONFIGURE_ENV[@]}" \
    ${srcdir}/libiberty/configure --prefix="${installbin}" \
    --host=i686-pc-nacl \
    --target=i686-pc-nacl
  spopd


  StepBanner "GOLD-NATIVE-SB" "Configure (gold)"
  # NOTE: we are still building one unnecessary target: "32bit big-endian"
  # which is dragged in by targ_extra_big_endian=true in
  # pnacl/src/gold/gold/configure.tgt
  # removing it causes undefined symbols during linking of gold.
  # The potential savings are guesstimated to be 300kB in binary size
  local gold_targets=${BINUTILS_GOLD_SB_ELF_TARGETS}
  # TODO(jvoung): fix the --host= and --target= ... they should be the same.

  mkdir -p "${objdir}/gold"
  spushd "${objdir}/gold"
  RunWithLog "${BINUTILS_GOLD_SB_LOG_PREFIX}".configure \
    env -i \
    PATH="/usr/bin:/bin" \
    "${BINUTILS_GOLD_SB_CONFIGURE_ENV[@]}" \
    CXXFLAGS="-Werror" \
    CFLAGS="-Werror" \
    ac_cv_search_zlibVersion=no \
    ac_cv_header_sys_mman_h=no \
    ac_cv_func_mmap=no \
    ac_cv_func_mallinfo=no \
    ac_cv_prog_cc_g=no \
    ac_cv_prog_cxx_g=no \
    ${srcdir}/gold/configure --prefix="${installbin}" \
                                      --enable-targets=${gold_targets} \
                                      --host=i686-pc-nacl \
                                      --target=i686-pc-nacl \
                                      --disable-nls \
                                      --enable-plugins=no \
                                      --enable-naclsrpc=yes \
                                      --disable-werror \
                                      --with-sysroot="${NONEXISTENT_PATH}"
  # Note: the extra ac_cv settings:
  # * eliminate unnecessary use of zlib
  # * eliminate use of mmap
  # (those should not have much impact on the non-sandboxed
  # version but help in the sandboxed case)
  # We also disable debug (-g) because the bitcode files become too big
  # (with ac_cv_prog_cc_g).

  # There's no point in setting the correct path as sysroot, because we
  # want the toolchain to be relocatable. The driver will use ld command-line
  # option --sysroot= to override this value and set it to the correct path.
  # However, we need to include --with-sysroot during configure to get this
  # option. So fill in a non-sense, non-existent path.
  spopd
}

# binutils-gold-sb-make - Make binutils (unsandboxed)
binutils-gold-sb-make() {
  local objdir="${BINUTILS_GOLD_SB_OBJDIR}"
  ts-touch-open "${objdir}/"

  StepBanner "GOLD-NATIVE-SB" "Make (liberty)"
  spushd "${objdir}/libiberty"

  RunWithLog "${BINUTILS_GOLD_SB_LOG_PREFIX}".make \
      env -i PATH="/usr/bin:/bin" \
      make ${MAKE_OPTS}
  spopd

  StepBanner "GOLD-NATIVE-SB" "Make (gold)"
  spushd "${objdir}/gold"
  RunWithLog "${BINUTILS_GOLD_SB_LOG_PREFIX}".make \
      env -i PATH="/usr/bin:/bin" \
      make ${MAKE_OPTS} ld-new
  spopd

  ts-touch-commit "${objdir}"
}

# binutils-gold-sb-install - Install gold
binutils-gold-sb-install() {
  local objdir="${BINUTILS_GOLD_SB_OBJDIR}"
  local installbin="${SB_INSTALL_DIR}/bin"

  StepBanner "GOLD-NATIVE-SB" "Install [${installbin}]"

  mkdir -p "${installbin}"
  spushd "${installbin}"

  # Install just "ld"
  cp "${objdir}"/gold/ld-new ld-gold.pexe

  # Translate and install
  translate-sb-tool ld-gold
  install-sb-tool ld-gold
  spopd
}


#+-------------------------------------------------------------------------
#########################################################################
#     < NEWLIB-BITCODE >
#########################################################################

#+ newlib - Build and install newlib in bitcode.
newlib() {
  StepBanner "NEWLIB (BITCODE)"

  # TODO(pdox): Why is this step needed?
  sysroot

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
    ${srcdir}/configure \
        --disable-multilib \
        --prefix="${NEWLIB_INSTALL_DIR}" \
        --disable-newlib-supplied-syscalls \
        --disable-texinfo \
        --disable-libgloss \
        --enable-newlib-iconv \
        --enable-newlib-io-long-long \
        --enable-newlib-io-long-double \
        --enable-newlib-io-c99-formats \
        --enable-newlib-mb \
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
  spopd

  # Newlib installs files into usr/${REAL_CROSS_TARGET}/*
  # Get rid of the ${REAL_CROSS_TARGET}/ prefix.
  pushd "${NEWLIB_INSTALL_DIR}"
  mkdir -p lib include
  mv -f ${REAL_CROSS_TARGET}/lib/* lib
  rm -rf  include/sys include/machine
  mv -f ${REAL_CROSS_TARGET}/include/* include
  rmdir ${REAL_CROSS_TARGET}/lib
  rmdir ${REAL_CROSS_TARGET}/include
  rmdir ${REAL_CROSS_TARGET}

  StepBanner "NEWLIB" "Extra-install"
  local sys_include=${SYSROOT_DIR}/include
  pushd "${NEWLIB_INSTALL_DIR}"
  # NOTE: we provide a new pthread.h via extra-sdk
  rm include/pthread.h
  cp include/machine/endian.h ${sys_include}
  cp include/sys/param.h ${sys_include}
  cp include/newlib.h ${sys_include}
  popd

  # NOTE: we provide our own pthread.h via extra-sdk
  StepBanner "NEWLIB" "Removing old pthreads headers"
  rm -f "${sys_include}/pthread.h"

  # Clang claims posix thread model, not single as llvm-gcc does.
  # It means that libstdcpp needs pthread.h to be in place.
  # This should go away when we properly import pthread.h with
  # the other newlib headers. This hack is tracked by
  # http://code.google.com/p/nativeclient/issues/detail?id=2333
  StepBanner "NEWLIB" "Copying pthreads headers ahead of time "\
  "(HACK. See http://code.google.com/p/nativeclient/issues/detail?id=2333)"
  sdk-headers newlib
}

libs-support() {
  StepBanner "LIBS-SUPPORT"
  libs-support-newlib
  libs-support-bitcode
  local arch
  for arch in arm x86-32 x86-64; do
    libs-support-native ${arch}
  done
}

libs-support-newlib() {
  mkdir -p "${INSTALL_LIB_NEWLIB}"
  spushd "${PNACL_SUPPORT}"
  # Install crt1.x (linker script)
  StepBanner "LIBS-SUPPORT-NEWLIB" "Install crt1.x (linker script)"
  cp crt1.x "${INSTALL_LIB_NEWLIB}"/crt1.x
  spopd
}

libs-support-bitcode() {
  local build_dir="${TC_BUILD}/libs-support-bitcode"
  local cc_cmd="${PNACL_CC_NEUTRAL} -no-save-temps"

  mkdir -p "${build_dir}"
  spushd "${PNACL_SUPPORT}/bitcode"
  # Install crti.bc (empty _init/_fini)
  StepBanner "LIBS-SUPPORT" "Install crti.bc"
  ${cc_cmd} -c crti.c -o "${build_dir}"/crti.bc

  # Install crtbegin bitcode (__dso_handle/__cxa_finalize for C++)
  StepBanner "LIBS-SUPPORT" "Install crtbegin.bc / crtbeginS.bc"
  ${cc_cmd} -c crtdummy.c -o "${build_dir}"/crtdummy.bc
  ${cc_cmd} -c crtbegin.c -o "${build_dir}"/crtbegin.bc
  ${cc_cmd} -c crtbegin.c -o "${build_dir}"/crtbeginS.bc \
            -DSHARED

  # Install pnacl_abi.bc
  # (NOTE: This does a trivial bitcode link to set the right metadata)
  StepBanner "LIBS-SUPPORT" "Install pnacl_abi.bc (stub pso)"
  ${cc_cmd} -Wno-builtin-requires-header -nostdlib -shared \
            -Wl,-soname="" pnacl_abi.c -o "${build_dir}"/pnacl_abi.bc

  spopd

  # Install to both newlib and glibc lib directories
  spushd "${build_dir}"
  local files="crti.bc crtdummy.bc crtbegin.bc crtbeginS.bc pnacl_abi.bc"
  mkdir -p "${INSTALL_LIB_NEWLIB}"
  cp -f ${files} "${INSTALL_LIB_NEWLIB}"
  mkdir -p "${INSTALL_LIB_GLIBC}"
  cp -f ${files} "${INSTALL_LIB_GLIBC}"
  spopd
}

libs-support-native() {
  local arch=$1
  local destdir="${INSTALL_LIB_NATIVE}"${arch}
  local label="LIBS-SUPPORT (${arch})"
  mkdir -p "${destdir}"

  local flags="--pnacl-allow-native --pnacl-allow-translate -no-save-temps"
  local cc_cmd="${PNACL_CC_NEUTRAL} -arch ${arch} ${flags}"

  spushd "${PNACL_SUPPORT}"

  # Compile crtbegin.o / crtend.o
  StepBanner "${label}" "Install crtbegin.o / crtend.o"
  ${cc_cmd} -c crtbegin.c -o "${destdir}"/crtbegin.o
  ${cc_cmd} -c crtend.c -o "${destdir}"/crtend.o

  # Compile crtbeginS.o / crtendS.o
  StepBanner "${label}" "Install crtbeginS.o / crtendS.o"
  ${cc_cmd} -c crtbegin.c -fPIC -DSHARED -o "${destdir}"/crtbeginS.o
  ${cc_cmd} -c crtend.c -fPIC -DSHARED -o "${destdir}"/crtendS.o

  # Make libcrt_platform.a
  StepBanner "${label}" "Install libcrt_platform.a"
  local tmpdir="${TC_BUILD}/libs-support-native"
  rm -rf "${tmpdir}"
  mkdir -p "${tmpdir}"
  ${cc_cmd} -c setjmp_${arch/-/_}.S -o "${tmpdir}"/setjmp.o

  # For ARM, also compile aeabi_read_tp.S
  if  [ ${arch} == arm ] ; then
    ${cc_cmd} -c aeabi_read_tp.S -o "${tmpdir}"/aeabi_read_tp.o
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
            disable_nosys_linker_warnings=1
            naclsdk_validate=0
            --verbose)

SDK_IS_SETUP=false
sdk-setup() {
  if ${SDK_IS_SETUP} && [ $# -eq 0 ]; then
    return 0
  fi
  if [ $# -ne 1 ]; then
    Fatal "Please specify libmode: newlib or glibc"
  fi
  check-libmode "$1"
  SDK_IS_SETUP=true
  SDK_LIBMODE=$1

  SDK_INSTALL_ROOT="${INSTALL_ROOT}/${SDK_LIBMODE}/sdk"
  SDK_INSTALL_LIB="${SDK_INSTALL_ROOT}/lib"
  SDK_INSTALL_INCLUDE="${SDK_INSTALL_ROOT}/include"
}

sdk() {
  sdk-setup "$@"
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
  sdk-setup "$@"
  StepBanner "SDK" "Clean"
  rm -rf "${SDK_INSTALL_ROOT}"

  # clean scons obj dirs
  rm -rf "${SCONS_OUT}"/nacl-*-pnacl*
}

sdk-headers() {
  sdk-setup "$@"
  mkdir -p "${SDK_INSTALL_INCLUDE}"

  local extra_flags=""
  local neutral_platform="x86-32"
  if [ ${SDK_LIBMODE} == "glibc" ]; then
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
      includedir="$(PosixToSysPath "${SDK_INSTALL_INCLUDE}")"
  spopd
}

sdk-libs() {
  sdk-setup "$@"
  StepBanner "SDK" "Install libraries"
  mkdir -p "${SDK_INSTALL_LIB}"

  local extra_flags=""
  local neutral_platform="x86-32"
  if [ ${SDK_LIBMODE} == "glibc" ]; then
    extra_flags="--nacl_glibc"
  fi

  spushd "${NACL_ROOT}"
  RunWithLog "sdk.libs.bitcode" \
      ./scons \
      "${SCONS_ARGS[@]}" \
      ${extra_flags} \
      platform=${neutral_platform} \
      install_lib \
      libdir="$(PosixToSysPath "${SDK_INSTALL_LIB}")"
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
            pnacl_generate_pexe=0 \
            --verbose \
            pnacl_irt_shim
  local out_dir_prefix="${SCONS_OUT}"/nacl-x86-64-pnacl-clang
  local outdir="${out_dir_prefix}"/obj/src/untrusted/pnacl_irt_shim
  mkdir -p "${INSTALL_LIB_X8664}"
  cp "${outdir}"/libpnacl_irt_shim.a "${INSTALL_LIB_X8664}"
  spopd
}

sdk-verify() {
  sdk-setup "$@"
  StepBanner "SDK" "Verify"

  # Verify bitcode libraries
  verify-bitcode-dir "${SDK_INSTALL_LIB}"
}

newlib-nacl-headers-clean() {
  # Clean the include directory and revert it to its pure state
  if [ -d "${TC_SRC_NEWLIB}" ]; then
    rm -rf "${NEWLIB_INCLUDE_DIR}"
    # If the script is interrupted right here,
    # then NEWLIB_INCLUDE_DIR will not exist, and the repository
    # will be in a bad state. This will be fixed during the next
    # invocation by newlib-nacl-headers.

    spushd "$(dirname "${NEWLIB_INCLUDE_DIR}")"
    RunWithLog "newlib-nacl-headers-clean" \
      git checkout ${NEWLIB_INCLUDE_DIR}
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
  if ! git-has-changes "${NEWLIB_INCLUDE_DIR}" ; then
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

#+ ppapi-headers
#+ Note, this is experimental for now and must be called manually to avoid
#+ any unexpected interference with where scons picks up the ppapi headers
#+ today.
# TODO(robertm): harmonize this with whatever we do for the nacl-gcc TC
ppapi-headers() {
  ppapi-headers-one newlib
  ppapi-headers-one glibc
}

ppapi-headers-one() {
  local libmode=$1
  check-libmode ${libmode}
  local sdkinclude="$(GetInstallDir ${libmode})"/sdk/include
  local dst="${sdkinclude}"/ppapi
  StepBanner "PPAPI-HEADERS" "${dst}"
  # This is quick and dirty 1st cut.
  local ppapi_base=${NACL_ROOT}/../ppapi
  rm -rf ${dst}
  mkdir -p ${dst}
  cp -r ${ppapi_base}/c            ${dst}
  cp -r ${ppapi_base}/cpp          ${dst}
  cp -r ${ppapi_base}/utility      ${dst}
  cp -r ${ppapi_base}/lib/gl/gles2 ${dst}

  # pruning (needs a lot more work)
  rm -rf ${dst}/*/private
  rm -rf ${dst}/*/trusted
  rm -rf ${dst}/*/documentation
  rm -rf ${dst}/*/.svn

  rm -f ${dst}/*/*.c
  rm -f ${dst}/*/*.cc
  rm -f ${dst}/*/*/*.cc

  cp -r ${ppapi_base}/lib/gl/include/KHR   "${sdkinclude}"
  cp -r ${ppapi_base}/lib/gl/include/EGL   "${sdkinclude}"
  cp -r ${ppapi_base}/lib/gl/include/GLES2 "${sdkinclude}"
}
#+-------------------------------------------------------------------------
#@ driver                - Install driver scripts.
driver() {
  StepBanner "DRIVER"
  driver-install newlib
  driver-install glibc
}

# install python scripts and redirector shell/batch scripts
driver-install-python() {
  local destdir="$1"
  shift
  local pydir="${destdir}/pydir"

  StepBanner "DRIVER" "Installing driver adaptors to ${destdir}"
  mkdir -p "${destdir}"
  mkdir -p "${pydir}"
  rm -f "${destdir}"/pnacl-*

  spushd "${DRIVER_DIR}"

  # Copy python scripts
  cp $@ driver_log.py driver_env.py *tools.py loader.py "${pydir}"

  # Install redirector shell/batch scripts
  cp findpython.sh "${destdir}"
  for name in $@; do
    local dest="${destdir}/${name/.py}"
    cp redirect.sh "${dest}"
    chmod +x "${dest}"
    if ${BUILD_PLATFORM_WIN}; then
      cp redirect.bat "${dest}".bat
    fi
  done
  spopd
}

# The driver is a simple python script which changes its behavior
# depending on the name it is invoked as.
driver-install() {
  local libmode=$1
  check-libmode ${libmode}
  local destdir="${INSTALL_ROOT}/${libmode}/bin"

  driver-install-python "${destdir}" "pnacl-*.py"

  # Tell the driver the library mode
  echo "LIBMODE=${libmode}" > "${destdir}"/driver.conf

  # Install readelf and size
  cp -a "${BINUTILS_INSTALL_DIR}/bin/${BINUTILS_TARGET}-readelf" \
        "${destdir}/readelf"
  cp -a "${BINUTILS_INSTALL_DIR}/bin/${BINUTILS_TARGET}-size" \
        "${destdir}/size"

  # On windows, copy the cygwin DLLs needed by the driver tools
  if ${BUILD_PLATFORM_WIN}; then
    StepBanner "DRIVER" "Copying cygwin libraries"
    local deps="gcc_s-1 iconv-2 win1 intl-8 stdc++-6 z"
    for name in ${deps}; do
      cp "/bin/cyg${name}.dll" "${destdir}"
    done
  fi
}

#@ driver-install-translator - Install driver scripts for translator component
driver-install-translator() {
  local destdir="${INSTALL_TRANSLATOR}/bin"

  driver-install-python "${destdir}" pnacl-translate.py pnacl-nativeld.py

  # Translator is newlib
  echo "LIBMODE=newlib" > "${destdir}"/driver.conf
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
  # Use preprocessor to strip the C-style comments.
  ${PNACL_PP} -xc "${archive}" | awk -v archive="$(basename ${archive})" '
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
  # llvm-mc does not automatically insert these tags (unlike gnu-as).
  # So we exclude llvm-mc generated object files for now
  if [[ $1 == aeabi_read_tp.o || $1 == setjmp.o ]] ; then
    return
  fi
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
    echo "PASS (bitcode pso)"
    # TODO(pdox): Add a call to pnacl-meta to check for the "shared" property.
  fi
}

#+ verify-bitcode-dir    - Verify that the files in a directory are bitcode.
verify-bitcode-dir() {
  local dir="$1"
  # This avoids errors when * finds no matches.
  shopt -s nullglob
  SubBanner "VERIFY: ${dir}"
  for i in "${dir}"/*.a ; do
    verify-archive-llvm "$i"
  done
  for i in "${dir}"/*.pso ; do
    verify-pso "$i"
  done
  for i in "${dir}"/*.bc ; do
    echo -n "verify $(basename "$i"): "
    verify-object-llvm "$i"
    echo "PASS (bitcode)"
  done
  for i in "${dir}"/*.o ; do
    Fatal "Native object file $i inside bitcode directory"
  done
  shopt -u nullglob
}


#+ verify-native-dir     - Verify that files in a directory are native for arch.
verify-native-dir() {
  local arch="$1"
  local dir="$2"

  SubBanner "VERIFY: ${dir}"

  # This avoids errors when * finds no matches.
  shopt -s nullglob
  for i in "${dir}"/*.o ; do
    verify-object-${arch} "$i"
  done

  for i in "${dir}"/*.a ; do
    verify-archive-${arch} "$i"
  done

  for i in "${dir}"/*.bc "${dir}"/*.pso ; do
    Fatal "Bitcode file $i found inside native directory"
  done
  shopt -u nullglob
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
  verify-bitcode
  verify-native
}

verify-bitcode() {
  verify-bitcode-dir "${INSTALL_LIB_NEWLIB}"
  verify-bitcode-dir "${INSTALL_LIB_GLIBC}"
}

verify-native() {
  local arch
  for arch in arm x86-32 x86-64; do
    verify-native-dir ${arch} "${INSTALL_LIB_NATIVE}${arch}"
  done
}

#+ verify-triple-build <arch> <srpcmode> <libmode>
#+     Verify that the sandboxed translator produces an identical
#+     translation of itself (llc.pexe) as the unsandboxed translator.
#+     (NOTE: This function is experimental/untested)
verify-triple-build() {
  sb-setup "$@"
  StepBanner "VERIFY" "Verifying triple build for ${SB_LABEL}"

  local arch="${SB_ARCH}"
  local bindir="${SB_INSTALL_DIR}/bin"
  local llc_nexe="${bindir}/llc.nexe"
  local llc_pexe="${bindir}/llc.pexe"
  assert-file "${llc_nexe}" "sandboxed llc for ${arch} does not exist"
  assert-file "${llc_pexe}" "llc.pexe does not exist"

  local flags="--pnacl-sb --pnacl-driver-verbose"
  if [ ${SB_SRPCMODE} == "srpc" ] ; then
    flags+=" --pnacl-driver-set-SRPC=1"
  else
    flags+=" --pnacl-driver-set-SRPC=0"
  fi

  if [ ${arch} == "arm" ] ; then
    # Use emulator if we are not on ARM
    local hostarch=$(uname -m)
    if ! [[ "${BUILD_ARCH}" =~ arm ]]; then
      flags+=" --pnacl-use-emulator"
    fi
  fi

  local objdir="${SB_INSTALL_DIR}/triple-build"
  local new_llc_nexe="${objdir}/llc.rebuild.nexe"
  mkdir -p "${objdir}"
  StepBanner "VERIFY" "Translating ${llc_pexe} using sandboxed tools (${arch})"
  RunWithLog "verify.triple.build" \
    "${PNACL_TRANSLATE}" ${flags} -arch ${arch} "${llc_pexe}" \
                         -o "${new_llc_nexe}"

  if ! cmp --silent "${llc_nexe}" "${new_llc_nexe}" ; then
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
  echo "INSTALL_ROOT:      ${INSTALL_ROOT}"
  Banner "Your Environment:"
  env | grep PNACL
  Banner "uname info for builder:"
  uname -a
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
#
# This is called from install-sb-tool on the final nexes.
print-size-of-sb-tool() {
  local arch=$1
  local toolname=$2
  local tool_size_string=\
$(${PNACL_SIZE} -B "${toolname}.nexe" | grep '[0-9]\+')

  set -- ${tool_size_string}
  echo "RESULT ${toolname}_${arch}_size: text= $1 bytes"
  echo "RESULT ${toolname}_${arch}_size: data= $2 bytes"
  echo "RESULT ${toolname}_${arch}_size: bss= $3 bytes"
  echo "RESULT ${toolname}_${arch}_size: total= $4 bytes"
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
