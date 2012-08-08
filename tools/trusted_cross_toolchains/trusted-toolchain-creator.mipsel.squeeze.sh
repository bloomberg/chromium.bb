#!/bin/bash
# Copyright 2012 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
#
#@ This script creates the mips trusted SDK.
#@ It must be run from the native_client directory.

######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

readonly CS_URL=https://sourcery.mentor.com/sgpp/lite/mips/portal/package9761/public/mips-linux-gnu/mips-2011.09-75-mips-linux-gnu-i686-pc-linux-gnu.tar.bz2

readonly DOWNLOAD_QEMU_URL="http://download.savannah.gnu.org/releases/qemu/qemu-0.12.5.tar.gz"

readonly INSTALL_ROOT=$(pwd)/toolchain/linux_mips-trusted

readonly TMP=$(pwd)/toolchain/tmp/crosstool-trusted

readonly PATCH_MIPS32=$(readlink -f ../third_party/qemu/qemu-0.12.5.patch_mips)

readonly CS_ROOT=${INSTALL_ROOT}/mips-release

readonly JAIL_MIPS32=${CS_ROOT}/mips-linux-gnu/libc/el

readonly  MAKE_OPTS="-j8"
# These are simple compiler wrappers to force 32bit builds
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
  echo "...................................................................."
}

Usage() {
  echo
  echo "$0 trusted_sdk"
  echo
  echo "trusted_sdk - Build everything and package it"
  echo
}

DownloadOrCopy() {
  if [[ -f "$2" ]] ; then
     echo "$2 already in place"
  elif [[ $1 =~  'http://' || $1 =~  'https://' ]] ; then
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
# with the right tools
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


CreateTarBall() {
  local tarball=$1
  Banner "creating tar ball ${tarball}"
  tar cfz ${tarball}-mips.tgz -C ${INSTALL_ROOT} .
}


# try to keep the tarball small
PruneDirs() {
  Banner "pruning code sourcery tree"
  SubBanner "Size before: $(du -msc  ${CS_ROOT})"
  rm -rf ${CS_ROOT}/share
  rm -rf ${CS_ROOT}/mips-linux-gnu/lib/uclibc
  rm -rf ${CS_ROOT}/mips-linux-gnu/lib/soft-float
  rm -rf ${CS_ROOT}/mips-linux-gnu/lib/micromips

  rm -rf ${CS_ROOT}/mips-linux-gnu/libc/uclibc
  rm -rf ${CS_ROOT}/mips-linux-gnu/libc/soft-float
  rm -rf ${CS_ROOT}/mips-linux-gnu/libc/micromips

  rm -rf ${CS_ROOT}/lib/gcc/mips-linux-gnu/4.4.1/uclibc
  rm -rf ${CS_ROOT}/lib/gcc/mips-linux-gnu/4.4.1/soft-float
  rm -rf ${CS_ROOT}/lib/gcc/mips-linux-gnu/4.4.1/micromips

  rm -rf ${CS_ROOT}/mips-linux-gnu/include/c++/4.4.1/mips-linux-gnu/uclibc
  rm -rf ${CS_ROOT}/mips-linux-gnu/include/c++/4.4.1/mips-linux-gnu/soft-float
  rm -rf ${CS_ROOT}/mips-linux-gnu/include/c++/4.4.1/mips-linux-gnu/micromips

  SubBanner "Size after: $(du -msc  ${CS_ROOT})"
}


# Download the codesourcery tarball or use a local copy when available.
DownloadOrCopyAndInstallCodeSourceryTarball() {
  Banner "Installing Codesourcery Toolchain"
  local tarball="${TMP}/${CS_URL##*/}"
  DownloadOrCopy ${CS_URL} ${tarball}

  SubBanner "Untaring  ${INSTALL_ROOT}/${tarball}"
  tar jxf ${tarball} -C ${INSTALL_ROOT}

  pushd ${INSTALL_ROOT}
  mv mips-* mips-release
  popd
}


InstallTrustedLinkerScript() {
  local trusted_ld_script=${INSTALL_ROOT}/ld_script_mips_trusted
  # We are using the output of "ld --verbose" which contains
  # the linker script delimited by "=========".
  # We are changing the image start address to 70000000
  # to move the sel_ldr and other images "out of the way"
  Banner "installing trusted linker script to ${trusted_ld_script}"

  ${CS_ROOT}/bin/mips-linux-gnu-ld  --verbose |\
      grep -A 10000 "=======" |\
      grep -v "=======" |\
      sed -e 's/0400000/70000000/g' > ${trusted_ld_script}
}


InstallMissingHeaders() {
  Banner "installing openssl headers from local system"
  cp -r /usr/include/openssl ${JAIL_MIPS32}/usr/include/
}


MissingSharedLibCleanup() {
  Banner "Cleanup dangling symlinks"
}

# ----------------------------------------------------------------------
# mips32 deb files to complete our code sourcery jail
# ----------------------------------------------------------------------

readonly REPO_DEBIAN=http://ftp.debian.org/debian
readonly MIPS32_PACKAGES=${REPO_DEBIAN}/dists/squeeze/main/binary-mipsel/Packages.bz2

readonly TMP_PACKAGELIST_MIPS32=${TMP}/../packagelist_mipsel.tmp

# These are good enough for native client
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
  DownloadOrCopy ${MIPS32_PACKAGES} ${TMP}/../Packages_mipsel.bz2
  bzcat ${TMP}/../Packages_mipsel.bz2\
    | egrep '^(Package:|Filename:)' > ${TMP}/../Packages_mipsel
  for pkg in ${BASE_PACKAGES} ; do
    grep  -A 1 "${pkg}\$" ${TMP}/../Packages_mipsel\
      | egrep -o "pool/.*" >> ${TMP_PACKAGELIST_MIPS32}
  done
}

InstallMissingLibraries() {
  readonly DEP_FILES_NEEDED_MIPS32=$(cat ${TMP_PACKAGELIST_MIPS32})
  for file in ${DEP_FILES_NEEDED_MIPS32} ; do
    local package="${TMP}/${file##*/}"
    Banner "installing ${file}"
    DownloadOrCopy ${REPO_DEBIAN}/${file} ${package}
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

  echo "OUTPUT_FORMAT(elf32-tradlittlemips)" > libc.so
  echo "GROUP ( libc.so.6 libc_nonshared.a  AS_NEEDED ( ld.so.1 ) )" >> libc.so

  echo "OUTPUT_FORMAT(elf32-tradlittlemips)" > libpthread.so
  echo "GROUP ( libpthread.so.0 libpthread_nonshared.a )" >> libpthread.so
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
  SubBanner "Untaring"
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
  DownloadOrCopyAndInstallCodeSourceryTarball
  PruneDirs
  GeneratePackageLists
  InstallMissingHeaders
  InstallMissingLibraries
  MissingSharedLibCleanup
  InstallTrustedLinkerScript
  BuildAndInstallQemu
  CreateTarBall $1

else
  Usage
  exit -1

fi

