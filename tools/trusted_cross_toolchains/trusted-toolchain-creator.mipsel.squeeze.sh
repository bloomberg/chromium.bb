#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#@ This script creates the mips trusted SDK.
#@ It must be run from the native_client directory.

# This script is intended to build a mipsel-linux-gnu cross compilation
# toolchain that runs on x86 linux and generates code for a little-endian,
# hard-float, mips32 target.
#
# It expects the host machine to have relatively recent versions of GMP (4.2.0
# or later), MPFR (2.4.2), and MPC (0.8.1) in order to build the GCC.
#
# Common way to get those is:
# sudo apt-get install libmpfr-dev libmpc-dev libgmp3-dev

######################################################################
# Config
######################################################################

set -o nounset
set -o errexit
set -o xtrace

readonly MAKE_OPTS="-j8"
readonly ARCH="mips32"

readonly GCC_URL="http://ftp.gnu.org/gnu/gcc/gcc-4.7.2/gcc-4.7.2.tar.bz2"
readonly GCC_SHA1SUM="a464ba0f26eef24c29bcd1e7489421117fb9ee35"

readonly BINUTILS_URL="http://ftp.gnu.org/gnu/binutils/binutils-2.22.tar.bz2"
readonly BINUTILS_SHA1SUM="65b304a0b9a53a686ce50a01173d1f40f8efe404"

readonly KERNEL_URL="http://www.linux-mips.org/pub/linux/mips/kernel/v2.6/linux-2.6.38.4.tar.gz"
readonly KERNEL_SHA1SUM="377fa5cf5f1d0c396759b1c4d147330e7e5b6d7f"

readonly GDB_URL="http://ftp.gnu.org/gnu/gdb/gdb-7.5.tar.bz2"
readonly GDB_SHA1SUM="79b61152813e5730fa670c89e5fc3c04b670b02c"

readonly EGLIBC_SVN_URL="svn://svn.eglibc.org/branches/eglibc-2_14"
readonly EGLIBC_REVISION="20996"

readonly DOWNLOAD_QEMU_URL="http://download.savannah.gnu.org/releases/qemu/qemu-0.12.5.tar.gz"

readonly INSTALL_ROOT=$(pwd)/toolchain/linux_mips-trusted

readonly TMP=$(pwd)/toolchain/tmp/crosstool-trusted

readonly BUILD_DIR=${TMP}/build

readonly PATCH_MIPS32=$(readlink -f ../third_party/qemu/qemu-0.12.5.patch_mips)

readonly JAIL_MIPS32=${INSTALL_ROOT}/sysroot

# These are simple compiler wrappers to force 32bit builds.
readonly  CC32=$(readlink -f pnacl/scripts/mygcc32)
readonly  CXX32=$(readlink -f pnacl/scripts/myg++32)

######################################################################
# Helper
######################################################################

Banner() {
  echo "######################################################################"
  echo $*
  echo "######################################################################"
}

SubBanner() {
  echo "......................................................................"
  echo $*
  echo "......................................................................"
}

Usage() {
  echo
  echo "$0 trusted_sdk"
  echo
  echo "trusted_sdk - Build everything and package it"
  echo
}

CheckoutOrCopy() {
  local url=$1
  local revision=$2
  local filename="${TMP}/${url##*/}"
  local filetype="${url%%:*}"

  if [ "${filename}" == "" ]; then
    echo "Unknown error occured. Aborting."
    exit 1
  fi

  if [ "${filetype}" == "svn" ]; then
    SubBanner "checkout from ${url} -> ${filename}"
    svn --force export -r ${revision} ${url} ${filename}
  else
    SubBanner "copying from ${url}"
    cp ${url} ${filename}
  fi
}

DownloadOrCopy() {
  local url=$1
  local filename="${TMP}/${url##*/}"
  local filetype="${url%%:*}"

  if [ "${filename}" == "" ]; then
    echo "Unknown error occured. Aborting."
    exit 1
  fi

  if [[ "${filetype}" ==  "http" || ${filetype} ==  "https" ]] ; then
    if [ ! -f "${filename}" ]; then
      SubBanner "downloading from ${url} -> ${filename}"
      wget ${url} -O ${filename}
    fi
  else
    SubBanner "copying from ${url}"
    cp ${url} ${filename}
  fi
}

DownloadOrCopyAndVerify() {
  local url=$1
  local checksum=$2
  local filename="${TMP}/${url##*/}"
  local filetype="${url%%:*}"

  if [ "${filename}" == "" ]; then
    echo "Unknown error occured. Aborting."
    exit 1
  fi

  if [[ "${filetype}" ==  "http" || ${filetype} ==  "https" ]] ; then
    if [ ! -f "${filename}" ]; then
      SubBanner "downloading from ${url} -> ${filename}"
      wget ${url} -O ${filename}
    fi
    if [ "${checksum}" != "nochecksum" ]; then
      if [ "$(sha1sum ${filename} | cut -d ' ' -f 1)" != "${checksum}" ]; then
        echo "${filename} sha1sum failed. Deleting file and aborting."
        rm -f ${filename}
        exit 1
      fi
    fi
  else
    SubBanner "copying from ${url}"
    cp ${url} ${filename}
  fi
}

######################################################################
#
######################################################################

# some sanity checks to make sure this script is run from the right place
# with the right tools.
SanityCheck() {
  Banner "Sanity Checks"
  if [[ $(basename $(pwd)) != "native_client" ]] ; then
    echo "ERROR: run this script from the native_client/ dir"
    exit -1
  fi

  if ! mkdir -p "${INSTALL_ROOT}" ; then
     echo "ERROR: ${INSTALL_ROOT} can't be created."
    exit -1
  fi

  for tool in cleanlinks wget ; do
    if ! which ${tool} ; then
      echo "Required binary $tool not found."
      echo "Exiting."
      exit 1
    fi
  done
}


ClearInstallDir() {
  Banner "clearing dirs in ${INSTALL_ROOT}"
  rm -rf ${INSTALL_ROOT}/*
}

ClearBuildDir() {
  Banner "clearing dirs in ${BUILD_DIR}"
  rm -rf ${BUILD_DIR}/*
}

CreateTarBall() {
  local tarball=$1
  Banner "creating tar ball ${tarball}"
  tar cfz ${tarball}-mips.tgz -C ${INSTALL_ROOT} .
}


# Download the toolchain source tarballs or use a local copy when available.
DownloadOrCopyAndInstallToolchain() {
  Banner "Installing toolchain"

  local tarball="${TMP}/${BINUTILS_URL##*/}"
  DownloadOrCopyAndVerify ${BINUTILS_URL} ${BINUTILS_SHA1SUM}
  SubBanner "extracting from ${tarball}"
  tar jxf ${tarball} -C ${TMP}

  tarball="${TMP}/${GCC_URL##*/}"
  DownloadOrCopyAndVerify ${GCC_URL} ${GCC_SHA1SUM}
  SubBanner "extracting from ${tarball}"
  tar jxf ${tarball} -C ${TMP}

  tarball="${TMP}/${GDB_URL##*/}"
  DownloadOrCopyAndVerify ${GDB_URL} ${GDB_SHA1SUM}
  SubBanner "extracting from ${tarball}"
  tar jxf ${tarball} -C ${TMP}

  tarball="${TMP}/${KERNEL_URL##*/}"
  DownloadOrCopyAndVerify ${KERNEL_URL} ${KERNEL_SHA1SUM}
  SubBanner "extracting from ${tarball}"
  tar zxf ${tarball} -C ${TMP}

  local eglibc_dir="${TMP}/${EGLIBC_SVN_URL##*/}"
  CheckoutOrCopy ${EGLIBC_SVN_URL} ${EGLIBC_REVISION}


  Banner "Preparing the code"

  if [ ! -d "${TMP}/eglibc-2_14/libc/ports" ]; then
    ln -s ${TMP}/eglibc-2_14/ports ${TMP}/eglibc-2_14/libc/ports
  fi

  # Fix a minor syntax issue in tc-mips.c.
  local OLD_TEXT="as_warn_where (fragp->fr_file, fragp->fr_line, msg);"
  local NEW_TEXT="as_warn_where (fragp->fr_file, fragp->fr_line, \"%s\", msg);"
  local FILE_NAME="${TMP}/binutils-2.22/gas/config/tc-mips.c"
  sed -i "s/${OLD_TEXT}/${NEW_TEXT}/g" "${FILE_NAME}"

  export PATH=${INSTALL_ROOT}/bin:$PATH


  Banner "Building binutils"

  mkdir -p ${BUILD_DIR}/binutils/
  pushd ${BUILD_DIR}/binutils/

  SubBanner "Configuring"
  ${TMP}/binutils-2.22/configure \
    --prefix=${INSTALL_ROOT}     \
    --target=mipsel-linux-gnu    \
    --with-sysroot=${JAIL_MIPS32}

  SubBanner "Make"
  make ${MAKE_OPTS} all-binutils all-gas all-ld

  SubBanner "Install"
  make ${MAKE_OPTS} install-binutils install-gas install-ld

  popd


  Banner "Building GCC (initial)"

  mkdir -p ${BUILD_DIR}/gcc/initial
  pushd ${BUILD_DIR}/gcc/initial

  SubBanner "Configuring"
  ${TMP}/gcc-4.7.2/configure \
    --prefix=${INSTALL_ROOT} \
    --disable-libssp         \
    --disable-libgomp        \
    --disable-libmudflap     \
    --disable-fixed-point    \
    --disable-decimal-float  \
    --with-mips-plt          \
    --with-endian=little     \
    --with-arch=${ARCH}      \
    --enable-languages=c     \
    --with-newlib            \
    --without-headers        \
    --disable-shared         \
    --disable-threads        \
    --disable-libquadmath    \
    --disable-libatomic      \
    --target=mipsel-linux-gnu

  SubBanner "Make"
  make ${MAKE_OPTS} all

  SubBanner "Install"
  make ${MAKE_OPTS} install

  popd


  Banner "Installing Linux kernel headers"
  pushd ${TMP}/linux-2.6.38.4
  make headers_install ARCH=mips INSTALL_HDR_PATH=${JAIL_MIPS32}/usr
  popd


  Banner "Building EGLIBC (initial)"

  mkdir -p ${JAIL_MIPS32}/usr/lib
  mkdir -p ${BUILD_DIR}/eglibc/initial
  pushd ${BUILD_DIR}/eglibc/initial

  SubBanner "Configuring"
  BUILD_CC=gcc                      \
  AR=mipsel-linux-gnu-ar            \
  RANLIB=mipsel-linux-gnu-ranlib    \
  CC=mipsel-linux-gnu-gcc           \
  CXX=mipsel-linux-gnu-g++          \
  ${TMP}/eglibc-2_14/libc/configure \
    --prefix=/usr                   \
    --enable-add-ons                \
    --build=i686-pc-linux-gnu       \
    --host=mipsel-linux-gnu         \
    --disable-profile               \
    --without-gd                    \
    --without-cvs                   \
    --with-headers=${JAIL_MIPS32}/usr/include

  SubBanner "Install"
  make ${MAKE_OPTS} install-headers install_root=${JAIL_MIPS32} \
                    install-bootstrap-headers=yes

  make csu/subdir_lib  && \
       cp csu/crt1.o csu/crti.o csu/crtn.o ${JAIL_MIPS32}/usr/lib

  mipsel-linux-gnu-gcc -nostdlib      \
                       -nostartfiles  \
                       -shared        \
                       -x c /dev/null \
                       -o ${JAIL_MIPS32}/usr/lib/libc.so

  popd


  Banner "Building GCC (intermediate)"

  mkdir -p ${BUILD_DIR}/gcc/intermediate
  pushd ${BUILD_DIR}/gcc/intermediate

  SubBanner "Configuring"
  ${TMP}/gcc-4.7.2/configure  \
    --prefix=${INSTALL_ROOT}  \
    --disable-libssp          \
    --disable-libgomp         \
    --disable-libmudflap      \
    --disable-fixed-point     \
    --disable-decimal-float   \
    --with-mips-plt           \
    --with-endian=little      \
    --with-arch=${ARCH}       \
    --target=mipsel-linux-gnu \
    --enable-languages=c      \
    --disable-libquadmath     \
    --disable-libatomic       \
    --with-sysroot=${JAIL_MIPS32}

  SubBanner "Make"
  make ${MAKE_OPTS} all

  SubBanner "Install"
  make ${MAKE_OPTS} install

  popd


  Banner "Building EGLIBC (final)"

  mkdir -p ${BUILD_DIR}/eglibc/final
  pushd ${BUILD_DIR}/eglibc/final

  BUILD_CC=gcc                      \
  AR=mipsel-linux-gnu-ar            \
  RANLIB=mipsel-linux-gnu-ranlibi   \
  CC=mipsel-linux-gnu-gcc           \
  CXX=mipsel-linux-gnu-g++          \
  ${TMP}/eglibc-2_14/libc/configure \
    --prefix=/usr                   \
    --enable-add-ons                \
    --host=mipsel-linux-gnu         \
    --disable-profile               \
    --without-gd                    \
    --without-cvs                   \
    --build=i686-pc-linux-gnu       \
    --with-headers=${JAIL_MIPS32}/usr/include

  SubBanner "Make"
  make ${MAKE_OPTS} all

  SubBanner "Install"
  make ${MAKE_OPTS} install install_root=${JAIL_MIPS32}

  popd


  Banner "Building GCC (final)"

  mkdir -p ${BUILD_DIR}/gcc/final
  pushd ${BUILD_DIR}/gcc/final

  ${TMP}/gcc-4.7.2/configure  \
    --prefix=${INSTALL_ROOT}  \
    --disable-libssp          \
    --disable-libgomp         \
    --disable-libmudflap      \
    --disable-fixed-point     \
    --disable-decimal-float   \
    --with-mips-plt           \
    --with-endian=little      \
    --with-arch=${ARCH}       \
    --target=mipsel-linux-gnu \
    --enable-__cxa_atexit     \
    --enable-languages=c,c++  \
    --with-sysroot=${JAIL_MIPS32}

  SubBanner "Make"
  make ${MAKE_OPTS} all

  SubBanner "Install"
  make ${MAKE_OPTS} install

  popd


  Banner "Building GDB"

  mkdir -p ${BUILD_DIR}/gdb/
  pushd ${BUILD_DIR}/gdb/

  ${TMP}/gdb-7.5/configure   \
    --prefix=${INSTALL_ROOT} \
    --target=mipsel-linux-gnu

  SubBanner "Make"
  make ${MAKE_OPTS} all-gdb

  SubBanner "Install"
  make ${MAKE_OPTS} install-gdb

  popd
}


InstallTrustedLinkerScript() {
  local trusted_ld_script=${INSTALL_ROOT}/ld_script_mips_trusted
  # We are using the output of "ld --verbose" which contains
  # the linker script delimited by "=========".
  # We are changing the image start address to 70000000
  # to move the sel_ldr and other images "out of the way"
  Banner "installing trusted linker script to ${trusted_ld_script}"

  ${INSTALL_ROOT}/bin/mipsel-linux-gnu-ld  --verbose |\
      grep -A 10000 "=======" |\
      grep -v "=======" |\
      sed -e 's/0400000/70000000/g' > ${trusted_ld_script}
}


# ----------------------------------------------------------------------
# mips32 deb files to complete our code sourcery jail
# ----------------------------------------------------------------------

readonly REPO_DEBIAN=http://ftp.debian.org/debian
readonly MIPS32_PACKAGES=${REPO_DEBIAN}/dists/squeeze/main/binary-mipsel/Packages.bz2

readonly TMP_PACKAGELIST_MIPS32=${TMP}/../packagelist_mipsel.tmp

# These are good enough for native client.
readonly BASE_PACKAGES="\
  libssl0.9.8 \
  libssl-dev \
  libx11-6 \
  libx11-dev \
  x11proto-core-dev \
  libxt6 \
  libxt-dev \
  zlib1g \
  zlib1g-dev \
  libasound2 \
  libasound2-dev \
  libaa1 \
  libaa1-dev \
  libxau-dev \
  libxau6 \
  libxcb1 \
  libxcb1-dev \
  libxcb-render0 \
  libxcb-render0-dev \
  libxcb-render-util0 \
  libxcb-render-util0-dev \
  libxcb-shm0 \
  libxcb-shm0-dev \
  libxdmcp6 \
  libxdmcp-dev \
  libxss1 \
  libxss-dev"

GeneratePackageLists() {
  Banner "generating package lists for mips32"
  echo -n > ${TMP_PACKAGELIST_MIPS32}
  DownloadOrCopy ${MIPS32_PACKAGES}
  bzcat ${TMP}/Packages.bz2\
    | egrep '^(Package:|Filename:)' > ${TMP}/Packages_mipsel
  for pkg in ${BASE_PACKAGES} ; do
    grep  -A 1 "${pkg}\$" ${TMP}/Packages_mipsel\
      | egrep -o "pool/.*" >> ${TMP_PACKAGELIST_MIPS32}
  done
}

InstallMissingLibraries() {
  readonly DEP_FILES_NEEDED_MIPS32=$(cat ${TMP_PACKAGELIST_MIPS32})
  for file in ${DEP_FILES_NEEDED_MIPS32} ; do
    local package="${TMP}/${file##*/}"
    Banner "installing ${file}"
    DownloadOrCopy ${REPO_DEBIAN}/${file}
    SubBanner "extracting to ${JAIL_MIPS32}"
    dpkg --fsys-tarfile ${package}\
      | tar -xvf - --exclude=./usr/share -C ${JAIL_MIPS32}
  done

  Banner "some cleanup"

  pushd ${JAIL_MIPS32}/usr/lib/
  cleanlinks > /dev/null 2> /dev/null
  FixLibs
  popd
}

FixLibs() {
  Banner "Fixing libraries"

  rm -f libbz2.so
  ln -s ../../lib/libbz2.so.1 libbz2.so

  rm -f libm.so
  ln -s ../../lib/libm.so.6 libm.so

  rm -f libdl.so
  ln -s ../../lib/libdl.so.2 libdl.so

  rm -f librt.so
  ln -s ../../lib/librt.so.1 librt.so

  rm -f libpcre.so
  ln -s ../../lib/libpcre.so.3  libpcre.so

  rm -f libresolv.so
  ln -s ../../lib/libresolv.so.2  libresolv.so

  ln -s ../../../mipsel-linux-gnu/lib/libstdc++.so.6.0.17 .
  ln -s libstdc++.so.6.0.17 libstdc++.so.6
  ln -s libstdc++.so.6.0.17 libstdc++.so

  ln -s ../../../mipsel-linux-gnu/lib/libgcc_s.so.1 .
  ln -s libgcc_s.so.1 libgcc_s.so
}

BuildAndInstallQemu() {
  local saved_dir=$(pwd)
  local tmpdir="${TMP}/qemu-mips.nacl"
  local tarball="qemu-0.12.5.tar.gz"

  Banner "Building qemu in ${tmpdir}"

  rm -rf ${tmpdir}
  mkdir ${tmpdir}
  cd ${tmpdir}

  SubBanner "Downloading"
  wget -c ${DOWNLOAD_QEMU_URL}

  SubBanner "Untarring"
  tar zxf  ${tarball}
  cd qemu-0.12.5
  SubBanner "Patching"
  patch -p1 < ${PATCH_MIPS32}

  echo
  echo "NOTE: on 64 bit systems you will need to the following 32bit libs:"
  echo "lib32z1-dev"
  echo

  SubBanner "Configuring"
  env -i PATH=/usr/bin/:/bin \
    ./configure \
    --cc=${CC32} \
    --disable-system \
    --enable-linux-user \
    --disable-darwin-user \
    --disable-bsd-user \
    --target-list=mipsel-linux-user \
    --disable-sdl \
    --disable-linux-aio \
    --static

  SubBanner "Make"
  env -i PATH=/usr/bin/:/bin \
      make MAKE_OPTS=${MAKE_OPTS}

  SubBanner "Install"
  cp mipsel-linux-user/qemu-mipsel ${INSTALL_ROOT}/qemu-mips32
  cd ${saved_dir}
  cp tools/trusted_cross_toolchains/qemu_tool_mips32.sh ${INSTALL_ROOT}
  ln -sf qemu_tool_mips32.sh ${INSTALL_ROOT}/run_under_qemu_mips32
}

######################################################################
# Main
######################################################################

if [[ $# -eq 0 ]] ; then
  echo "you must specify a mode on the commandline:"
  echo
  Usage
  exit -1

elif [[ $1 == "trusted_sdk" ]]; then
  mkdir -p ${TMP}
  SanityCheck
  ClearInstallDir
  ClearBuildDir
  DownloadOrCopyAndInstallToolchain
  GeneratePackageLists
  InstallMissingLibraries
  InstallTrustedLinkerScript
  BuildAndInstallQemu
  CreateTarBall $1

else
  Usage
  exit -1

fi

