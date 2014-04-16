#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#@                 PNaCl toolchain build script
#@-------------------------------------------------------------------
#@ This script builds the ARM and PNaCl untrusted toolchains.
#@ It MUST be run from the native_client/ directory.
######################################################################
# Directory Layout Description
######################################################################
#
# All directories are relative to BASE which is
# On Linux: native_client/toolchain/linux_x86/pnacl_newlib/
# On Mac: native_client/toolchain/mac_x86/pnacl_newlib/
# On Windows: native_client/toolchain/win_x86/pnacl_newlib/
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

readonly TOOLCHAIN_BUILD="${NACL_ROOT}/toolchain_build/toolchain_build_pnacl.py"

# For different levels of make parallelism change this in your env
readonly PNACL_CONCURRENCY=${PNACL_CONCURRENCY:-8}
# Concurrency for builds using the host's system compiler (which might be goma)
readonly PNACL_CONCURRENCY_HOST=${PNACL_CONCURRENCY_HOST:-${PNACL_CONCURRENCY}}
PNACL_PRUNE=${PNACL_PRUNE:-false}
PNACL_BUILD_ARM=true
PNACL_BUILD_MIPS=${PNACL_BUILD_MIPS:-false}

if ${BUILD_PLATFORM_MAC} || ${BUILD_PLATFORM_WIN}; then
  # We don't yet support building ARM tools for mac or windows.
  PNACL_BUILD_ARM=false
  PNACL_BUILD_MIPS=false
fi

# PNaCl builds libc++/libc++abi as well as libstdc++, allowing users to
# choose which to use through the -stdlib=XXX command-line argument.
#
# The following strings are used for banner names as well as file and
# folder names. These names are created by the libraries themselves, and
# expected by their dependents. Changing them would be ill-advised.
readonly LIB_CXX_NAME="libc++"
readonly LIB_STDCPP_NAME="libstdc++"

# TODO(pdox): Decide what the target should really permanently be
readonly CROSS_TARGET_ARM=arm-none-linux-gnueabi
readonly BINUTILS_TARGET=arm-pc-nacl
readonly REAL_CROSS_TARGET=le32-nacl
readonly NACL64_TARGET=x86_64-nacl

readonly DRIVER_DIR="${PNACL_ROOT}/driver"
readonly ARM_ARCH=armv7-a
readonly ARM_FPU=vfp

readonly TOOLCHAIN_ROOT="${NACL_ROOT}/toolchain"
readonly TOOLCHAIN_BASE="${TOOLCHAIN_ROOT}/${SCONS_BUILD_PLATFORM}_x86"

readonly NNACL_NEWLIB_ROOT="${TOOLCHAIN_BASE}/nacl_x86_newlib"
readonly NNACL_ARM_NEWLIB_ROOT="${TOOLCHAIN_BASE}/nacl_arm_newlib"

readonly PNACL_MAKE_OPTS="${PNACL_MAKE_OPTS:-}"
readonly MAKE_OPTS="-j${PNACL_CONCURRENCY} VERBOSE=1 ${PNACL_MAKE_OPTS}"
readonly MAKE_OPTS_HOST="-j${PNACL_CONCURRENCY_HOST} VERBOSE=1 ${PNACL_MAKE_OPTS}"

readonly NONEXISTENT_PATH="/going/down/the/longest/road/to/nowhere"

# For speculative build status output. ( see status function )
# Leave this blank, it will be filled during processing.
SPECULATIVE_REBUILD_SET=""

readonly PNACL_SUPPORT="${PNACL_ROOT}/support"

readonly THIRD_PARTY="${NACL_ROOT}"/../third_party
readonly NACL_SRC_THIRD_PARTY_MOD="${NACL_ROOT}/src/third_party_mod"

# Git sources
readonly PNACL_GIT_ROOT="${PNACL_ROOT}/git"
readonly TC_SRC_BINUTILS="${PNACL_GIT_ROOT}/binutils"
readonly TC_SRC_LLVM="${PNACL_GIT_ROOT}/llvm"
readonly TC_SRC_GCC="${PNACL_GIT_ROOT}/gcc"
readonly TC_SRC_NEWLIB="${PNACL_GIT_ROOT}/nacl-newlib"
readonly TC_SRC_LIBSTDCPP="${TC_SRC_GCC}/${LIB_STDCPP_NAME}-v3"
readonly TC_SRC_COMPILER_RT="${PNACL_GIT_ROOT}/compiler-rt"
readonly TC_SRC_CLANG="${PNACL_GIT_ROOT}/clang"
readonly TC_SRC_LIBCXX="${PNACL_GIT_ROOT}/libcxx"

readonly SERVICE_RUNTIME_SRC="${NACL_ROOT}/src/trusted/service_runtime"
readonly EXPORT_HEADER_SCRIPT="${SERVICE_RUNTIME_SRC}/export_header.py"
readonly NACL_SYS_HEADERS="${SERVICE_RUNTIME_SRC}/include"
readonly NEWLIB_INCLUDE_DIR="${TC_SRC_NEWLIB}/newlib/libc/sys/nacl"

# The location of each project. These should be absolute paths.
readonly TC_BUILD="${PNACL_ROOT}/build"
readonly TC_BUILD_LLVM="${TC_BUILD}/llvm_${HOST_ARCH}"
readonly TC_BUILD_BINUTILS="${TC_BUILD}/binutils_${HOST_ARCH}"
readonly TC_BUILD_BINUTILS_LIBERTY="${TC_BUILD}/binutils-liberty"
TC_BUILD_NEWLIB="${TC_BUILD}/newlib"
readonly TC_BUILD_COMPILER_RT="${TC_BUILD}/compiler_rt"
readonly TC_BUILD_GCC="${TC_BUILD}/gcc"
readonly NACL_HEADERS_TS="${TC_BUILD}/nacl.sys.timestamp"

readonly TIMESTAMP_FILENAME="make-timestamp"

# PNaCl toolchain installation directories (absolute paths)
readonly INSTALL_ROOT="${TOOLCHAIN_BASE}/pnacl_newlib"
readonly INSTALL_BIN="${INSTALL_ROOT}/bin"

# Bitcode lib directories (including static bitcode libs)
INSTALL_LIB="${INSTALL_ROOT}/lib"

# Native nacl lib directories
# The pattern `${INSTALL_LIB_NATIVE}${arch}' is used in many places.
readonly INSTALL_LIB_NATIVE="${INSTALL_ROOT}/lib-"
readonly INSTALL_LIB_ARM="${INSTALL_LIB_NATIVE}arm"
readonly INSTALL_LIB_X8632="${INSTALL_LIB_NATIVE}x86-32"
readonly INSTALL_LIB_X8664="${INSTALL_LIB_NATIVE}x86-64"
readonly INSTALL_LIB_MIPS32="${INSTALL_LIB_NATIVE}mips32"

# PNaCl client-translators (sandboxed) binary locations
readonly INSTALL_TRANSLATOR="${TOOLCHAIN_BASE}/pnacl_translator"


# The INSTALL_HOST directory has host binaries and libs which
# are part of the toolchain (e.g. llvm and binutils).
# There are also tools-x86 and tools-arm which have host binaries which
# are not part of the toolchain but might be useful in the SDK, e.g.
# arm sel_ldr and x86-hosted arm/mips validators.
readonly INSTALL_HOST="${INSTALL_ROOT}/host_${HOST_ARCH}"

# Component installation directories
readonly LLVM_INSTALL_DIR="${INSTALL_HOST}"
readonly BINUTILS_INSTALL_DIR="${INSTALL_HOST}"
readonly BFD_PLUGIN_DIR="${BINUTILS_INSTALL_DIR}/lib/bfd-plugins"
readonly FAKE_INSTALL_DIR="${INSTALL_HOST}/fake"
NEWLIB_INSTALL_DIR="${INSTALL_ROOT}/usr"

# Location of the PNaCl tools defined for configure invocations.
readonly PNACL_CC="${INSTALL_BIN}/pnacl-clang"
readonly PNACL_CXX="${INSTALL_BIN}/pnacl-clang++"
readonly PNACL_LD="${INSTALL_BIN}/pnacl-ld"
readonly PNACL_PP="${INSTALL_BIN}/pnacl-clang -E"
readonly PNACL_AR="${INSTALL_BIN}/pnacl-ar"
readonly PNACL_RANLIB="${INSTALL_BIN}/pnacl-ranlib"
readonly PNACL_AS="${INSTALL_BIN}/pnacl-as"
readonly PNACL_DIS="${INSTALL_BIN}/pnacl-dis"
readonly PNACL_FINALIZE="${INSTALL_BIN}/pnacl-finalize"
readonly PNACL_NM="${INSTALL_BIN}/pnacl-nm"
readonly PNACL_TRANSLATE="${INSTALL_BIN}/pnacl-translate"
readonly PNACL_READELF="${INSTALL_BIN}/pnacl-readelf"
readonly PNACL_SIZE="${BINUTILS_INSTALL_DIR}/bin/${REAL_CROSS_TARGET}-size"
readonly PNACL_STRIP="${INSTALL_BIN}/pnacl-strip"
readonly ILLEGAL_TOOL="${INSTALL_BIN}"/pnacl-illegal

GetNNaClTool() {
  local arch=$1
  case ${arch} in
    x86-32) echo ${NNACL_NEWLIB_ROOT}/bin/i686-nacl-gcc ;;
    x86-64) echo ${NNACL_NEWLIB_ROOT}/bin/x86_64-nacl-gcc ;;
    arm) echo ${NNACL_ARM_NEWLIB_ROOT}/bin/arm-nacl-gcc ;;
    mips32)
      # No NNaCl for mips
      echo -n "${PNACL_CC} -arch mips32 --pnacl-bias=mips32 "
      echo "--pnacl-allow-translate --pnacl-allow-native" ;;
    *) Fatal "Unexpected argument to GetNNaClTool: $*" ;;
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
SBTC_PRODUCTION=${SBTC_PRODUCTION:-true}

# Which arches to build for our sandboxed toolchain.
SBTC_ARCHES_ALL=${SBTC_ARCHES_ALL:-"armv7 i686 x86_64"}

get-sbtc-llvm-arches() {
# For LLVM i686 brings in both i686 and x86_64.  De-dupe that.
  echo ${SBTC_ARCHES_ALL} \
    | sed 's/x86_64/i686/' | sed 's/i686\(.*\)i686/i686\1/'
}
SBTC_ARCHES_LLVM=$(get-sbtc-llvm-arches)


CC=${CC:-gcc}
CXX=${CXX:-g++}
AR=${AR:-ar}
RANLIB=${RANLIB:-ranlib}

if ${HOST_ARCH_X8632}; then
  # These are simple compiler wrappers to force 32bit builds
  CC="${PNACL_ROOT}/scripts/mygcc32"
  CXX="${PNACL_ROOT}/scripts/myg++32"
fi

# Set up some environment variables to build flavored bitcode libs
setup-biased-bitcode-env() {
  local arch=$1
  # Do this to avoid .S files in newlib that clang doesn't like
  NEWLIB_TARGET="${REAL_CROSS_TARGET}"
  TC_BUILD_NEWLIB="${TC_BUILD}/newlib-${arch}"
  LIB_CPP_BUILD="${TC_BUILD}/c++-stdlib-newlib-${arch}"
  case ${arch} in
    portable)
      BIASED_BC_CFLAGS=""
      NEWLIB_INSTALL_DIR="${INSTALL_ROOT}/usr"
      INSTALL_LIB="${INSTALL_ROOT}/lib"
      LIB_CPP_INSTALL_DIR="${INSTALL_ROOT}/usr"
      ;;
    arm)
      BIASED_BC_CFLAGS="--target=armv7-unknown-nacl-gnueabihf -mfloat-abi=hard"
      NEWLIB_INSTALL_DIR="${INSTALL_ROOT}/usr-bc-${arch}"
      INSTALL_LIB="${INSTALL_ROOT}/lib-bc-${arch}"
      LIB_CPP_INSTALL_DIR="${INSTALL_ROOT}/usr-bc-${arch}"
      ;;
    x86-32)
      BIASED_BC_CFLAGS="--target=i686-nacl"
      NEWLIB_INSTALL_DIR="${INSTALL_ROOT}/usr-bc-${arch}"
      INSTALL_LIB="${INSTALL_ROOT}/lib-bc-${arch}"
      LIB_CPP_INSTALL_DIR="${INSTALL_ROOT}/usr-bc-${arch}"
      ;;
    x86-64)
      BIASED_BC_CFLAGS="--target=x86_64-nacl"
      NEWLIB_INSTALL_DIR="${INSTALL_ROOT}/usr-bc-${arch}"
      INSTALL_LIB="${INSTALL_ROOT}/lib-bc-${arch}"
      LIB_CPP_INSTALL_DIR="${INSTALL_ROOT}/usr-bc-${arch}"
      ;;
    mips32)
      # MIPS doesn't use biased bitcode.
      BIASED_BC_CFLAGS=""
      NEWLIB_INSTALL_DIR="${INSTALL_ROOT}/usr-bc-${arch}"
      INSTALL_LIB="${INSTALL_ROOT}/lib-bc-${arch}"
      LIB_CPP_INSTALL_DIR="${INSTALL_ROOT}/usr-bc-${arch}"
      ;;
    *)
      echo "Newlib architecture not implemented yet"
      exit 1
  esac
}

setup-lib-cpp-env() {
  # NOTE: we do not expect the assembler or linker to be used for libs
  #       hence the use of ILLEGAL_TOOL.

  STD_ENV_FOR_LIB_CPP=(
    CC_FOR_BUILD="${CC}"
    CC="${PNACL_CC}"
    CXX="${PNACL_CXX}"
    RAW_CXX_FOR_TARGET="${PNACL_CXX}"
    LD="${ILLEGAL_TOOL}"
    CFLAGS="-g -O2 -mllvm -inline-threshold=5 ${BIASED_BC_CFLAGS}"
    CXXFLAGS="-g -O2 -mllvm -inline-threshold=5 ${BIASED_BC_CFLAGS}"
    CFLAGS_FOR_TARGET="-g -O2 -mllvm -inline-threshold=5 ${BIASED_BC_CFLAGS}"
    CXXFLAGS_FOR_TARGET="-g -O2 -mllvm -inline-threshold=5 ${BIASED_BC_CFLAGS}"
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
  local arch=$1
  setup-biased-bitcode-env ${arch}

  STD_ENV_FOR_NEWLIB=(
    # TODO(robertm): get rid of '-allow-asm' here once we have a way of
    # distinguishing "good" from "bad" asms.
    CFLAGS_FOR_TARGET="-allow-asm -g -O2 -mllvm -inline-threshold=5 ${BIASED_BC_CFLAGS}"
    CXXFLAGS_FOR_TARGET="-allow-asm -g -O2 -mllvm -inline-threshold=5 ${BIASED_BC_CFLAGS}"
    CC_FOR_TARGET="${PNACL_CC}"
    GCC_FOR_TARGET="${PNACL_CC}"
    CXX_FOR_TARGET="${PNACL_CXX}"
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

#@ sync-sources          - check out repos needed to build toolchain
sync-sources() {
  StepBanner "SYNC SOURCES"
  python ${TOOLCHAIN_BUILD} --legacy-repo-sync
  newlib-nacl-headers
}

git-sync() {
  local gitbase="${PNACL_GIT_ROOT}"
  # Disable depot tools auto-update for these invocations. (It
  # will still get updated when it is invoked at the start of each build,
  # and we don't want it updating from cygwin here)
  export DEPOT_TOOLS_UPDATE=0

  mkdir -p "${gitbase}"
  cp "${PNACL_ROOT}"/gclient_template "${gitbase}/.gclient"

  if ! [ -d "${gitbase}/dummydir" ]; then
    spushd "${gitbase}"
    ${GCLIENT} update --verbose -j1
    spopd
  fi

  newlib-nacl-headers-clean
  cp "${PNACL_ROOT}"/DEPS "${gitbase}"/dummydir
  spushd "${gitbase}"
  ${GCLIENT} update --verbose -j1
  spopd

  # Copy nacl headers into newlib tree.
  newlib-nacl-headers
}

gerrit-upload() {
  # Upload to gerrit code review
  # (Note: normally we use Rietveld code review. This is only useful if
  # e.g. we want external users to use gerrit instead of rietveld so we can
  # easily merge their commits without giving them direct access). But
  # this at least documents the specifics of uploading to our repo
  # TODO(dschuff): Find a good way to persist the user's login. maybe
  # do something like what depot tools does
  if ! [ -f ~/.gerrit_username ]; then
    echo "Enter your gerrit username: "
    read user
    echo "Saving gerrit username (${user}) to ~/.gerrit_username"
    echo ${user} > ~/.gerrit_username
  else
    local user=$(cat ~/.gerrit_username)
  fi
  cd "${TC_SRC_LLVM}"
  git push ssh://${user}@gerrit.chromium.org:29418/native_client/pnacl-llvm \
    HEAD:refs/for/master
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
  ${GCLIENT} runhooks --force --verbose
}

#@ libs                  - install native libs and build bitcode libs
libs() {
  libs-clean
  # MIPS32 doesn't use biased bitcode.
  for arch in portable arm x86-32 x86-64; do
    newlib ${arch}
  done
  libs-support
  for arch in arm x86-32 x86-64 mips32 x86-32-nonsfi; do
    dummy-irt-shim ${arch}
  done
  compiler-rt-all
  libgcc_eh-all
  # MIPS32 doesn't use biased bitcode.
  for arch in portable arm x86-32 x86-64; do
    lib-cpp ${LIB_CXX_NAME} ${arch}
    lib-cpp ${LIB_STDCPP_NAME} ${arch}
  done
}

#@ everything            - Build and install untrusted SDK. no translator
everything() {
  sync-sources
  build-all
}

#@ build-all does everything AFTER getting the sources
build-all() {
  mkdir -p "${INSTALL_ROOT}"
  # This is needed to build misc-tools and run ARM tests.
  # We check this early so that there are no surprises later, and we can
  # handle all user interaction early.
  check-for-trusted

  clean-install

  clean-logs

  build-host

  driver

  libs

  # NOTE: we delay the tool building till after the sdk is essentially
  #      complete, so that sdk sanity checks don't fail
  misc-tools
  cp "${PNACL_ROOT}/README" "${INSTALL_ROOT}"
  verify

  if ${PNACL_PRUNE}; then
    prune
  fi
}

#@ Build just the host binaries
build-host() {
  binutils
  llvm
  if ${PNACL_PRUNE}; then
    prune-host
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

#@ translator-all   -  Build and install all of the translators.
translator-all() {
  StepBanner \
    "SANDBOXED TC [prod=${SBTC_PRODUCTION}] [arches=${SBTC_ARCHES_ALL}]"

  # Build the SDK if it not already present.
  sdk
  # Also build private libs to allow building nexes without the IRT
  # segment gap.  Specifically, only the sandboxed translator nexes
  # are built without IRT support to gain address space and reduce
  # swap file usage. Also libsrpc and its dependencies are now considered
  # private libs because they are not in the real SDK
  sdk-private-libs

  binutils-liberty
  if ${SBTC_PRODUCTION}; then
    # Build each architecture separately.
    local arch
    for arch in ${SBTC_ARCHES_LLVM} ; do
      llvm-sb ${arch}
    done
    for arch in ${SBTC_ARCHES_ALL} ; do
      binutils-gold-sb ${arch}
    done
  else
    # Using arch `universal` builds the sandboxed tools from a single
    # .pexe which support all targets.
    llvm-sb universal
    binutils-gold-sb universal
    if ${PNACL_PRUNE}; then
      # The universal pexes have already been translated.
      # They don't need to stick around.
      rm -rf "$(GetTranslatorInstallDir universal)"
    fi
  fi

  # Copy native libs to translator install dir.
  cp -a ${INSTALL_LIB_NATIVE}* ${INSTALL_TRANSLATOR}

  driver-install-translator

  if ${PNACL_PRUNE}; then
    sdk-clean newlib
  fi
}


#+ translator-prune    Remove leftover files like pexes from pnacl_translator
#+                     build and from translator-archive-pexes.
translator-prune() {
  find "${INSTALL_TRANSLATOR}" -name "*.pexe" -exec "rm" {} +
}


#+ translator-clean <arch> -
#+     Clean one translator install/build
translator-clean() {
  local arch=$1
  StepBanner "TRANSLATOR" "Clean ${arch}"
  rm -rf "$(GetTranslatorInstallDir ${arch})"
  rm -rf "$(GetTranslatorBuildDir ${arch})"
}

#@ all                   - Alias for 'everything'
all() {
  everything
}

#@ status                - Show status of build directories
status() {
  # TODO(robertm): this is currently broken
  StepBanner "BUILD STATUS"

  status-helper "BINUTILS"             binutils
  status-helper "LLVM"                 llvm

  status-helper "NEWLIB"               newlib
  status-helper "C++ Standard Library" lib-cpp
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
  rm -rf "${INSTALL_LIB_NATIVE}"*
}


#@-------------------------------------------------------------------------

prune-host() {
  echo "stripping binaries (binutils)"
  strip "${BINUTILS_INSTALL_DIR}"/bin/*

  echo "stripping binaries (llvm)"
  if ! strip "${LLVM_INSTALL_DIR}"/bin/* ; then
    echo "NOTE: some failures during stripping are expected"
  fi

  echo "removing unused clang shared lib"
  rm -rf "${LLVM_INSTALL_DIR}"/${SO_DIR}/*clang${SO_EXT}

  echo "removing unused binutils binaries"
  rm -rf "${LLVM_INSTALL_DIR}"/bin/le32-nacl-elfedit
  rm -rf "${LLVM_INSTALL_DIR}"/bin/le32-nacl-gprof
  rm -rf "${LLVM_INSTALL_DIR}"/bin/le32-nacl-objcopy

  echo "removing unused LLVM/Clang binaries"
  rm -rf "${LLVM_INSTALL_DIR}"/bin/bc-wrap
  rm -rf "${LLVM_INSTALL_DIR}"/bin/bugpoint
  rm -rf "${LLVM_INSTALL_DIR}"/bin/c-index-test
  rm -rf "${LLVM_INSTALL_DIR}"/bin/clang-*
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llc
  rm -rf "${LLVM_INSTALL_DIR}"/bin/lli
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llvm-ar
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llvm-bcanalyzer
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llvm-config
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llvm-cov
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llvm-diff
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llvm-dwarfdump
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llvm-extract
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llvm-mcmarkup
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llvm-prof
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llvm-ranlib
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llvm-readobj
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llvm-rtdyld
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llvm-size
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llvm-stress
  rm -rf "${LLVM_INSTALL_DIR}"/bin/llvm-symbolizer
  rm -rf "${LLVM_INSTALL_DIR}"/bin/macho-dump
  rm -rf "${LLVM_INSTALL_DIR}"/bin/pso-stub
  rm -rf "${LLVM_INSTALL_DIR}"/bin/*-tblgen

  echo "removing llvm & clang headers"
  rm -rf "${LLVM_INSTALL_DIR}"/include

  echo "removing docs/ and share/"
  rm -rf "${LLVM_INSTALL_DIR}"/docs
  rm -rf "${LLVM_INSTALL_DIR}"/share

  echo "removing unused libs"
  rm -rf "${LLVM_INSTALL_DIR}"/lib/*.a
  rm -rf "${LLVM_INSTALL_DIR}"/lib/bfd-plugins
  rm -rf "${LLVM_INSTALL_DIR}"/lib/BugpointPasses.so
  rm -rf "${LLVM_INSTALL_DIR}"/lib/LLVMHello.so
}

#+ prune                 - Prune toolchain
prune() {
  StepBanner "PRUNE" "Pruning toolchain"
  local dir_size_before=$(get_dir_size_in_mb ${INSTALL_ROOT})

  SubBanner "Size before: ${INSTALL_ROOT} ${dir_size_before}MB"

  prune-host

  echo "removing .pyc files"
  rm -f "${INSTALL_BIN}"/pydir/*.pyc

  local dir_size_after=$(get_dir_size_in_mb "${INSTALL_ROOT}")
  SubBanner "Size after: ${INSTALL_ROOT} ${dir_size_after}MB"
}

#+ tarball <filename>    - Produce tarball file
tarball() {
  if [ ! -n "${1:-}" ]; then
    echo "Error: tarball needs a tarball name." >&2
    exit 1
  fi
  local tarball="$(ArgumentToAbsolutePath "$1")"
  StepBanner "TARBALL" "Creating tar ball ${tarball}"
  tar zcf "${tarball}" -C "${INSTALL_ROOT}" .
  ls -l ${tarball}
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
  StepBanner "LLVM (${HOST_ARCH} HOST)"

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
  # Symbolic link named : ${TC_SRC_LLVM}/tools/clang
  # Needs to point to   : ${TC_SRC_CLANG}
  ln -sf "${TC_SRC_CLANG}" "${TC_SRC_LLVM}"/tools/clang
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

  # Some components like ARMDisassembler may time out when built with -O3.
  # If we ever start using MSVC, we may also need to tone down the opt level
  # (see the settings in the CMake file for those components).
  local llvm_extra_opts=${LLVM_EXTRA_OPTIONS}
  if ${BUILD_PLATFORM_MAC}; then
    llvm_extra_opts="${LLVM_EXTRA_OPTIONS} --with-optimize-option=-O2"
  fi

  llvm-link-clang
  # The --with-binutils-include is to allow llvm to build the gold plugin
  # re: --enable-targets  "x86" brings in both i686 and x86_64.
  #
  # Disabling zlib features for now to reduce set of shared lib deps.
  # This may disable things like compressed debug info:
  # https://code.google.com/p/nativeclient/issues/detail?id=3592
  #
  # Disabling terminfo removes a dependency on libtinfo on Linux which
  # is awkward mostly because it would require a multilib x86-32 copy
  # of libtinfo installed.  This just disables output colouring.
  local binutils_include="${TC_SRC_BINUTILS}/include"
  RunWithLog "llvm.configure" \
      env -i PATH="${PATH}" \
             MAKE_OPTS=${MAKE_OPTS_HOST} \
             CC="${CC}" \
             CXX="${CXX}" \
             ${srcdir}/configure \
             --enable-shared \
             --disable-zlib \
             --disable-terminfo \
             --disable-jit \
             --with-binutils-include=${binutils_include} \
             --enable-targets=x86,arm,mips \
             --prefix="${LLVM_INSTALL_DIR}" \
             --program-prefix= \
             ${llvm_extra_opts}


  spopd
}

#+ llvm-configure-ninja - Configure with cmake for ninja build
# Not used by default. Call manually. Pass build type (Release or Debug)
# as an argument; The default is Release.
llvm-configure-ninja() {
  StepBanner "LLVM" "Configure (Cmake-ninja)"

  local srcdir="${TC_SRC_LLVM}"
  local objdir="${TC_BUILD_LLVM}"
  local buildtype="${1:-Release}"

  mkdir -p "${objdir}"
  spushd "${objdir}"

  llvm-link-clang
  local binutils_include="${TC_SRC_BINUTILS}/include"
  # Disabling zlib features for now to reduce set of shared lib deps.
  # This may disable things like compressed debug info:
  # https://code.google.com/p/nativeclient/issues/detail?id=3592
  RunWithLog "llvm.configure.cmake" \
    env \
      cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=${buildtype} \
      -DCMAKE_INSTALL_PREFIX="${LLVM_INSTALL_DIR}" \
      -DCMAKE_INSTALL_RPATH='$ORIGIN/../lib' \
      -DBUILD_SHARED_LIBS=ON \
      -DLLVM_TARGETS_TO_BUILD="X86;ARM;Mips" \
      -DLLVM_ENABLE_ASSERTIONS=ON \
      -ULLVM_ENABLE_ZLIB \
      -DLLVM_ENABLE_TERMINFO=OFF \
      -DLLVM_BUILD_TESTS=ON \
      -DLLVM_APPEND_VC_REV=ON \
      -DLLVM_BINUTILS_INCDIR="${binutils_include}" \
      ${srcdir}
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
  [ ! -f "${TC_BUILD_LLVM}/config.status" \
    -a ! -f "${TC_BUILD_LLVM}/build.ninja" ]
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

  if [ -f "${TC_BUILD_LLVM}/build.ninja" ]; then
    if [ -f "${TC_BUILD_LLVM}/config.status" ]; then
      echo "ERROR: Found multiple build system files in ${TC_BUILD_LLVM}"
      exit 1
    fi
    echo "Using ninja"
    ninja
  else
    RunWithLog llvm.make \
      env -i PATH="${PATH}" \
      MAKE_OPTS="${MAKE_OPTS_HOST}" \
      NACL_SANDBOX=0 \
      CC="${CC}" \
      CXX="${CXX}" \
      make ${MAKE_OPTS_HOST} all
  fi

  ts-touch-commit  "${objdir}"

  spopd
}

#+ llvm-install          - Install LLVM
llvm-install() {
  StepBanner "LLVM" "Install"

  spushd "${TC_BUILD_LLVM}"
  llvm-link-clang
  if [ -f "${TC_BUILD_LLVM}/build.ninja" ]; then
    echo "Using ninja"
    RunWithLog llvm.install ninja install
  else
    RunWithLog llvm.install \
      env -i PATH=/usr/bin/:/bin \
      MAKE_OPTS="${MAKE_OPTS}" \
      NACL_SANDBOX=0 \
      CC="${CC}" \
      CXX="${CXX}" \
      make ${MAKE_OPTS} install
  fi
  spopd

  # This is really part of libgcc_eh, but gets blown away by the ninja build.
  install-unwind-header
}

#########################################################################
#########################################################################
#     < LIBGCC_EH >
#########################################################################
#########################################################################

libgcc_eh-all() {
  StepBanner "LIBGCC_EH (from GCC 4.6)"

  libgcc_eh arm
  libgcc_eh mips32
  libgcc_eh x86-32
  libgcc_eh x86-64
}

libgcc_eh-setup() {
  local arch=$1
  local flags=$2
  # For x86 we use nacl-gcc to build libgcc_eh because of some issues with
  # LLVM's handling of the gcc intrinsics used in the library. See
  # https://code.google.com/p/nativeclient/issues/detail?id=1933
  # and http://llvm.org/bugs/show_bug.cgi?id=8541
  # For ARM, LLVM does work and we use it to avoid dealing with the fact that
  # arm-nacl-gcc uses different libgcc support functions than PNaCl.
  if [ ${arch} == "arm" ]; then
    LIBGCC_EH_ENV=(
      CC="${PNACL_CC} ${flags} -arch ${arch} --pnacl-bias=${arch} \
         --pnacl-allow-translate --pnacl-allow-native" \
      AR="${PNACL_AR}" \
      NM="${PNACL_NM}" \
      RANLIB="${PNACL_RANLIB}")
  else
    LIBGCC_EH_ENV=(
      CC="$(GetNNaClTool ${arch}) ${flags}" \
      AR="${PNACL_AR}" \
      NM="${PNACL_NM}" \
      RANLIB="${PNACL_RANLIB}")
  fi
}

libgcc_eh() {
  local arch=$1

  local objdir="${TC_BUILD}/libgcc_eh-${arch}-newlib"
  local subdir="${objdir}/fake-target/libgcc"
  local installdir="${INSTALL_LIB_NATIVE}${arch}"
  local label="(${arch} libgcc_eh.a)"

  mkdir -p "${installdir}"
  rm -rf "${installdir}"/libgcc_eh*
  rm -rf "${objdir}"

  # Setup fake gcc build directory.
  mkdir -p "${objdir}"/gcc
  cp -a "${PNACL_ROOT}"/scripts/libgcc-newlib.mvars \
        "${objdir}"/gcc/libgcc.mvars
  cp -a "${PNACL_ROOT}"/scripts/libgcc-tconfig.h "${objdir}"/gcc/tconfig.h
  touch "${objdir}"/gcc/tm.h

  install-unwind-header

  mkdir -p "${subdir}"
  spushd "${subdir}"
  local flags=""
  flags+=" -DENABLE_RUNTIME_CHECKING"

  libgcc_eh-setup ${arch} "${flags}"

  StepBanner "LIBGCC_EH" "Configure ${label}"
  RunWithLog libgcc.${arch}.configure \
    env -i \
      PATH="/usr/bin:/bin" \
      /bin/sh \
      "${TC_SRC_GCC}"/libgcc/configure \
        "${LIBGCC_EH_ENV[@]}" \
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
  RunWithLog libgcc.${arch}.make make libgcc_eh.a
  spopd

  StepBanner "LIBGCC_EH" "Install ${label}"
  cp ${subdir}/libgcc_eh.a "${installdir}"
}

install-unwind-header() {
  # TODO(pdox):
  # We need to establish an unwind ABI, since this is part of the ABI
  # exposed to the bitcode by the translator. This header should not vary
  # across compilers or C libraries.
  local install="/usr/bin/install -c -m 644"
  ${install} ${TC_SRC_GCC}/gcc/unwind-generic.h \
             ${LLVM_INSTALL_DIR}/lib/clang/3.4/include/unwind.h
}

#########################################################################
#########################################################################
#     < COMPILER-RT >
#########################################################################
#########################################################################

compiler-rt-all() {
  StepBanner "COMPILER-RT (LIBGCC)"
  compiler-rt arm
  compiler-rt mips32
  compiler-rt x86-32
  compiler-rt x86-64
  compiler-rt x86-32-nonsfi
}


#+ compiler-rt           - build/install llvm's replacement for libgcc.a
compiler-rt() {
  local arch=$1
  local src="${TC_SRC_COMPILER_RT}/lib"
  local objdir="${TC_BUILD_COMPILER_RT}-${arch}"
  local installdir="${INSTALL_LIB_NATIVE}${arch}"
  StepBanner "compiler rt" "build (${arch})"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"

  spushd "${objdir}"
  RunWithLog libgcc.${arch}.make \
      make -j ${PNACL_CONCURRENCY} -f ${src}/Makefile-pnacl libgcc.a \
        CC="${PNACL_CC}" \
        AR="${PNACL_AR}" \
        "SRC_DIR=${src}" \
        "CFLAGS=-arch ${arch} -DPNACL_${arch} --pnacl-allow-translate -O3"

  StepBanner "compiler rt" "install (${arch})"
  mkdir -p "${installdir}"
  cp libgcc.a "${installdir}"
  spopd
}

#########################################################################
#########################################################################
#                          < LIBSTDC++/LIBC++ >
#########################################################################
#########################################################################

check-lib-cpp() {
  local lib=$1
  if [ ${lib} != ${LIB_CXX_NAME} ] && [ ${lib} != ${LIB_STDCPP_NAME} ]; then
    echo "ERROR: Unsupported C++ Standard Library '${lib}'. Choose one of: ${LIB_CXX_NAME}, ${LIB_STDCPP_NAME}"
    exit -1
  fi
}

LIB_CPP_IS_SETUP=false
lib-cpp-setup() {
  if ${LIB_CPP_IS_SETUP} && [ $# -eq 0 ]; then
    return 0
  fi
  local arch=$1
  setup-biased-bitcode-env ${arch}
  LIB_CPP_IS_SETUP=true
}

lib-cpp() {
  local lib=$1
  local arch=$2
  check-lib-cpp "${lib}"
  lib-cpp-setup "${arch}"
  StepBanner "C++ Standard Library" "(BITCODE $*)"

  if lib-cpp-needs-configure "${lib}" "${arch}"; then
    lib-cpp-clean "${lib}"
    lib-cpp-configure "${lib}" "${arch}"
  else
    StepBanner "${lib}" "configure"
  fi

  if lib-cpp-needs-make "${lib}" "${arch}"; then
    lib-cpp-make "${lib}" "${arch}"
  else
    SkipBanner "${lib}" "make"
  fi

  lib-cpp-install "${lib}" "${arch}"
  LIB_CPP_IS_SETUP=false
}

#+ lib-cpp-clean - clean libc++/libstdc++ in bitcode
lib-cpp-clean() {
  local lib=$1
  StepBanner "${lib}" "Clean"
  rm -rf ${TC_BUILD}/${lib}*
}

lib-cpp-needs-configure() {
  local lib=$1
  local arch=$2
  lib-cpp-setup "${arch}"
  local objdir="${LIB_CPP_BUILD}-${lib}"
  ts-newer-than "${TC_BUILD_LLVM}" "${objdir}" && return 0
  [ ! -f "${objdir}/config.status" ]
  return $?
}

lib-cpp-configure() {
  local lib=$1
  local arch=$2
  lib-cpp-setup "${arch}"
  StepBanner "${lib}" "Configure"
  local objdir="${LIB_CPP_BUILD}-${lib}"
  local subdir="${objdir}/pnacl-target"
  local flags=""
  mkdir -p "${subdir}"
  spushd "${subdir}"

  flags+="--with-newlib --disable-shared --disable-rpath"

  setup-lib-cpp-env

  if [ ${lib} == ${LIB_CXX_NAME} ]; then
    local srcdir="${TC_SRC_LIBCXX}"
    # HAS_THREAD_LOCAL is used by libc++abi's exception storage, the
    # fallback is pthread otherwise.
    local cflags="-g -O2 -mllvm -inline-threshold=5 ${BIASED_BC_CFLAGS} \
      -DHAS_THREAD_LOCAL=1"
    # LLVM's lit is used to test libc++. run.py serves as a shell that
    # translates pexe->nexe and executes in sel_ldr. The libc++ test
    # suite needs to be told to use pnacl-clang++'s "system library",
    # which happens to be libc++ when the -stdlib parameter is used.
    # The target architecture is set through the PNACL_RUN_ARCH
    # environment variable (``-arch env``).
    local litargs="--verbose"
    litargs+=" --param shell_prefix='${NACL_ROOT}/run.py -arch env --retries=1'"
    litargs+=" --param exe_suffix='.pexe'"
    litargs+=" --param use_system_lib=true"
    litargs+=" --param link_flags='-std=gnu++11 --pnacl-exceptions=sjlj'"
    # TODO(jfb) CMAKE_???_COMPILER_WORKS can be removed once the PNaCl
    #           driver scripts stop confusing cmake for libc++. See:
    #           https://code.google.com/p/nativeclient/issues/detail?id=3661
    RunWithLog libcxx.configure \
      env \
      cmake -G "Unix Makefiles" \
      -DCMAKE_CXX_COMPILER_WORKS=1 \
      -DCMAKE_C_COMPILER_WORKS=1 \
      -DCMAKE_INSTALL_PREFIX="${LIB_CPP_INSTALL_DIR}" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER="${PNACL_CC}" \
      -DCMAKE_CXX_COMPILER="${PNACL_CXX}" \
      -DCMAKE_AR="${PNACL_AR}" \
      -DCMAKE_NM="${PNACL_NM}" \
      -DCMAKE_RANLIB="${PNACL_RANLIB}" \
      -DCMAKE_LD="${ILLEGAL_TOOL}" \
      -DCMAKE_AS="${ILLEGAL_TOOL}" \
      -DCMAKE_OBJDUMP="${ILLEGAL_TOOL}" \
      -DCMAKE_C_FLAGS="-std=gnu11 ${cflags}" \
      -DCMAKE_CXX_FLAGS="-std=gnu++11 ${cflags}" \
      -DLIT_EXECUTABLE="${TC_SRC_LLVM}/utils/lit/lit.py" \
      -DLLVM_LIT_ARGS="${litargs}" \
      -DLIBCXX_ENABLE_CXX0X=0 \
      -DLIBCXX_ENABLE_SHARED=0 \
      -DLIBCXX_CXX_ABI=libcxxabi \
      -DLIBCXX_LIBCXXABI_INCLUDE_PATHS="${TC_SRC_LIBCXX}/../libcxxabi/include" \
      ${flags} \
      ${srcdir}
  else
    local srcdir="${TC_SRC_LIBSTDCPP}"
    RunWithLog libstdcpp.configure \
      env -i PATH=/usr/bin/:/bin \
      "${STD_ENV_FOR_LIB_CPP[@]}" \
      "${srcdir}"/configure \
      --host="${CROSS_TARGET_ARM}" \
      --prefix="${LIB_CPP_INSTALL_DIR}" \
      --enable-cxx-flags="-D__SIZE_MAX__=4294967295" \
      --disable-multilib \
      --disable-linux-futex \
      --disable-libstdcxx-time \
      --disable-sjlj-exceptions \
      --disable-libstdcxx-pch \
      ${flags}
  fi
  spopd
}

lib-cpp-needs-make() {
  local lib=$1
  local arch=$2
  lib-cpp-setup "${arch}"
  local srcdir="${TC_SRC_LIBSTDCPP}" && \
    [ ${lib} == ${LIB_CXX_NAME} ] && srcdir="${TC_SRC_LIBCXX}"
  local objdir="${LIB_CPP_BUILD}-${lib}"

  ts-modified "${srcdir}" "${objdir}"
  return $?
}

lib-cpp-make() {
  local lib=$1
  local arch=$2
  lib-cpp-setup "${arch}"
  StepBanner "${lib}" "Make"
  local objdir="${LIB_CPP_BUILD}-${lib}"

  ts-touch-open "${objdir}"

  spushd "${objdir}/pnacl-target"
  setup-lib-cpp-env
  RunWithLog "${lib}.make" \
    env -i PATH=/usr/bin/:/bin \
    make \
    "${STD_ENV_FOR_LIB_CPP[@]}" \
    ${MAKE_OPTS}
  spopd

  ts-touch-commit "${objdir}"
}

lib-cpp-install() {
  local lib=$1
  local arch=$2
  lib-cpp-setup "${arch}"
  StepBanner "${lib}" "Install"
  local objdir="${LIB_CPP_BUILD}-${lib}"

  spushd "${objdir}/pnacl-target"

  # Clean the existing installation
  local include_dir=""
  if [ ${lib} == ${LIB_CXX_NAME} ]; then
    include_dir="v[0-9]"
  else
    include_dir="[0-9].[0-9].[0-9]"
  fi
  rm -rf "${LIB_CPP_INSTALL_DIR}"/include/c++/"${include_dir}"
  rm -rf "${LIB_CPP_INSTALL_DIR}"/lib/"${lib}"*

  # install headers (=install-data)
  # for good measure make sure we do not keep any old headers
  setup-lib-cpp-env
  if [ ${lib} == ${LIB_CXX_NAME} ]; then
    RunWithLog "${lib}.install" \
      make \
      "${STD_ENV_FOR_LIB_CPP[@]}" \
      ${MAKE_OPTS} install
  else
    RunWithLog "${lib}.install" \
      make \
      "${STD_ENV_FOR_LIB_CPP[@]}" \
      ${MAKE_OPTS} install-data
  fi

  # Install bitcode library
  mkdir -p "${LIB_CPP_INSTALL_DIR}/lib"
  if [ ${lib} == ${LIB_CXX_NAME} ]; then
    cp "${objdir}/pnacl-target/lib/${LIB_CXX_NAME}.a" \
      "${LIB_CPP_INSTALL_DIR}/lib"
  else
    cp "${objdir}/pnacl-target/src/.libs/${LIB_STDCPP_NAME}.a" \
      "${LIB_CPP_INSTALL_DIR}/lib"
  fi
  spopd

  # libstdc++ installs a file with an abnormal name: "libstdc++*-gdb.py"
  # The asterisk may be due to a bug in libstdc++ Makefile/configure.
  # This causes problems on the Windows bot (during cleanup, toolchain
  # directory delete fails due to the bad character).
  # Rename it to get rid of the asterisk.
  if [ ${lib} == ${LIB_STDCPP_NAME} ]; then
    spushd "${LIB_CPP_INSTALL_DIR}/lib"
    mv -f ${LIB_STDCPP_NAME}'*'-gdb.py ${LIB_STDCPP_NAME}-gdb.py
    spopd
  fi
}

#########################################################################
#########################################################################
#                          < Tools >
#########################################################################
#########################################################################

build-validator() {
  local arch=$1
  StepBanner "MISC-TOOLS" "Building validator ("${arch}")"
  spushd "${NACL_ROOT}"
  RunWithLog ${arch}_ncval_core \
    ./scons MODE=opt-host \
    sysinfo=0 \
    ${arch}-ncval-core
  cp ${SCONS_OUT}/opt-linux-x86-32/obj/src/trusted/\
validator_${arch}/${arch}-ncval-core ${INSTALL_ROOT}/tools-x86
  spopd
}

#+ misc-tools            - Build and install sel_ldr and validator for ARM.
misc-tools() {
  if ${PNACL_BUILD_ARM} ; then
    StepBanner "MISC-TOOLS" "Building sel_ldr (arm)"

    # TODO(robertm): revisit some of these options
    spushd "${NACL_ROOT}"
    RunWithLog arm_sel_ldr \
      ./scons MODE=opt-host \
      platform=arm \
      naclsdk_validate=0 \
      sysinfo=0 \
      -j${PNACL_CONCURRENCY} \
      sel_ldr
    rm -rf  "${INSTALL_ROOT}/tools-arm"
    mkdir "${INSTALL_ROOT}/tools-arm"
    local sconsdir="${SCONS_OUT}/opt-${SCONS_BUILD_PLATFORM}-arm"
    cp "${sconsdir}/obj/src/trusted/service_runtime/sel_ldr" \
       "${INSTALL_ROOT}/tools-arm"
    spopd
  else
    StepBanner "MISC-TOOLS" "Skipping arm sel_ldr (No trusted arm toolchain)"
  fi

  # TODO(petarj): It would be nice to build MIPS sel_ldr on builbots later.
  if [ "${PNACL_BUILD_MIPS}" == "true" ] ; then
    StepBanner "MISC-TOOLS" "Building sel_ldr (mips32)"
    spushd "${NACL_ROOT}"
    RunWithLog mips32_sel_ldr \
      ./scons MODE=opt-host \
      platform=mips32 \
      naclsdk_validate=0 \
      sysinfo=0 \
      -j${PNACL_CONCURRENCY} \
      sel_ldr
    rm -rf  "${INSTALL_ROOT}/tools-mips32"
    mkdir "${INSTALL_ROOT}/tools-mips32"
    local sconsdir="scons-out/opt-${SCONS_BUILD_PLATFORM}-mips32"
    cp "${sconsdir}/obj/src/trusted/service_runtime/sel_ldr" \
       "${INSTALL_ROOT}/tools-mips32"
    spopd
  else
    StepBanner "MISC-TOOLS" "Skipping mips sel_ldr (No trusted mips toolchain)"
  fi

  if ${BUILD_PLATFORM_LINUX} ; then
    rm -rf  "${INSTALL_ROOT}/tools-x86"
    mkdir "${INSTALL_ROOT}/tools-x86"
    for target in arm mips; do
      build-validator $target
    done
  else
    for target in arm mips; do
      StepBanner "MISC-"${target} "Skipping " ${target} " validator (Not yet supported on Mac)"
    done
  fi
}

#########################################################################
#     < BINUTILS >
#########################################################################

#+-------------------------------------------------------------------------
#+ binutils          - Build and install binutils (cross-tools)
binutils() {
  StepBanner "BINUTILS (${HOST_ARCH} HOST)"

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
  local targ="arm-pc-nacl,i686-pc-nacl,x86_64-pc-nacl,mipsel-pc-nacl"
  mkdir -p "${objdir}"
  spushd "${objdir}"

  # The linux escaping is horrible but apparently the only way of doing this:
  # c.f.:  http://sourceware.org/ml/binutils/2009-05/msg00252.html
  # all we try to do here is to add "$ORIGIN/../lib to "rpath".
  # If you ever touch this please make sure that rpath is correct via:
  # objdump -p ${TOOLCHAIN_BASE}/pnacl_newlib/host/bin/le32-nacl-ld.gold
  # objdump -p ${TOOLCHAIN_BASE}/pnacl_newlib/host/bin/le32-nacl-objdump
  if ${BUILD_PLATFORM_LINUX} ; then
      local flags='-Xlinker -rpath -Xlinker '"'"'$\\$$\$$\\$$\$$ORIGIN/../lib'"'"
      local shared='yes'
      local zlib='--without-zlib'
  else
      # The shared build for binutils on mac is currently disabled.
      # A mac-expert needs to look at this but
      # It seems that on mac the linker is storing "full" library paths into
      # the dynamic image, e.g, for the llc dynamic image we see  paths like:
      # @executable_path/../lib/libLLVM-3.3svn.dylib
      # This only works if at linktime the libraries are already at
      # @executable_path/../lib which is not the case for mac
      #local flags="-Xlinker -rpath -Xlinker '@executable_path/../lib'"
      local flags=''
      local shared='no'
      local zlib=''
  fi
  # The --enable-gold and --enable-plugins options are on so that we
  # can use gold's support for plugin to link PNaCl modules, and use
  # gold as the final linker. We do not use bfd ld, and it is disabled
  # in part because we do not have its MIPS support downstream.

  # We llvm's mc for assembly so we no longer build gas
  RunWithLog binutils.configure \
      env -i \
      PATH="${PATH}" \
      CC="${CC}" \
      CXX="${CXX}" \
      LDFLAGS="${flags}" \
      ${srcdir}/configure \
          --prefix="${BINUTILS_INSTALL_DIR}" \
          --target=${BINUTILS_TARGET} \
          --program-prefix=${REAL_CROSS_TARGET}- \
          --enable-targets=${targ} \
          --enable-shared=${shared} \
          --enable-gold=default \
          --enable-ld=no \
          --disable-nls \
          --enable-plugins \
          --disable-werror \
          --without-gas \
          ${zlib} \
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
  ts-modified "$srcdir" "$objdir" && ret=0
  return ${ret}
}

#+ binutils-make     - Make binutils for ARM
binutils-make() {
  StepBanner "BINUTILS" "Make"
  local objdir="${TC_BUILD_BINUTILS}"
  spushd "${objdir}"

  ts-touch-open "${objdir}"

  if ${BUILD_PLATFORM_LINUX}; then
    RunWithLog binutils.make \
      env -i PATH="/usr/bin:/bin" \
      ac_cv_search_zlibVersion=no \
      make ${MAKE_OPTS_HOST}
  else
    local control_parallel=""
    if ${BUILD_PLATFORM_MAC}; then
      # Try to avoid parallel builds for Mac to see if it avoids flake.
      # http://code.google.com/p/nativeclient/issues/detail?id=2926
      control_parallel="-j1"
    fi
    RunWithLog binutils.make \
      env -i PATH="${PATH}" \
      make ${MAKE_OPTS_HOST} ${control_parallel}
  fi

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

  # TODO(robertm): remove this once we manage to avoid building
  #                ld in the first place
  echo "pruning hack: ${BINUTILS_INSTALL_DIR}"
  rm -f "${BINUTILS_INSTALL_DIR}/bin/${REAL_CROSS_TARGET}-ld"
  rm -f "${BINUTILS_INSTALL_DIR}/bin/${REAL_CROSS_TARGET}-ld.bfd"

  # Also remove "${BINUTILS_INSTALL_DIR}/${BINUTILS_TARGET}" which contains
  # duplicate binaries and unused linker scripts
  echo "remove unused ${BINUTILS_INSTALL_DIR}/${BINUTILS_TARGET}/"
  rm -rf "${BINUTILS_INSTALL_DIR}/${BINUTILS_TARGET}/"

  # Move binutils shared libs to host/lib.
  # The first "*" expands to the host string, e.g.
  # x86_64-unknown-linux-gnu
  if ${BUILD_PLATFORM_LINUX} ; then
    echo "move shared libs to ${BINUTILS_INSTALL_DIR}/${SO_DIR}"
    for lib in ${BINUTILS_INSTALL_DIR}/*/${BINUTILS_TARGET}/lib/lib*${SO_EXT}
    do
      echo "moving ${lib}"
      mv ${lib} ${BINUTILS_INSTALL_DIR}/${SO_DIR}
    done
  fi

  # NOTE: this needs to come AFTER the moving of the shared libs
  # TODO(robertm): this needs to be augmented for mac and windows
  echo "remove unused (remaining) binutils static libs and headers"
  rm -rf "${BINUTILS_INSTALL_DIR}/i686-pc-linux-gnu" \
         "${BINUTILS_INSTALL_DIR}/x86_64-unknown-linux-gnu"

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
      PATH="${PATH}" \
      CC="${CC}" \
      CXX="${CXX}" \
      ${srcdir}/configure
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
      PATH="${PATH}" \
      CC="${CC}" \
      CXX="${CXX}" \
      make ${MAKE_OPTS_HOST} all-libiberty

  ts-touch-commit "${objdir}"

  spopd
}

#########################################################################
#     CLIENT BINARIES (SANDBOXED)
#########################################################################

check-arch() {
  local arch=$1
  for valid_arch in i686 x86_64 armv7 universal ; do
    if [ "${arch}" == "${valid_arch}" ] ; then
      return
    fi
  done

  Fatal "ERROR: Unsupported arch [$1]. Must be: i686, x86_64, armv7, universal"
}

llvm-sb-setup() {
  local arch=$1
  LLVM_SB_LOG_PREFIX="llvm.sb.${arch}"
  LLVM_SB_OBJDIR="$(GetTranslatorBuildDir ${arch})/llvm-sb"

  # The SRPC headers are included directly from the nacl tree, as they are
  # not in the SDK. libsrpc should have already been built by the
  # build.sh sdk-private-libs step.
  # This is always statically linked.
  # The LLVM sandboxed build uses the normally-disallowed external
  # function __nacl_get_arch().  Allow that for now.
  local flags="-static -I$(GetAbsolutePath ${NACL_ROOT}/..) \
    --pnacl-disable-abi-check "

  LLVM_SB_CONFIGURE_ENV=(
    AR="${PNACL_AR}" \
    AS="${PNACL_AS}" \
    CC="${PNACL_CC} ${flags}" \
    CXX="${PNACL_CXX} ${flags}" \
    LD="${PNACL_LD} ${flags}" \
    NM="${PNACL_NM}" \
    RANLIB="${PNACL_RANLIB}")
}

#+-------------------------------------------------------------------------
#+ llvm-sb <arch>- Build and install llvm tools (sandboxed)
llvm-sb() {
  local arch=$1
  check-arch ${arch}
  llvm-sb-setup ${arch}
  StepBanner "LLVM-SB" "Sandboxed pnacl-llc [${arch}]"
  local srcdir="${TC_SRC_LLVM}"
  assert-dir "${srcdir}" "You need to checkout llvm."

  if llvm-sb-needs-configure ${arch} ; then
    llvm-sb-clean ${arch}
    llvm-sb-configure ${arch}
  else
    SkipBanner "LLVM-SB" "configure ${arch}"
  fi

  llvm-sb-make ${arch}
  llvm-sb-install ${arch}
}

llvm-sb-needs-configure() {
  [ ! -f "${LLVM_SB_OBJDIR}/config.status" ]
  return $?
}

# llvm-sb-clean          - Clean llvm tools (sandboxed)
llvm-sb-clean() {
  local arch=$1
  StepBanner "LLVM-SB" "Clean ${arch}"
  local objdir="${LLVM_SB_OBJDIR}"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"
}

# llvm-sb-configure - Configure llvm tools (sandboxed)
llvm-sb-configure() {
  local arch=$1

  StepBanner "LLVM-SB" "Configure ${arch}"
  local srcdir="${TC_SRC_LLVM}"
  local objdir="${LLVM_SB_OBJDIR}"
  local installdir="$(GetTranslatorInstallDir ${arch})"
  local targets=""
  # For LLVM, "x86" brings in both i686 and x86_64.
  case ${arch} in
    i686) targets=x86 ;;
    x86_64) targets=x86 ;;
    armv7) targets=arm ;;
    universal) targets=x86,arm ;;
  esac

  spushd "${objdir}"
  # TODO(jvoung): remove ac_cv_func_getrusage=no once newlib has getrusage
  # in its headers.  Otherwise, configure thinks that we can link in
  # getrusage (stub is in libnacl), but we can't actually compile code
  # that uses ::getrusage because it's not in headers:
  # https://code.google.com/p/nativeclient/issues/detail?id=3657
  RunWithLog \
      ${LLVM_SB_LOG_PREFIX}.configure \
      env -i \
      PATH="/usr/bin:/bin" \
      ${srcdir}/configure \
        "${LLVM_SB_CONFIGURE_ENV[@]}" \
        --prefix=${installdir} \
        --host=nacl \
        --enable-targets=${targets} \
        --disable-assertions \
        --enable-pic=no \
        --enable-static \
        --enable-shared=no \
        --disable-jit \
        --enable-optimized \
        --target=${CROSS_TARGET_ARM} \
        llvm_cv_link_use_export_dynamic=no \
        ac_cv_func_getrusage=no
  spopd
}

# llvm-sb-make - Make llvm tools (sandboxed)
llvm-sb-make() {
  local arch=$1
  StepBanner "LLVM-SB" "Make ${arch}"
  local objdir="${LLVM_SB_OBJDIR}"

  spushd "${objdir}"
  ts-touch-open "${objdir}"

  local tools_to_build="pnacl-llc"
  local export_dyn_env="llvm_cv_link_use_export_dynamic=no"
  local isjit=0
  RunWithLog ${LLVM_SB_LOG_PREFIX}.make \
      env -i PATH="/usr/bin:/bin" \
      ONLY_TOOLS="${tools_to_build}" \
      NACL_SANDBOX=1 \
      KEEP_SYMBOLS=1 \
      VERBOSE=1 \
      ${export_dyn_env} \
      make ${MAKE_OPTS} tools-only

  ts-touch-commit "${objdir}"

  spopd
}

# llvm-sb-install - Install llvm tools (sandboxed)
llvm-sb-install() {
  local arch=$1
  StepBanner "LLVM-SB" "Install ${arch}"

  local toolname="pnacl-llc"
  local installdir="$(GetTranslatorInstallDir ${arch})"/bin
  mkdir -p "${installdir}"
  spushd "${installdir}"
  local objdir="${LLVM_SB_OBJDIR}"
  cp -f "${objdir}"/Release*/bin/${toolname} .
  mv -f ${toolname} ${toolname}.pexe
  local arches=${arch}
  if [[ "${arch}" == "universal" ]]; then
    arches="${SBTC_ARCHES_ALL}"
  elif [[ "${arch}" == "i686" ]]; then
    # LLVM does not separate the i686 and x86_64 backends.
    # Translate twice to get both nexes.
    arches="i686 x86_64"
  fi
  translate-sb-tool ${toolname} "${arches}"
  install-sb-tool ${toolname} "${arches}"
  spopd
}

# translate-sb-tool <toolname> <arches>
#
# Translate <toolname>.pexe to <toolname>.<arch>.nexe in the current directory.
translate-sb-tool() {
  local toolname=$1
  local arches=$2
  local pexe="${toolname}.pexe"
  if ${PNACL_PRUNE}; then
    # Only strip debug, to preserve symbol names for testing. This is okay
    # because we strip the native nexe later anyway.
    # So, why bother stripping here at all?
    # It does appear to affect the size of the nexe:
    # http://code.google.com/p/nativeclient/issues/detail?id=3305
    ${PNACL_STRIP} --strip-debug "${pexe}"
  fi

  local tarch
  for tarch in ${arches}; do
    local nexe="${toolname}.${tarch}.nexe"
    StepBanner "TRANSLATE" \
               "Translating ${toolname}.pexe to ${tarch} (background)"
    # NOTE: we are using --noirt to build without a segment gap
    # since we aren't loading the IRT for the translator nexes.
    #
    # Compiling with -ffunction-sections, -fdata-sections, --gc-sections
    # helps reduce the size a bit. If you want to use --gc-sections to test out:
    # http://code.google.com/p/nativeclient/issues/detail?id=1591
    # you will need to do a build without these flags.
    "${PNACL_TRANSLATE}" -ffunction-sections -fdata-sections --gc-sections \
      --allow-llvm-bitcode-input --noirt -arch ${tarch} "${pexe}" -o "${nexe}" &
    QueueLastProcess
  done
  StepBanner "TRANSLATE" "Waiting for translation processes to finish"
  QueueWait

  # Test that certain symbols have been pruned before stripping.
  if [ "${toolname}" == "pnacl-llc" ]; then
    for tarch in ${arches}; do
      local nexe="${toolname}.${tarch}.nexe"
      local llvm_host_glob="${LLVM_INSTALL_DIR}/lib/libLLVM*so"
      python "${PNACL_ROOT}/prune_test.py" "${PNACL_NM}" \
        "${llvm_host_glob}" "${nexe}"
    done
  fi

  if ${PNACL_PRUNE}; then
    # Strip the nexes.
    for tarch in ${arches}; do
      local nexe="${toolname}.${tarch}.nexe"
      ${PNACL_STRIP} "${nexe}"
    done
  fi
  StepBanner "TRANSLATE" "Done."
}

# install-sb-tool <toolname> <arches>
#
# Install <toolname>.<arch>.nexe from the current directory
# into the right location (as <toolname>.nexe)
install-sb-tool() {
  local toolname="$1"
  local arches="$2"
  local tarch
  for tarch in ${arches}; do
    local installbin="$(GetTranslatorInstallDir ${tarch})"/bin
    mkdir -p "${installbin}"
    mv -f ${toolname}.${tarch}.nexe "${installbin}"/${toolname}.nexe
    if ${PNACL_BUILDBOT}; then
      spushd "${installbin}"
      local tag="${toolname}_${tarch}_size"
      print-tagged-tool-sizes "${tag}" "$(pwd)/${toolname}.nexe"
      spopd
    fi
  done
}

GetTranslatorBuildDir() {
  local arch="$1"
  echo "${TC_BUILD}/translator-${arch//_/-}"
}

GetTranslatorInstallDir() {
  local arch="$1"
  echo "${INSTALL_TRANSLATOR}"/${arch}
}

### Sandboxed version of gold.

#+-------------------------------------------------------------------------
#+ binutils-gold-sb - Build and install gold (sandboxed)
#+                    This is the replacement for the old
#+                    final sandboxed linker which was bfd based.
#+                    It has nothing to do with the bitcode linker
#+                    which is also gold based.
binutils-gold-sb() {
  local arch=$1
  check-arch ${arch}
  StepBanner "GOLD-NATIVE-SB" "(libiberty + gold) ${arch}"

  local srcdir="${TC_SRC_BINUTILS}"
  assert-dir "${srcdir}" "You need to checkout gold."

  binutils-gold-sb-clean ${arch}
  binutils-gold-sb-configure ${arch}
  binutils-gold-sb-make ${arch}
  binutils-gold-sb-install ${arch}
}

# binutils-gold-sb-clean - Clean gold
binutils-gold-sb-clean() {
  local arch=$1
  StepBanner "GOLD-NATIVE-SB" "Clean ${arch}"
  local objdir="$(GetTranslatorBuildDir ${arch})/binutils-gold-sb"

  rm -rf "${objdir}"
  mkdir -p "${objdir}"
}

# binutils-gold-sb-configure - Configure binutils for gold (unsandboxed)
binutils-gold-sb-configure() {
  local arch=$1
  local srcdir="${TC_SRC_BINUTILS}"
  local objdir="$(GetTranslatorBuildDir ${arch})/binutils-gold-sb"
  local installbin="$(GetTranslatorInstallDir ${arch})/bin"

  # The SRPC headers are included directly from the nacl tree, as they are
  # not in the SDK. libsrpc should have already been built by the
  # build.sh sdk-private-libs step
  # The Gold sandboxed build uses the normally-disallowed external
  # function __nacl_get_arch().  Allow that for now.
  #
  # TODO(jfb) Gold currently only builds with libstdc++.
  local flags="-static -I$(GetAbsolutePath ${NACL_ROOT}/..) \
    -fno-exceptions -O3 --pnacl-disable-abi-check -stdlib=${LIB_STDCPP_NAME} "
  local configure_env=(
    AR="${PNACL_AR}" \
    AS="${PNACL_AS}" \
    CC="${PNACL_CC} ${flags}" \
    CXX="${PNACL_CXX} ${flags}" \
    CC_FOR_BUILD="${CC}" \
    CXX_FOR_BUILD="${CXX}" \
    LD="${PNACL_LD} ${flags}" \
    NM="${PNACL_NM}" \
    RANLIB="${PNACL_RANLIB}"
  )
  local gold_targets=""
  case ${arch} in
    i686)      gold_targets=i686-pc-nacl ;;
    x86_64)    gold_targets=x86_64-pc-nacl ;;
    armv7)     gold_targets=arm-pc-nacl ;;
    universal)
      gold_targets=i686-pc-nacl,x86_64-pc-nacl,arm-pc-nacl ;;
  esac

  # gold always adds "target" to the enabled targets so we are
  # little careful to not build too much
  # Note: we are (ab)using target for both --host and --target
  #       which configure expects to be present
  local target
  if [ ${arch} == "universal" ] ; then
    target=i686-pc-nacl
  else
    target=${gold_targets}
  fi

  StepBanner "GOLD-NATIVE-SB" "Configure (libiberty)"
  # Gold depends on liberty only for a few functions:
  # xrealloc, lbasename, etc.
  # we could remove these if necessary.

  mkdir -p "${objdir}/libiberty"
  spushd "${objdir}/libiberty"
  StepBanner "GOLD-NATIVE-SB" "Dir [$(pwd)]"
  local log_prefix="binutils-gold.sb.${arch}"
  RunWithLog "${log_prefix}".configure \
    env -i \
    PATH="/usr/bin:/bin" \
    "${configure_env[@]}" \
    ${srcdir}/libiberty/configure --prefix="${installbin}" \
    --host=${target} \
    --target=${target}
  spopd

  StepBanner "GOLD-NATIVE-SB" "Configure (gold) ${arch}"
  mkdir -p "${objdir}/gold"
  spushd "${objdir}/gold"
  StepBanner "GOLD-NATIVE-SB" "Dir [$(pwd)]"
  # Removed -Werror until upstream gold no longer has problems with new clang
  # warnings. http://code.google.com/p/nativeclient/issues/detail?id=2861
  # TODO(sehr,robertm): remove this when gold no longer has these.
  RunWithLog "${log_prefix}".configure \
    env -i \
    PATH="/usr/bin:/bin" \
    "${configure_env[@]}" \
    CXXFLAGS="" \
    CFLAGS="" \
    ac_cv_search_zlibVersion=no \
    ac_cv_header_sys_mman_h=no \
    ac_cv_func_mmap=no \
    ac_cv_func_mallinfo=no \
    ac_cv_prog_cc_g=no \
    ac_cv_prog_cxx_g=no \
    ${srcdir}/gold/configure --prefix="${installbin}" \
                                      --enable-targets=${gold_targets} \
                                      --host=${target} \
                                      --target=${target} \
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

# binutils-gold-sb-make - Make binutils (sandboxed)
binutils-gold-sb-make() {
  local arch=${1}
  local objdir="$(GetTranslatorBuildDir ${arch})/binutils-gold-sb"
  ts-touch-open "${objdir}/"

  StepBanner "GOLD-NATIVE-SB" "Make (liberty) ${arch}"
  spushd "${objdir}/libiberty"

  RunWithLog "binutils-gold.liberty.sb.${arch}".make \
      env -i PATH="/usr/bin:/bin" \
      make ${MAKE_OPTS}
  spopd

  StepBanner "GOLD-NATIVE-SB" "Make (gold) ${arch}"
  spushd "${objdir}/gold"
  RunWithLog "binutils-gold.sb.${arch}".make \
      env -i PATH="/usr/bin:/bin" \
      make ${MAKE_OPTS} ld-new
  spopd

  ts-touch-commit "${objdir}"
}

# binutils-gold-sb-install - Install gold
binutils-gold-sb-install() {
  local arch=$1
  local objdir="$(GetTranslatorBuildDir ${arch})/binutils-gold-sb"
  local installbin="$(GetTranslatorInstallDir ${arch})/bin"

  StepBanner "GOLD-NATIVE-SB" "Install [${installbin}] ${arch}"

  mkdir -p "${installbin}"
  spushd "${installbin}"

  # Install just "ld"
  cp "${objdir}"/gold/ld-new ld.pexe

  # Translate and install
  local arches=${arch}
  if [[ "${arch}" == "universal" ]]; then
    arches="${SBTC_ARCHES_ALL}"
  fi
  translate-sb-tool ld "${arches}"
  install-sb-tool ld "${arches}"
  spopd
}


#+-------------------------------------------------------------------------
#########################################################################
#     < NEWLIB-BITCODE >
#########################################################################

#+ newlib - Build and install newlib in bitcode.
newlib() {
  local arch=$1
  setup-newlib-env ${arch}
  StepBanner "NEWLIB (${arch})"

  if newlib-needs-configure; then
    newlib-clean
    newlib-configure ${arch}
  else
    SkipBanner "NEWLIB" "configure"
  fi

  if newlib-needs-make; then
    newlib-make ${arch}
  else
    SkipBanner "NEWLIB" "make"
  fi

  newlib-install ${arch}
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
  local arch=$1
  setup-newlib-env ${arch}
  StepBanner "NEWLIB" "Configure (${NEWLIB_TARGET})"

  local srcdir="${TC_SRC_NEWLIB}"
  local objdir="${TC_BUILD_NEWLIB}"
  mkdir -p "${objdir}"
  spushd "${objdir}"

  RunWithLog "newlib.${arch}.configure" \
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
        --enable-newlib-iconv-from-encodings=UTF-8,UTF-16LE,UCS-4LE,UTF-16,UCS-4 \
        --enable-newlib-iconv-to-encodings=UTF-8,UTF-16LE,UCS-4LE,UTF-16,UCS-4 \
        --enable-newlib-io-long-long \
        --enable-newlib-io-long-double \
        --enable-newlib-io-c99-formats \
        --enable-newlib-mb \
        --target="${NEWLIB_TARGET}"

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
  local arch=$1
  setup-newlib-env ${arch}
  StepBanner "NEWLIB" "Make"
  local srcdir="${TC_SRC_NEWLIB}"
  local objdir="${TC_BUILD_NEWLIB}"

  ts-touch-open "${objdir}"

  spushd "${objdir}"
  RunWithLog "newlib.${arch}.make" \
    env -i PATH="/usr/bin:/bin" \
    make \
      "${STD_ENV_FOR_NEWLIB[@]}" \
      ${MAKE_OPTS}
  spopd

  ts-touch-commit "${objdir}"

}

#+ newlib-install        - Install Bitcode Newlib using build env.
newlib-install() {
  local arch=$1
  setup-newlib-env ${arch}
  StepBanner "NEWLIB" "Install"
  local objdir="${TC_BUILD_NEWLIB}"

  spushd "${objdir}"

  # NOTE: we might be better off not using install, as we are already
  #       doing a bunch of copying of headers and libs further down
  RunWithLog newlib.install \
    env -i PATH="/usr/bin:/bin" \
      make \
      "${STD_ENV_FOR_NEWLIB[@]}" \
      install ${MAKE_OPTS}
  spopd

  # Newlib installs files into usr/${NEWLIB_TARGET}/*
  # Get rid of the ${NEWLIB_TARGET}/ prefix.
  spushd "${NEWLIB_INSTALL_DIR}"
  mkdir -p lib include
  mv -f ${NEWLIB_TARGET}/lib/* lib
  rm -rf  include/sys include/machine
  mv -f ${NEWLIB_TARGET}/include/* include
  rmdir ${NEWLIB_TARGET}/lib
  rmdir ${NEWLIB_TARGET}/include
  rmdir ${NEWLIB_TARGET}

  cp "${NACL_ROOT}/src/untrusted/pthread/pthread.h" \
    "${NACL_ROOT}/src/untrusted/pthread/semaphore.h" \
    include
  # Copy the nacl_random.h header, which is needed by the libc++ build. It
  # uses the IRT so should be safe to include in the toolchain tarball.
  mkdir include/nacl
  cp "${NACL_ROOT}/src/untrusted/nacl/nacl_random.h" \
    include/nacl
  spopd
}

libs-support() {
  StepBanner "LIBS-SUPPORT"
  libs-support-newlib-crt1 portable
  libs-support-newlib-crt1 x86-64
  libs-support-bitcode portable
  libs-support-bitcode x86-64

  local arch
  for arch in arm x86-32 x86-64 mips32 x86-32-nonsfi; do
    libs-support-native ${arch}
  done

  libs-support-unsandboxed
}

libs-support-newlib-crt1() {
  local arch=$1
  setup-biased-bitcode-env ${arch}
  mkdir -p "${INSTALL_LIB}"
  spushd "${PNACL_SUPPORT}/bitcode"

  StepBanner "LIBS-SUPPORT-NEWLIB" \
             "Install crt1.x & crt1_for_eh.x (linker scripts)"
  # Two versions of crt1.x exist, for different scenarios (with and without EH).
  # See: https://code.google.com/p/nativeclient/issues/detail?id=3069
  cp crt1.x "${INSTALL_LIB}"/crt1.x
  cp crt1_for_eh.x "${INSTALL_LIB}"/crt1_for_eh.x
  spopd
}

libs-support-bitcode() {
  local arch=$1
  setup-biased-bitcode-env ${arch}
  local build_dir="${TC_BUILD}/libs-support-bitcode-${arch}"
  local cc_cmd="${PNACL_CC} -Wall -Werror -O2 ${BIASED_BC_CFLAGS}"

  mkdir -p "${build_dir}"
  spushd "${PNACL_SUPPORT}/bitcode"
  # Install crti.bc (empty _init/_fini)
  StepBanner "LIBS-SUPPORT" "Install ${arch} crti.bc"
  ${cc_cmd} -c crti.c -o "${build_dir}"/crti.bc

  # Install crtbegin bitcode (__cxa_finalize for C++)
  StepBanner "LIBS-SUPPORT" "Install ${arch} crtbegin.bc"
  # NOTE: we do not have an "end" version of ththis
  ${cc_cmd} -c crtbegin.c -o "${build_dir}"/crtbegin.bc

  # Install unwind_stubs.bc (stubs for _Unwind_* functions when libgcc_eh
  # is not included in the native link).
  StepBanner "LIBS-SUPPORT" "Install ${arch} unwind_stubs.bc"
  ${cc_cmd} -c unwind_stubs.c -o "${build_dir}"/unwind_stubs.bc
  StepBanner "LIBS-SUPPORT" "Install ${arch} sjlj_eh_redirect.bc"
  ${cc_cmd} -c sjlj_eh_redirect.c -o "${build_dir}"/sjlj_eh_redirect.bc

  # Install libpnaclmm.a (__atomic_*).
  StepBanner "LIBS-SUPPORT" "Install ${arch} libpnaclmm.a"
  ${cc_cmd} -c pnaclmm.c -o "${build_dir}"/pnaclmm.bc
  ${PNACL_AR} rc "${build_dir}"/libpnaclmm.a "${build_dir}"/pnaclmm.bc

  spopd

  # Install to actual lib directories.
  spushd "${build_dir}"
  local files="crti.bc crtbegin.bc libpnaclmm.a unwind_stubs.bc sjlj_eh_redirect.bc"
  mkdir -p "${INSTALL_LIB}"
  cp -f ${files} "${INSTALL_LIB}"
  spopd
}

libs-support-native() {
  local arch=$1
  local destdir="${INSTALL_LIB_NATIVE}"${arch}
  local label="LIBS-SUPPORT (${arch})"
  mkdir -p "${destdir}"

  local flags="--pnacl-allow-native --pnacl-allow-translate -Wall -Werror"
  local cc_cmd="${PNACL_CC} -arch ${arch} --pnacl-bias=${arch} ${flags} \
      -I../../.. -O3"

  spushd "${PNACL_SUPPORT}"

  # NOTE: one of the reasons why we build these native is
  #       the shim library on x86-64
  #       c.f.: https://sites.google.com/a/google.com/nativeclient/project-pnacl/c-exception-handling
  # Compile crtbegin.o / crtend.o
  StepBanner "${label}" "Install crtbegin.o / crtend.o"

  # Build two versions of crtbegin: one that relies on later linking with
  # libgcc_eh and one that doesn't.
  # See: https://code.google.com/p/nativeclient/issues/detail?id=3069
  ${cc_cmd} -c crtbegin.c -o "${destdir}"/crtbegin.o
  ${cc_cmd} -c crtbegin.c -DLINKING_WITH_LIBGCC_EH \
            -o "${destdir}"/crtbegin_for_eh.o
  ${cc_cmd} -c crtend.c -o "${destdir}"/crtend.o

  # Make libcrt_platform.a
  StepBanner "${label}" "Install libcrt_platform.a"
  local tmpdir="${TC_BUILD}/libs-support-native"
  rm -rf "${tmpdir}"
  mkdir -p "${tmpdir}"
  ${cc_cmd} -c pnacl_irt.c -o "${tmpdir}"/pnacl_irt.o
  ${cc_cmd} -c relocate.c -o "${tmpdir}"/relocate.o

  local setjmp_arch="$arch"
  if [ "$setjmp_arch" = "x86-32-nonsfi" ]; then
    setjmp_arch=x86-32
  fi
  ${cc_cmd} -c setjmp_${setjmp_arch/-/_}.S -o "${tmpdir}"/setjmp.o

  # Some of the support code lives in third_party/ because it's based on code
  # from other open-source projects.
  ${cc_cmd} \
    -c "${NACL_SRC_THIRD_PARTY_MOD}/pnacl_native_newlib_subset/string.c" \
    -std=c99 -o "${tmpdir}"/string.o
  # Pull in the no-errno __ieee754_fmod from newlib and rename it to fmod.
  # This is to support the LLVM frem instruction.
  ${cc_cmd} \
    -c "${TC_SRC_NEWLIB}/newlib/libm/math/e_fmod.c" \
    -I"${TC_SRC_NEWLIB}/newlib/libm/common/" \
    -D__ieee754_fmod=fmod \
    -std=c99 -o "${tmpdir}"/e_fmod.o
  ${cc_cmd} \
    -c "${TC_SRC_NEWLIB}/newlib/libm/math/ef_fmod.c" \
    -I"${TC_SRC_NEWLIB}/newlib/libm/common/" \
    -D__ieee754_fmodf=fmodf \
    -std=c99 -o "${tmpdir}"/ef_mod.o

  # For ARM, also compile aeabi_read_tp.S
  if  [ ${arch} == arm ] ; then
    ${cc_cmd} -c aeabi_read_tp.S -o "${tmpdir}"/aeabi_read_tp.o
  fi
  spopd

  ${PNACL_AR} rc "${destdir}"/libcrt_platform.a "${tmpdir}"/*.o
}

libs-support-unsandboxed() {
  if ${BUILD_PLATFORM_LINUX} || ${BUILD_PLATFORM_MAC}; then
    local arch="x86-32-${BUILD_PLATFORM}"
    StepBanner "LIBS-SUPPORT (${arch})" "Install unsandboxed_irt.o"
    local destdir="${INSTALL_LIB_NATIVE}"${arch}
    mkdir -p ${destdir}
    # The NaCl headers insist on having a platform macro such as
    # NACL_LINUX defined, but unsandboxed_irt.c does not itself use
    # any of these macros, so defining NACL_LINUX here even on
    # non-Linux systems is OK.
    gcc -m32 -O2 -Wall -Werror -I${NACL_ROOT}/.. -DNACL_LINUX=1 -c \
        ${PNACL_SUPPORT}/unsandboxed_irt.c -o ${destdir}/unsandboxed_irt.o
  fi
}


# Build the dummy "libpnacl_irt_shim.a", which is useful for building
# commandline programs.  It cannot be used to build PPAPI programs
# because it does not actually shim the PPAPI interfaces.
# The library is named the same as the real PPAPI shim to ensure that
# the commandlines are the same.
# This must be built after newlib(), since it uses headers like <stdint.h>.
dummy-irt-shim() {
  local arch=$1
  local destdir="${INSTALL_LIB_NATIVE}"${arch}
  local label="DUMMY-IRT-SHIM (${arch})"
  mkdir -p "${destdir}"

  local flags="--pnacl-allow-native --pnacl-allow-translate -Wall -Werror"
  local cc_cmd="${PNACL_CC} -arch ${arch} ${flags}"

  spushd "${PNACL_SUPPORT}"
  StepBanner "${label}" "Install libpnacl_irt_shim_dummy.a"
  local tmpdir="${TC_BUILD}/dummy-irt-shim"
  rm -rf "${tmpdir}"
  mkdir -p "${tmpdir}"
  ${cc_cmd} -c dummy_shim_entry.c -o "${tmpdir}"/dummy_shim_entry.o
  spopd

  ${PNACL_AR} rc "${destdir}"/libpnacl_irt_shim_dummy.a "${tmpdir}"/*.o
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
  SDK_IS_SETUP=true

  SDK_INSTALL_ROOT="${INSTALL_ROOT}/sdk"
  SDK_INSTALL_LIB="${SDK_INSTALL_ROOT}/lib"
  SDK_INSTALL_INCLUDE="${SDK_INSTALL_ROOT}/include"
}

sdk() {
  sdk-setup "$@"
  StepBanner "SDK"
  sdk-clean
  sdk-headers
  sdk-libs
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

  StepBanner "SDK" "Install headers"
  spushd "${NACL_ROOT}"
  # TODO(pnacl-team): remove this pnaclsdk_mode once we have a better story
  # about host binary type (x86-32 vs x86-64).  SCons only knows how to use
  # x86-32 host binaries right now, so we need pnaclsdk_mode to override that.
  RunWithLog "sdk.headers" \
      ./scons \
      "${SCONS_ARGS[@]}" \
      ${extra_flags} \
      platform=${neutral_platform} \
      pnaclsdk_mode="custom:${INSTALL_ROOT}" \
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

  spushd "${NACL_ROOT}"
  # See above TODO about pnaclsdk_mode.
  RunWithLog "sdk.libs.bitcode" \
      ./scons \
      "${SCONS_ARGS[@]}" \
      ${extra_flags} \
      platform=${neutral_platform} \
      pnaclsdk_mode="custom:${INSTALL_ROOT}" \
      install_lib \
      libdir="$(PosixToSysPath "${SDK_INSTALL_LIB}")"
  spopd
}

# This builds lib*_private.a, to allow building the llc and ld nexes without
# the IRT and without the segment gap.
sdk-private-libs() {
  sdk-setup "$@"
  StepBanner "SDK" "Private (non-IRT) libs"
  spushd "${NACL_ROOT}"

  local neutral_platform="x86-32"
  # See above TODO about pnaclsdk_mode.
  RunWithLog "sdk.libs_private.bitcode" \
    ./scons \
    -j${PNACL_CONCURRENCY} \
    bitcode=1 \
    platform=${neutral_platform} \
    pnaclsdk_mode="custom:${INSTALL_ROOT}" \
    --verbose \
    libnacl_sys_private \
    libpthread_private \
    libnacl_dyncode_private \
    libplatform \
    libimc \
    libimc_syscalls \
    libsrpc \
    libgio

  local out_dir_prefix="${SCONS_OUT}"/nacl-x86-32-pnacl-pexe-clang
  local outdir="${out_dir_prefix}"/lib
  mkdir -p "${SDK_INSTALL_LIB}"
  cp "${outdir}"/lib*_private.a \
     "${outdir}"/lib{platform,imc,imc_syscalls,srpc,gio}.a "${SDK_INSTALL_LIB}"
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
      ${GIT} checkout "$(basename "${NEWLIB_INCLUDE_DIR}")"
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


#+-------------------------------------------------------------------------
#@ driver                - Install driver scripts.
driver() {
  StepBanner "DRIVER"
  driver-install
}

# install python scripts and redirector shell/batch scripts
driver-install-python() {
  local destdir="$1"
  shift
  local pydir="${destdir}/pydir"

  StepBanner "DRIVER" "Installing driver adaptors to ${destdir}"
  rm -rf "${destdir}"
  mkdir -p "${destdir}"
  mkdir -p "${pydir}"

  spushd "${DRIVER_DIR}"

  # Copy python scripts
  cp $@ driver_log.py driver_env.py driver_temps.py \
    *tools.py filetype.py loader.py "${pydir}"

  # Install redirector shell/batch scripts
  for name in $@; do
    local dest="${destdir}/${name/.py}"
    # In some situations cygwin cp messes up the permissions of the redirector
    # shell/batch scripts. Using cp -a seems to make sure it preserves them.
    cp -a redirect.sh "${dest}"
    chmod +x "${dest}"
    if ${BUILD_PLATFORM_WIN}; then
      cp -a redirect.bat "${dest}".bat
    fi
  done
  spopd
}

feature-version-file-install() {
  # Scons tests can check this version number to decide whether to
  # enable tests for toolchain bug fixes or new features.  This allows
  # tests to be enabled on the toolchain buildbots/trybots before the
  # new toolchain version is rolled into TOOL_REVISIONS (i.e. before
  # the tests would pass on the main NaCl buildbots/trybots).
  #
  # If you are adding a test that depends on a toolchain change, you
  # can increment this version number manually.
  echo 4 > "${INSTALL_ROOT}/FEATURE_VERSION"
}

# The driver is a simple python script which changes its behavior
# depending on the name it is invoked as.
driver-install() {
  local bindir=bin
  # On Linux we ship a fat toolchain with 2 sets of binaries defaulting to
  # x86-32 (mostly because of the 32 bit chrome bots). So the default
  # bin dir is 32, and the bin64 driver runs the 64 bit binaries
  if ${HOST_ARCH_X8664} && ${BUILD_PLATFORM_LINUX}; then
    bindir="bin64"
    # We want to be able to locally test a toolchain on 64 bit hosts without
    # building it twice and without extra env vars. So if a 32 bit toolchain
    # has not already been built, just symlink the bin dirs together.
    if [[ ! -d "${INSTALL_BIN}" ]]; then
      mkdir -p "${INSTALL_ROOT}"
      ln -s ${bindir} "${INSTALL_BIN}"
    fi
  fi

  # This directory (the ${INSTALL_ROOT}/${bindir} part)
  # should be kept in sync with INSTALL_BIN et al.
  local destdir="${INSTALL_ROOT}/${bindir}"

  driver-install-python "${destdir}" "pnacl-*.py"

  # Tell the driver the library mode and host arch
  echo """HAS_FRONTEND=1
HOST_ARCH=${HOST_ARCH}""" > "${destdir}"/driver.conf

  # On windows, copy the cygwin DLLs needed by the driver tools
  if ${BUILD_PLATFORM_WIN}; then
    StepBanner "DRIVER" "Copying cygwin libraries"
    local deps="gcc_s-1 iconv-2 win1 intl-8 stdc++-6 z"
    for name in ${deps}; do
      cp "/bin/cyg${name}.dll" "${destdir}"
    done
  fi

  # Install a REV file so that "pnacl-clang --version" knows the version
  # of the drivers themselves.
  DumpAllRevisions > "${destdir}/REV"

  feature-version-file-install
}

#@ driver-install-translator - Install driver scripts for translator component
driver-install-translator() {
  local destdir="${INSTALL_TRANSLATOR}/bin"

  driver-install-python "${destdir}" pnacl-translate.py pnacl-nativeld.py

  echo """HAS_FRONTEND=0""" > "${destdir}"/driver.conf
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

DumpAllRevisions() {
  one-line-rev-info ${NACL_ROOT}
  for d in ${PNACL_GIT_ROOT}/*/ ; do
    one-line-rev-info $d
  done
}

######################################################################
######################################################################
#     < VERIFY >
######################################################################
######################################################################

# Note: we could replace this with a modified version of tools/elf_checker.py
#       if we do not want to depend on binutils
readonly NACL_OBJDUMP=${BINUTILS_INSTALL_DIR}/bin/${REAL_CROSS_TARGET}-objdump

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
  if ${PNACL_DIS} "$1" -o - | grep asm ; then
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
  if ! grep -q "Tag_FP_arch: VFPv3" <<< ${arch_info} ; then
    echo "ERROR $1 - bad Tag_FP_arch"
    #TODO(robertm): figure out what the right thing to do is here, c.f.
    # http://code.google.com/p/nativeclient/issues/detail?id=966
    "${PNACL_READELF}" -A $1 | grep  Tag_FP_arch
    exit -1
  fi

  if ! grep -q "Tag_CPU_arch: v7" <<< ${arch_info} ; then
    echo "FAIL bad $1 Tag_CPU_arch"
    "${PNACL_READELF}" -A $1 | grep Tag_CPU_arch
    exit -1
  fi

  if ! grep -q "Tag_Advanced_SIMD_arch: NEONv1" <<< ${arch_info} ; then
    echo "FAIL bad $1 Tag_Advanced_SIMD_arch"
    "${PNACL_READELF}" -A $1 | grep Tag_Advanced_SIMD_arch
  fi

  # Check that the file uses the ARM hard-float ABI (where VFP
  # registers D0-D7 (s0-s15) are used to pass arguments and results).
  if ! grep -q "Tag_ABI_VFP_args: VFP registers" <<< ${arch_info} ; then
    echo "FAIL bad $1 Tag_ABI_VFP_args"
    "${PNACL_READELF}" -A $1 | grep Tag_ABI_VFP_args
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

#+ verify-bitcode-dir    - Verify that the files in a directory are bitcode.
verify-bitcode-dir() {
  local dir="$1"
  # This avoids errors when * finds no matches.
  shopt -s nullglob
  SubBanner "VERIFY: ${dir}"
  for i in "${dir}"/*.a ; do
    verify-archive-llvm "$i"
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
#+ verify                - Verifies that the pnacl-untrusted ELF files
#+                         are of the correct architecture.
verify() {
  StepBanner "VERIFY"
  verify-bitcode
  verify-native
}

verify-bitcode() {
  verify-bitcode-dir "${INSTALL_LIB}"
}

verify-native() {
  local arch
  for arch in arm x86-32 x86-64; do
    verify-native-dir ${arch} "${INSTALL_LIB_NATIVE}${arch}"
  done
}

#+ verify-triple-build <arch>
#+     Verify that the sandboxed translator produces an identical
#+     translation of itself (pnacl-llc.pexe) as the unsandboxed translator.
#+     (NOTE: This function is experimental/untested)
verify-triple-build() {
  local arch=$1
  StepBanner "VERIFY" "Verifying triple build for ${arch}"

  local bindir="$(GetTranslatorInstallDir ${arch})/bin"
  local llc_nexe="${bindir}/pnacl-llc.nexe"
  local llc_pexe="${bindir}/pnacl-llc.pexe"
  assert-file "${llc_nexe}" "sandboxed llc for ${arch} does not exist"
  assert-file "${llc_pexe}" "pnacl-llc.pexe does not exist"

  local flags="--pnacl-sb --pnacl-driver-verbose"

  if [ ${arch} == "arm" ] ; then
    # Use emulator if we are not on ARM
    local hostarch=$(uname -m)
    if ! [[ "${BUILD_ARCH}" =~ arm ]]; then
      flags+=" --pnacl-use-emulator"
    fi
  fi

  local triple_install_dir="$(GetTranslatorInstallDir ${arch})/triple-build"
  mkdir -p ${triple_install_dir}
  local new_llc_nexe="${triple_install_dir}/pnacl-llc.rebuild.nexe"
  mkdir -p "${triple_install_dir}"
  StepBanner "VERIFY" "Translating ${llc_pexe} using sandboxed tools (${arch})"
  local sb_translator="${INSTALL_TRANSLATOR}/bin/pnacl-translate"
  RunWithLog "verify.triple.build" \
    "${sb_translator}" ${flags} -arch ${arch} "${llc_pexe}" -o "${new_llc_nexe}"

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
  if [ -f ${TOOLCHAIN_BASE}/arm_trusted/ld_script_arm_trusted ]; then
    return 0
  else
    return 1
  fi
}

check-for-trusted() {
  if ! ${PNACL_BUILD_ARM} ; then
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
# Generate chromium perf bot logs for tracking the size of a binary.
#
print-tagged-tool-sizes() {
  local tag="$1"
  local binary="$2"

  # size output look like:
  #    text   data     bss     dec    hex  filename
  #  354421  16132  168920  539473  83b51  .../tool
  local sizes=($(${PNACL_SIZE} -B "${binary}" | grep '[0-9]\+'))
  echo "RESULT ${tag}: text= ${sizes[0]} bytes"
  echo "RESULT ${tag}: data= ${sizes[1]} bytes"
  echo "RESULT ${tag}: bss= ${sizes[2]} bytes"
  echo "RESULT ${tag}: total= ${sizes[3]} bytes"
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

"$@"
