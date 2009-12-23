#!/bin/bash
# Copyright 2009 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
#@ This script build the arm trusted SDK.
#@ It must be run from the native_client/ directory.

######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

# Recent CodeSourcery Toolchains can be found here
# http://www.codesourcery.com/sgpp/lite/arm/portal/subscription?@template=lite

readonly CS_URL=http://www.codesourcery.com/sgpp/lite/arm/portal/package5383/public/arm-none-linux-gnueabi/arm-2009q3-67-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2

# TODO(robertm): temporarily use /usr/local/crosstool2 to not conflict
#                with the working toolchain in /usr/local/crosstool
export INSTALL_ROOT=/usr/local/crosstool-trusted

export TMP=/tmp/crosstool-trusted

export CS_ROOT=${INSTALL_ROOT}/arm-2009q3

readonly JAIL=${CS_ROOT}/arm-none-linux-gnueabi/libc
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
  egrep "^#@" $0 | cut --bytes=3-
}


DownloadOrCopy() {
  if [[ -f "$2" ]] ; then
     echo "$2 already in place"
  elif [[ $1 =~  'http://' ]] ; then
    SubBanner "downloading from $1 -> $2"
    wget $1 -O $2
  else
    SubBanner "copying from $1"
    cp $1 $2
  fi
}
######################################################################
#
######################################################################

# some sanity checks to make sure this script is run from the right place
PathSanityCheck() {
  if [[ $(basename $(pwd)) != "native_client" ]] ; then
    echo "ERROR: run this script from the native_client/ dir"
    exit -1
  fi

  if [[ ! -d ${INSTALL_ROOT} ]] ; then
     echo "ERROR: ${INSTALL_ROOT} does not exist"
    exit -1
  fi
}


# TODO(robertm): consider wiping all of ${BASE_DIR}
ClearInstallDir() {
  Banner "clearing dirs in ${INSTALL_ROOT}"
  rm -rf ${INSTALL_ROOT}/*
}


CreateTarBall() {
  local tarball=$1
  Banner "creating tar ball ${tarball}"
  tar cfj ${tarball} -C ${INSTALL_ROOT} .
}


# try to keep the tarball small
PruneDirs() {
  Banner "pruning code sourcery tree"
  SubBanner "Size before: $(du -msc  ${CS_ROOT})"
  rm -rf ${CS_ROOT}/share
  rm -rf ${CS_ROOT}/arm-none-linux-gnueabi/libc/armv4t
  rm -rf ${CS_ROOT}/arm-none-linux-gnueabi/libc/thumb2
  SubBanner "Size after: $(du -msc  ${CS_ROOT})"
}


# Download the codesourcery tarball or use a local copy when available.
DownloadOrCopyAndInstallCodeSourceryTarball() {
  Banner "Installing Codesourcery Toolchain"
  local tarball="${TMP}/${CS_URL##*/}"
  DownloadOrCopy ${CS_URL} ${tarball}

  SubBanner "Untaring  ${INSTALL_ROOT}/${tarball}"
  tar jxf ${tarball} -C ${INSTALL_ROOT}
}


InstallTrustedLinkerScript() {
  local trusted_ld_script=${INSTALL_ROOT}/ld_script_arm_trusted
  # We are using the output of "ld --verbose" which contains
  # the linker script delimited by "=========".
  # We are changing the image start address to 70000000
  # to move the sel_ldr and other images "out of the way"
  Banner "installing trusted linker script to ${trusted_ld_script}"

  ${CS_ROOT}/bin/arm-none-linux-gnueabi-ld  --verbose |\
      grep -A 10000 "=======" |\
      grep -v "=======" |\
      sed -e 's/00008000/70000000/g' > ${trusted_ld_script}
}


InstallMissingHeaders() {
  Banner "installing openssl headers from local system"
  cp -r /usr/include/openssl ${JAIL}/usr/include/
}

# ----------------------------------------------------------------------
# armel deb files to complete our code sourcery jail
# TODO: switch to the equivalent of these from a ubuntu karmic repo
#
# NOTE: We always need the libssl static lib and X11 headers
#       The rest is for SDL support (sdl=armlocal)
# ----------------------------------------------------------------------
readonly REPO=http://repository.maemo.org/pool/maemo4.0.1/free

readonly LIBSSL_DEV=${REPO}/o/openssl/libssl-dev_0.9.7e-4.osso2+3sarge3.osso6_armel.deb
readonly LIBSDL_DEV=${REPO}/libs/libsdl1.2/libsdl1.2-dev_1.2.8-23_armel.deb
readonly LIBX11_DEV=${REPO}/libx/libx11/libx11-dev_1.1.1-1osso3_armel.deb
readonly LIBXT_DEV=${REPO}/libx/libxt/libxt-dev_1.0.5-2osso1_armel.deb
readonly LIBXEXT_DEV=${REPO}/libx/libxext/libxext-dev_1.0.3-2osso1_armel.deb
readonly LIBXAU_DEV=${REPO}/libx/libxau/libxau-dev_1.0.3-2osso1_armel.deb
readonly LIBXDMCP_DEV=${REPO}/libx/libxdmcp/libxdmcp-dev_1.0.2-2osso1_armel.deb


InstallMissingLibraries() {
  Banner "installing xdmcp"
  local package="${TMP}/${LIBXDMCP_DEV##*/}"
  DownloadOrCopy ${LIBXDMCP_DEV} ${package}
  SubBanner "extracting to ${JAIL}"
  dpkg --fsys-tarfile ${package}\
      | tar -xvf - --exclude=./usr/share -C ${JAIL}
  rm -f ${JAIL}/usr/lib/libXdmcp.so

  Banner "installing xau"
  local package="${TMP}/${LIBXAU_DEV##*/}"
  DownloadOrCopy ${LIBXAU_DEV} ${package}
  SubBanner "extracting to ${JAIL}"
  dpkg --fsys-tarfile ${package}\
      | tar -xvf - --exclude=./usr/share -C ${JAIL}
  rm -f ${JAIL}/usr/lib/libXau.so


  Banner "installing xext"
  local package="${TMP}/${LIBXEXT_DEV##*/}"
  DownloadOrCopy ${LIBXEXT_DEV} ${package}
  SubBanner "extracting to ${JAIL}"
  dpkg --fsys-tarfile ${package}\
      | tar -xvf - --exclude=./usr/share -C ${JAIL}
  rm -f ${JAIL}/usr/lib/libXext.so

  Banner "installing x11"
  local package="${TMP}/${LIBX11_DEV##*/}"
  DownloadOrCopy ${LIBX11_DEV} ${package}
  SubBanner "extracting to ${JAIL}"
  dpkg --fsys-tarfile ${package}\
      | tar -xvf - --exclude=./usr/share -C ${JAIL}
  rm -f ${JAIL}/usr/lib/libX11.so

  Banner "installing xt"
  local package="${TMP}/${LIBXT_DEV##*/}"
  DownloadOrCopy ${LIBXT_DEV} ${package}
  SubBanner "extracting to ${JAIL}"
  dpkg --fsys-tarfile ${package}\
      | tar -xvf - --exclude=./usr/share -C ${JAIL}
  rm -f ${JAIL}/usr/lib/libXt.so

  Banner "installing libsdl"
  local package="${TMP}/${LIBSDL_DEV##*/}"
  DownloadOrCopy ${LIBSDL_DEV} ${package}
  SubBanner "extracting to ${JAIL}"
  dpkg --fsys-tarfile ${package}\
      | tar -xvf - --exclude=./usr/share -C ${JAIL}
  rm -f ${JAIL}/usr/lib/libSDL.so

  Banner "installing libcrypto"
  local package="${TMP}/${LIBSSL_DEV##*/}"
  DownloadOrCopy ${LIBSSL_DEV} ${package}
  SubBanner "extracting"
  dpkg --fsys-tarfile ${package}\
      | tar -xf - -C ${TMP} ./usr/lib/libcrypto.a
  cp ${TMP}/usr/lib/libcrypto.a ${JAIL}/usr/lib/
  rm -f ./usr/lib/libcrypto*so
  rm -f ./lib/libcrypto*
}


BuildAndInstallQemu() {
  local saved_dir=$(pwd)
  local tmpdir="${TMP}/qemu.nacl"
  local patch=$(readlink -f tools/patches/qemu-0.10.1.patch_arm)
  local tarball=$(readlink -f ../third_party/qemu/qemu-0.10.1.tar.gz)
  Banner "Building qemu in ${tmpdir}"
  rm -rf ${tmpdir}
  mkdir ${tmpdir}
  cd ${tmpdir}
  SubBanner "Untaring"
  tar zxf  ${tarball}
  cd qemu-0.10.1
  SubBanner "Patching"
  patch -p1 < ${patch}
  SubBanner "Configuring"
  ./configure\
     --disable-system \
     --enable-linux-user \
     --disable-darwin-user \
     --disable-bsd-user \
     --target-list=arm-linux-user \
     --disable-sdl\
     --static
  SubBanner "Make"
  make -j6
  SubBanner "Install"
  cp arm-linux-user/qemu-arm ${INSTALL_ROOT}
  cd ${saved_dir}
  cp tools/llvm/qemu_tool.sh ${INSTALL_ROOT}
}
######################################################################
# Main
######################################################################
if [ $# -eq 0 ] ; then
  echo
  echo "you must specify a mode on the commandline:"
  echo
  Usage
  exit -1
fi

MODE=$1
shift

#@
#@ help
#@
#@   print help for all modes
if [ $MODE} = 'help' ] ; then
  Usage
  exit 0
fi

#@
#@ trusted_sdk <tarball>
#@
#@   create trusted sdk tarball
if [ ${MODE} = 'trusted_sdk' ] ; then
  mkdir -p ${TMP}
  PathSanityCheck
  ClearInstallDir
  DownloadOrCopyAndInstallCodeSourceryTarball
  InstallTrustedLinkerScript
  PruneDirs
  InstallMissingHeaders
  InstallMissingLibraries
  BuildAndInstallQemu
  CreateTarBall $1
  exit 0
fi

#@
#@ qemu_only
#@
#@   update only qemu
if [ ${MODE} = 'qemu_only' ] ; then
  BuildAndInstallQemu
  exit 0
fi

#@
#@ missing
#@
#@   update some missing libs and headers
if [ ${MODE} = 'missing' ] ; then
  mkdir -p /tmp/crosstool-trusted/
  InstallMissingLibraries
  InstallMissingHeaders
  exit 0
fi

#@
#@ tar <tarball>
#@
#@   tar everything up
if [ ${MODE} = 'tar' ] ; then
  CreateTarBall $1
  exit 0
fi

echo "ERROR: unknown mode ${MODE}"
exit -1
