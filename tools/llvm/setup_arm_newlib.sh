#!/bin/bash

# This script compiles newlib for arm (to use as an sdk library).
# It currently uses the code sourcery toolchain.

set -o nounset
set -o errexit

source $(dirname $0)/tools.sh

readonly NACL_ROOT=$(pwd)

readonly NEWLIB_INSTALL="${NACL_ROOT}/src/third_party/nacl_sdk/arm-newlib"
readonly NEWLIB_EXTRA_HEADER="${NEWLIB_INSTALL}/newlib_extra_header"
readonly NEWLIB_MARCH="armv6"
readonly NEWLIB_TARBALL=${NACL_ROOT}/../third_party/newlib/newlib-1.17.0.tar.gz
readonly NEWLIB_TARGET=arm-none-linux-gnueabi

###########################################################
readonly SCRATCH_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/newlib.arm.XXXXXX")"

readonly OBJ_ROOT="${SCRATCH_ROOT}/build"
readonly SRC_ROOT="${SCRATCH_ROOT}/newlib-1.17.0"

readonly MAKE_OPTS="${MAKE_OPTS:--j6}"

# Override constants from tools.sh:

# we do not expect newlib to build any binaries
readonly LD_FOR_TARGET="${ILLEGAL_TOOL}"

# we do not expect newlib to use objdump
readonly OBJDUMP_FOR_TARGET="${ILLEGAL_TOOL}"

# we do not expect newlib to use the assembler
readonly AS_FOR_TARGET="${ILLEGAL_TOOL}"

# we do not expect newlib to use strip
readonly STRIP_FOR_TARGET="${ILLEGAL_TOOL}"

######################################################################
# Helpers
######################################################################

Banner() {
  echo "**********************************************************************"
  echo -e $*
  echo "**********************************************************************"
}


runCommand() {
  local msg=$1
  local log=$2
  shift 2
  Banner "${msg}; log in ${log}"
  echo "=> Running: $@"
  "$@"  2>&1 | tee ${log}
  if [[ ${PIPESTATUS[0]} != 0 ]] ; then
    echo "runCommand failed"
    exit -1
  fi
}


######################################################################
#
######################################################################

checkCrosstoolBinaries() {
  for tool in ${CC_FOR_TARGET} \
              ${CXX_FOR_TARGET} \
              ${AS_FOR_TARGET} \
              ${LD_FOR_TARGET} \
              ${AR_FOR_TARGET} \
              ${NM_FOR_TARGET} \
              ${OBJDUMP_FOR_TARGET} \
              ${RANLIB_FOR_TARGET} \
              ${STRIP_FOR_TARGET}; do
    if [[ ! -x $tool ]]; then
      echo "Required binary $tool not found; install crosstools first."
      echo "Exiting."
      exit 1
    fi
  done
}

checkForExistingNewlib() {
  if [[ -d ${NEWLIB_INSTALL} ]]; then
    Banner "ERROR: library seems to be already installed in \n
  ${NEWLIB_INSTALL}"

    if tty >/dev/null; then
      echo -n "Would you like to delete it? [yN] "
      read yn
      case "$yn" in
        [Yy]) rm -fr ${NEWLIB_INSTALL} ;;
        *) exit -1;;
      esac
    fi
  fi
}

untarNewlib() {
  Banner "untaring to ${SCRATCH_ROOT}"
  mkdir -p ${SCRATCH_ROOT}
  tar zx -C ${SCRATCH_ROOT} -f ${NEWLIB_TARBALL}
}


# copies and rewrites some nacl specific system headers
exportHeader () {
  ${NACL_ROOT}/src/trusted/service_runtime/export_header.py \
    ${NACL_ROOT}/src/trusted/service_runtime/include $1
}


copyHeaders() {
  # copy nacl headers
  exportHeader ${SRC_ROOT}/newlib/libc/sys/nacl

  rm -rf ${NEWLIB_EXTRA_HEADER}
  mkdir -p ${NEWLIB_EXTRA_HEADER}

  # copy some elementary toolchain headers to a special place for newlib
  # NOTE: it is puzzling that we copy these headers from the code sourcery
  #       directory
  # TODO(robertm):  check whether they can be found in the llvm dirs
  cp ${CS_ROOT}/lib/gcc/arm-none-linux-gnueabi/4.2.1/include/*.h \
     ${CS_ROOT}/lib/gcc/arm-none-linux-gnueabi/4.2.1/install-tools/include/*.h \
     ${NEWLIB_EXTRA_HEADER}

  # copy some elementary toolchain c++ headers
  cp -r ${CS_ROOT}/arm-none-linux-gnueabi/include/c++ ${NEWLIB_EXTRA_HEADER}/

  # disable wchar and mbstate which is made available by the underlying code
  # sourcery toolchain but not appreciated by newlib
  local CPP_CFG=\
"${NEWLIB_EXTRA_HEADER}/c++/4.2.1/arm-none-linux-gnueabi/bits/c++config.h"
  cp ${CPP_CFG} ${CPP_CFG}.orig
  egrep -v 'WCHAR|MBSTATE' ${CPP_CFG}.orig > ${CPP_CFG}
}


patchNewlib() {
  local patch_file=$(readlink -f tools/patches/newlib-1.17.0_arm.patch)
  echo   "${SRC_ROOT}"
  Banner "patching newlib using ${patch_file}"
  # remove -fshort-enums
  readonly patch1="${SRC_ROOT}/newlib/libc/stdio/Makefile.am"
  mv ${patch1} ${patch1}.orig
  sed -e 's/-fshort-enums//' < ${patch1}.orig >  ${patch1}

  readonly patch2="${SRC_ROOT}/newlib/libc/stdio/Makefile.in"
  mv ${patch2} ${patch2}.orig
  sed -e 's/-fshort-enums//' < ${patch2}.orig >  ${patch2}

  # replace setjmp assembler code with an empty file
  readonly patch3="${SRC_ROOT}/newlib/libc/machine/arm/Makefile.in"
  mv ${patch3} ${patch3}.orig
  sed -e 's/setjmp.S/setjmp.c/g'  < ${patch3}.orig >  ${patch3}
  touch ${SRC_ROOT}/newlib/libc/machine/arm/setjmp.c

  # patch some more files using the traditional patch tool
  patch -d "${SRC_ROOT}" -p1 < ${patch_file}

  # remove newlib functions - we have our own dummy
  for s in ${SRC_ROOT}/newlib/libc/syscalls/*.c ; do
    mv ${s} ${s}.orig
    touch ${s}
  done
}


configureNewlib() {
  mkdir -p ${OBJ_ROOT}
  cd ${OBJ_ROOT}
  # NOTE: there were quoting issues with this which is why
  #       it is handled separately from the other env vars
  local CFLAGS_FOR_TARGET="-march=${NEWLIB_MARCH} \
                           -nostdinc \
                           -DMISSING_SYSCALL_NAMES=1 \
                           -ffixed-r9 \
                           -D__native_client__=1 \
                           -isystem ${NEWLIB_EXTRA_HEADER}"

  runCommand "Configuring newlib" \
    ${OBJ_ROOT}/configure.log \
    env -i \
    PATH="/usr/bin:/bin" \
    CFLAGS_FOR_TARGET="${CFLAGS_FOR_TARGET}" \
    CC_FOR_TARGET="${CC_FOR_TARGET}" \
    CXX_FOR_TARGET="${CXX_FOR_TARGET}" \
    AS_FOR_TARGET="${AS_FOR_TARGET}" \
    LD_FOR_TARGET="${LD_FOR_TARGET}" \
    AR_FOR_TARGET="${AR_FOR_TARGET}" \
    NM_FOR_TARGET="${NM_FOR_TARGET}" \
    OBJDUMP_FOR_TARGET=${OBJDUMP_FOR_TARGET} \
    RANLIB_FOR_TARGET=${RANLIB_FOR_TARGET} \
    STRIP_FOR_TARGET=${STRIP_FOR_TARGET} \
    ${SRC_ROOT}/configure \
    --disable-libgloss \
    --disable-multilib \
    --enable-newlib-reent-small \
    --prefix="${NEWLIB_INSTALL}" \
    --disable-newlib-supplied-syscalls \
    --disable-texinfo \
    --target="${NEWLIB_TARGET}"
}


buildNewlib() {
  cd ${OBJ_ROOT}
  runCommand "Building newlib" ${OBJ_ROOT}/make.log make ${MAKE_OPTS}
}


installNewlib() {
  cd ${OBJ_ROOT}
  runCommand "Installing" ${OBJ_ROOT}/install.log make install

  exportHeader ${NEWLIB_INSTALL}/${NEWLIB_TARGET}/include

  # we provide our own version on pthread.h later
  rm -f ${NEWLIB_INSTALL}/${NEWLIB_TARGET}/include/pthread.h
}


cleanUp() {
  Banner "removing ${SCRATCH_ROOT}"
  rm -rf ${SCRATCH_ROOT}
}

######################################################################
# main()
######################################################################

echo "Building in ${OBJ_ROOT} (logs will be there)"
echo "Installing in ${NEWLIB_INSTALL}"

checkCrosstoolBinaries

if [[ $(basename ${NACL_ROOT}) != "native_client" ]] ; then
  echo "ERROR: you must be in the native_client dir to execute this script"
  exit -1
fi

# NOTE: comment out these four lines to rebuild the unpacked newlib
# tree after it has been hand-modified.
checkForExistingNewlib
untarNewlib
copyHeaders
patchNewlib

configureNewlib

buildNewlib

installNewlib

cleanUp

echo "Done."
