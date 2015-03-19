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

######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

readonly SCRIPT_DIR=$(dirname $0)

readonly MAKE_OPTS="-j8"
readonly ARCH="mips32"

readonly GMP_URL="http://ftp.gnu.org/gnu/gmp/gmp-5.1.3.tar.bz2"
readonly GMP_SHA1SUM="b35928e2927b272711fdfbf71b7cfd5f86a6b165"

readonly MPFR_URL="http://ftp.gnu.org/gnu/mpfr/mpfr-3.1.2.tar.bz2"
readonly MPFR_SHA1SUM="46d5a11a59a4e31f74f73dd70c5d57a59de2d0b4"

readonly MPC_URL="http://ftp.gnu.org/gnu/mpc/mpc-1.0.2.tar.gz"
readonly MPC_SHA1SUM="5072d82ab50ec36cc8c0e320b5c377adb48abe70"

readonly GCC_URL="http://ftp.gnu.org/gnu/gcc/gcc-4.9.0/gcc-4.9.0.tar.bz2"
readonly GCC_SHA1SUM="fbde8eb49f2b9e6961a870887cf7337d31cd4917"

readonly BINUTILS_URL="http://ftp.gnu.org/gnu/binutils/binutils-2.24.tar.bz2"
readonly BINUTILS_SHA1SUM="7ac75404ddb3c4910c7594b51ddfc76d4693debb"

readonly KERNEL_URL="http://www.linux-mips.org/pub/linux/mips/kernel/v3.x/linux-3.14.2.tar.gz"
readonly KERNEL_SHA1SUM="9b094e817a7a9b7c09b5bc268e23de642c6c407a"

readonly GDB_URL="http://ftp.gnu.org/gnu/gdb/gdb-7.7.1.tar.bz2"
readonly GDB_SHA1SUM="35228319f7c715074a80be42fff64c7645227a80"

readonly GLIBC_URL="http://ftp.gnu.org/gnu/glibc/glibc-2.19.tar.bz2"
readonly GLIBC_SHA1SUM="382f4438a7321dc29ea1a3da8e7852d2c2b3208c"

readonly DOWNLOAD_QEMU_URL="http://wiki.qemu-project.org/download/qemu-2.0.0.tar.bz2"

readonly INSTALL_ROOT=$(pwd)/toolchain/linux_x86/mips_trusted

readonly TMP=$(pwd)/toolchain/tmp/crosstool-trusted

readonly BUILD_DIR=${TMP}/build

readonly JAIL_MIPS32=${INSTALL_ROOT}/sysroot

readonly CROSS_TARBALL="chromesdk_linux_mipsel"

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
  echo "$0 <nacl_sdk|chrome_sdk>"
  echo
  echo "nacl_sdk - Build nacl toolchain and package it"
  echo "chrome_sdk - Build chrome toolchain and package it"
  echo
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

  for tool in wget ; do
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
  mkdir -p ${JAIL_MIPS32}
}

ClearBuildDir() {
  Banner "clearing dirs in ${BUILD_DIR}"
  rm -rf ${BUILD_DIR}/*
}

CreateTarBall() {
  local tarball=$1
  Banner "creating tar ball ${tarball}"
  tar cfz ${tarball}.tgz -C ${INSTALL_ROOT} .
}


# Download the toolchain source tarballs or use a local copy when available.
DownloadOrCopyAndInstallToolchain() {
  Banner "Installing toolchain"

  tarball="${TMP}/${GCC_URL##*/}"
  DownloadOrCopyAndVerify ${GCC_URL} ${GCC_SHA1SUM}
  SubBanner "extracting from ${tarball}"
  tar jxf ${tarball} -C ${TMP}

  pushd ${TMP}/gcc-*

  local tarball="${TMP}/${GMP_URL##*/}"
  DownloadOrCopyAndVerify ${GMP_URL} ${GMP_SHA1SUM}
  SubBanner "extracting from ${tarball}"
  tar jxf ${tarball}
  local filename=`ls | grep gmp\-`
  rm -f gmp
  ln -s ${filename} gmp
  # Fix gmp configure problem with flex.
  sed -i "s/m4-not-needed/m4/" gmp/configure

  local tarball="${TMP}/${MPFR_URL##*/}"
  DownloadOrCopyAndVerify ${MPFR_URL} ${MPFR_SHA1SUM}
  SubBanner "extracting from ${tarball}"
  tar jxf ${tarball}
  local filename=`ls | grep mpfr\-`
  rm -f mpfr
  ln -s ${filename} mpfr

  local tarball="${TMP}/${MPC_URL##*/}"
  DownloadOrCopyAndVerify ${MPC_URL} ${MPC_SHA1SUM}
  SubBanner "extracting from ${tarball}"
  tar zxf ${tarball}
  local filename=`ls | grep mpc\-`
  rm -f mpc
  ln -s ${filename} mpc

  popd

  local tarball="${TMP}/${BINUTILS_URL##*/}"
  DownloadOrCopyAndVerify ${BINUTILS_URL} ${BINUTILS_SHA1SUM}
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

  tarball="${TMP}/${GLIBC_URL##*/}"
  DownloadOrCopyAndVerify ${GLIBC_URL} ${GLIBC_SHA1SUM}
  SubBanner "extracting from ${tarball}"
  tar jxf ${tarball} -C ${TMP}


  Banner "Preparing the code"

  # Fix a minor syntax issue in tc-mips.c.
  local OLD_TEXT="as_warn_where (fragp->fr_file, fragp->fr_line, msg);"
  local NEW_TEXT="as_warn_where (fragp->fr_file, fragp->fr_line, \"%s\", msg);"
  local FILE_NAME="${TMP}/binutils-2.24/gas/config/tc-mips.c"
  sed -i "s/${OLD_TEXT}/${NEW_TEXT}/g" "${FILE_NAME}"

  export PATH=${INSTALL_ROOT}/bin:$PATH


  Banner "Building binutils"

  rm -rf ${BUILD_DIR}/binutils/
  mkdir -p ${BUILD_DIR}/binutils/
  pushd ${BUILD_DIR}/binutils/

  SubBanner "Configuring"
  ${TMP}/binutils-2.24/configure \
    --prefix=${INSTALL_ROOT}     \
    --target=mipsel-linux-gnu    \
    --with-sysroot=${JAIL_MIPS32}

  SubBanner "Make"
  make ${MAKE_OPTS} all-binutils all-gas all-ld

  SubBanner "Install"
  make ${MAKE_OPTS} install-binutils install-gas install-ld

  popd


  Banner "Building GCC (initial)"

  rm -rf ${BUILD_DIR}/gcc/initial
  mkdir -p ${BUILD_DIR}/gcc/initial
  pushd ${BUILD_DIR}/gcc/initial

  SubBanner "Configuring"
  ${TMP}/gcc-4.9.0/configure \
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
  pushd ${TMP}/linux-3.14.2
  make headers_install ARCH=mips INSTALL_HDR_PATH=${JAIL_MIPS32}/usr
  popd

  Banner "Building GLIBC"

  rm -rf ${BUILD_DIR}/glibc/final
  mkdir -p ${BUILD_DIR}/glibc/final
  pushd ${BUILD_DIR}/glibc/final

  BUILD_CC=gcc                      \
  AR=mipsel-linux-gnu-ar            \
  RANLIB=mipsel-linux-gnu-ranlibi   \
  CC=mipsel-linux-gnu-gcc           \
  CXX=mipsel-linux-gnu-g++          \
  ${TMP}/glibc-2.19/configure       \
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

  rm -rf ${BUILD_DIR}/gcc/final
  mkdir -p ${BUILD_DIR}/gcc/final
  pushd ${BUILD_DIR}/gcc/final

  ${TMP}/gcc-4.9.0/configure  \
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

  rm -rf ${BUILD_DIR}/gdb/
  mkdir -p ${BUILD_DIR}/gdb/
  pushd ${BUILD_DIR}/gdb/

  ${TMP}/gdb-7.7.1/configure   \
    --prefix=${INSTALL_ROOT} \
    --target=mipsel-linux-gnu

  SubBanner "Make"
  make ${MAKE_OPTS} all-gdb

  SubBanner "Install"
  make ${MAKE_OPTS} install-gdb

  popd
}


# ----------------------------------------------------------------------
# mips32 deb files to complete our code sourcery jail
# ----------------------------------------------------------------------

readonly REPO_DEBIAN=http://ftp.debian.org/debian
readonly MIPS32_PACKAGES=${REPO_DEBIAN}/dists/wheezy/main/binary-mipsel/Packages.bz2

readonly BASE_PACKAGELIST_MIPS32=${SCRIPT_DIR}/packagelist.wheezy.mipsel.base
readonly EXTRA_PACKAGELIST_MIPS32=${SCRIPT_DIR}/packagelist.wheezy.mipsel.extra
readonly TMP_BASE_PKG_MIPS32=${TMP}/packagelist.generated.wheezy.mipsel.base
readonly TMP_EXTRA_PKG_MIPS32=${TMP}/packagelist.generated.wheezy.mipsel.extra

GeneratePackageLists() {
  local sdk_target=$1
  local packages=
  local TMP_PACKAGELIST=
  Banner "generating ${sdk_target} package lists for mips32"
  rm -f ${TMP}/Packages.bz2
  DownloadOrCopy ${MIPS32_PACKAGES}
  bzcat ${TMP}/Packages.bz2\
    | egrep '^(Package:|Filename:)' > ${TMP}/Packages_mipsel

  if [ ${sdk_target} == "nacl_sdk" ] ; then
    echo -n > ${TMP_BASE_PKG_MIPS32}
    TMP_PACKAGELIST=${TMP_BASE_PKG_MIPS32}
    packages=$(cat ${BASE_PACKAGELIST_MIPS32})
  elif [ ${sdk_target} == "chrome_sdk" ] ; then
    echo -n > ${TMP_EXTRA_PKG_MIPS32}
    TMP_PACKAGELIST=${TMP_EXTRA_PKG_MIPS32}
    packages=$(cat ${EXTRA_PACKAGELIST_MIPS32})
  else
    Banner "ERROR: Packages for \"${sdk_taget}\" not defined."
    exit -1
  fi

  for pkg in ${packages} ; do
    grep  -A 1 "${pkg}\$" ${TMP}/Packages_mipsel\
      | egrep -o "pool/.*" >> ${TMP_PACKAGELIST}
  done
}

InstallMissingLibraries() {
  local sdk_target=$1
  local DEP_FILES_NEEDED_MIPS32=

  if [ ${sdk_target} == "nacl_sdk" ] ; then
    DEP_FILES_NEEDED_MIPS32=$(cat ${TMP_BASE_PKG_MIPS32})
  elif [ ${sdk_target} == "chrome_sdk" ] ; then
    DEP_FILES_NEEDED_MIPS32=$(cat ${TMP_EXTRA_PKG_MIPS32})
  else
    Banner "ERROR: Target \"${sdk_taget}\" not defined."
    exit -1
  fi

  for file in ${DEP_FILES_NEEDED_MIPS32} ; do
    local package="${TMP}/${file##*/}"
    Banner "installing ${file}"
    DownloadOrCopy ${REPO_DEBIAN}/${file}
    SubBanner "extracting to ${JAIL_MIPS32}"
    dpkg --fsys-tarfile ${package}\
      | tar -xvf - --exclude=./usr/share -C ${JAIL_MIPS32}
  done
}

# Workaround for missing headers since pkg-config is not working correctly.
FixIncludes() {
  Banner "Fixing includes"
  pushd ${JAIL_MIPS32}/usr/include/glib-2.0
    ln -s ../../lib/glib-2.0/include/glibconfig.h .
  popd

  pushd ${JAIL_MIPS32}/usr/include/gtk-2.0
    ln -s ../../lib/gtk-2.0/include/gdkconfig.h .
  popd

  pushd ${JAIL_MIPS32}/usr/include/dbus-1.0/dbus
    ln -s ../../../lib/dbus-1.0/include/dbus/dbus-arch-deps.h .
  popd
}

FixLinks() {
  Banner "Fixing links"
  pushd ${JAIL_MIPS32}/lib/
    mv mipsel-linux-gnu/* .
    rm -rf mipsel-linux-gnu
    ln -s . mipsel-linux-gnu
  popd

  pushd ${JAIL_MIPS32}/usr/lib/
    mkdir -p pkgconfig
    mv mipsel-linux-gnu/pkgconfig/* pkgconfig/
    rm -rf mipsel-linux-gnu/pkgconfig
    mv mipsel-linux-gnu/* .
    rm -rf mipsel-linux-gnu
    ln -s . mipsel-linux-gnu
  popd

  pushd ${JAIL_MIPS32}/usr/lib/
    rm -f libstdc++.so*
    ln -s ../../../mipsel-linux-gnu/lib/libstdc++.so.6.0.20 .
    ln -s libstdc++.so.6.0.20 libstdc++.so.6
    ln -s libstdc++.so.6.0.20 libstdc++.so

    rm -f libgcc_s.so*
    ln -s ../../../mipsel-linux-gnu/lib/libgcc_s.so.1 .
    ln -s libgcc_s.so.1 libgcc_s.so
  popd
}

FixLibs() {
  Banner "Fixing libraries"

  readonly liblist="libbz2.so       \
                    libcom_err.so   \
                    libdbus-1.so    \
                    libexpat.so     \
                    libglib-2.0.so  \
                    libgpg-error.so \
                    libkeyutils.so  \
                    libpamc.so      \
                    libpam_misc.so  \
                    libpam.so       \
                    libpci.so       \
                    libpcre.so      \
                    libpng12.so     \
                    libudev.so      \
                    libz.so"

  pushd ${JAIL_MIPS32}/usr/lib/
    for library in ${liblist}; do
      rm -f ${library}
      ln -s ../../lib/${library}.[0123] ${library}
    done
  popd
}

BuildAndInstallQemu() {
  local saved_dir=$(pwd)
  local tmpdir="${TMP}/qemu-mips.nacl"
  local tarball="qemu-2.0.0.tar.bz2"

  Banner "Building qemu in ${tmpdir}"

  rm -rf ${tmpdir}
  mkdir ${tmpdir}
  cd ${tmpdir}

  SubBanner "Downloading"
  wget -c ${DOWNLOAD_QEMU_URL}

  SubBanner "Untarring"
  tar xf ${tarball}
  cd qemu-2.0.0

  SubBanner "Configuring"
  env -i PATH=/usr/bin/:/bin \
    ./configure \
    --disable-system \
    --enable-linux-user \
    --disable-bsd-user \
    --target-list=mipsel-linux-user \
    --disable-sdl \
    --disable-linux-aio

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

elif [[ $1 == "nacl_sdk" || $1 == "chrome_sdk" ]] ; then
  mkdir -p ${TMP}
  SanityCheck
  ClearInstallDir
  ClearBuildDir
  DownloadOrCopyAndInstallToolchain
  GeneratePackageLists $1
  InstallMissingLibraries $1
  FixLinks
  if [[ $1 == "nacl_sdk" ]] ; then
    BuildAndInstallQemu
    CreateTarBall $1
  else
    FixLibs
    FixIncludes
    CreateTarBall ${CROSS_TARBALL}
  fi

else
  Usage
  exit -1

fi

