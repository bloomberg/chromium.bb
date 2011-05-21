#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
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

export INSTALL_ROOT=$(pwd)/toolchain/linux_arm-trusted

export TMP=/tmp/crosstool-trusted

export CS_ROOT=${INSTALL_ROOT}/arm-2009q3

readonly JAIL=${CS_ROOT}/arm-none-linux-gnueabi/libc

readonly  MAKE_OPTS="-j6"
# These are simple compiler wrappers to force 32bit builds
readonly  CC32=$(readlink -f tools/llvm/mygcc32)
readonly  CXX32=$(readlink -f tools/llvm/myg++32)
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

  for tool in wget ; do
    if ! which ${tool} ; then
      echo "Required binary $tool not found."
      echo "Exiting."
      exit 1
    fi
  done
}


# TODO(robertm): consider wiping all of ${BASE_DIR}
ClearInstallDir() {
  Banner "clearing dirs in ${INSTALL_ROOT}"
  rm -rf ${INSTALL_ROOT}/*
}


CreateTarBall() {
  local tarball=$1
  Banner "creating tar ball ${tarball}"
  tar cfz ${tarball} -C ${INSTALL_ROOT} .
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
#
#       The rest is for SDL support (sdl=armlocal)
#
# We are using ubuntu jaunty packages here, c.f.
#
# ----------------------------------------------------------------------
readonly PACKAGES_NEEDED="libssl-dev\
                          libssl0.9.8\
                          libsdl1.2-dev\
                          libsdl1.2debian-alsa\
                          libx11-dev\
                          libx11-6\
                          x11proto-core-dev\
                          libxt-dev\
                          libxt6\
                          libxext-dev\
                          libxext6\
                          libxau-dev\
                          libxau6\
                          libxss-dev\
                          libxss1\
                          libxdmcp-dev\
                          libxdmcp6\
                          libcaca0\
                          libaa1\
                          libdirectfb-1.2-0\
                          libasound2\
                          libxcb1"


readonly REPO=http://ports.ubuntu.com/ubuntu-ports


# NOTE: the DEP_FILES_NEEDED_* string is obtained by running
#       "trusted-toolchain-creator.sh package_list"
#       Since the output will differ over time we freeze one here:
readonly DEP_FILES_NEEDED_KARMIC="\
    pool/main/o/openssl/libssl-dev_0.9.8g-16ubuntu3_armel.deb\
    pool/main/o/openssl/libssl0.9.8_0.9.8g-16ubuntu3_armel.deb\
    pool/main/libs/libsdl1.2/libsdl1.2-dev_1.2.13-4ubuntu4_armel.deb\
    pool/main/libs/libsdl1.2/libsdl1.2debian-alsa_1.2.13-4ubuntu4_armel.deb\
    pool/main/libx/libx11/libx11-dev_1.2.2-1ubuntu1_armel.deb\
    pool/main/libx/libx11/libx11-6_1.2.2-1ubuntu1_armel.deb\
    pool/main/x/x11proto-core/x11proto-core-dev_7.0.15-1_all.deb\
    pool/main/libx/libxt/libxt-dev_1.0.5-3ubuntu1_armel.deb\
    pool/main/libx/libxt/libxt6_1.0.5-3ubuntu1_armel.deb\
    pool/main/libx/libxext/libxext-dev_1.0.99.1-0ubuntu4_armel.deb\
    pool/main/libx/libxext/libxext6_1.0.99.1-0ubuntu4_armel.deb\
    pool/main/libx/libxau/libxau-dev_1.0.4-2_armel.deb\
    pool/main/libx/libxau/libxau6_1.0.4-2_armel.deb\
    pool/main/libx/libxss/libxss-dev_1.1.3-1_armel.deb\
    pool/main/libx/libxss/libxss1_1.1.3-1_armel.deb\
    pool/main/libx/libxdmcp/libxdmcp-dev_1.0.2-3_armel.deb\
    pool/main/libx/libxdmcp/libxdmcp6_1.0.2-3_armel.deb\
    pool/main/libc/libcaca/libcaca0_0.99.beta16-1_armel.deb\
    pool/main/a/aalib/libaa1_1.4p5-38_armel.deb\
    pool/main/d/directfb/libdirectfb-1.2-0_1.2.7-2ubuntu1_armel.deb\
    pool/main/a/alsa-lib/libasound2_1.0.20-3ubuntu6_armel.deb\
    pool/main/libx/libxcb/libxcb1_1.4-1_armel.deb"


InstallMissingLibraries() {
  for file in ${DEP_FILES_NEEDED_KARMIC} ; do
    local package="${TMP}/${file##*/}"
    Banner "installing ${file}"
    DownloadOrCopy ${REPO}/${file} ${package}
    SubBanner "extracting to ${JAIL}"
    dpkg --fsys-tarfile ${package}\
      | tar -xvf - --exclude=./usr/share -C ${JAIL}
  done

  Banner "symlink cleanup"
  (
    cd ${JAIL}
    find usr/lib -type l -printf '%p %l\n' | while read link target; do
      case "$target" in
        /*)
          # Relativize the symlink.
          ln -snfv "../..$target" "$link"
          ;;
      esac
      # Just in case there are dangling links other than ones
      # that just needed to be relativized, remove them.
      if [ ! -r "$link" ]; then
        rm -fv "$link"
      fi
    done
  )
}

# libssl.so and libcrypto.so cannot be used because they have
# a dependency on libz.so. We don't download libz, although
# we probably should. For now, remove the .so and let the linker
# use the .a instead.
HideSOHack() {
  Banner "Hide SO Hack"
  rm -f "${CS_ROOT}"/arm-none-linux-gnueabi/libc/usr/lib/libcrypto.so
  rm -f "${CS_ROOT}"/arm-none-linux-gnueabi/libc/usr/lib/libssl.so
  rm -f "${CS_ROOT}"/arm-none-linux-gnueabi/libc/usr/lib/libssl.so.0.9.8
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
    --target-list=arm-linux-user \
    --disable-sdl\
    --static

  SubBanner "Make"
  env -i PATH=/usr/bin/:/bin \
      make MAKE_OPTS=${MAKE_OPTS}

  SubBanner "Install"
  cp arm-linux-user/qemu-arm ${INSTALL_ROOT}
  cd ${saved_dir}
  cp tools/llvm/qemu_tool.sh ${INSTALL_ROOT}
  ln -sf qemu_tool.sh ${INSTALL_ROOT}/run_under_qemu_arm
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
  SanityCheck
  ClearInstallDir
  DownloadOrCopyAndInstallCodeSourceryTarball
  InstallTrustedLinkerScript
  PruneDirs
  InstallMissingHeaders
  InstallMissingLibraries
  HideSOHack
  BuildAndInstallQemu
  CreateTarBall $1
  exit 0
fi

#@
#@ qemu_only
#@
#@   update only qemu
if [ ${MODE} = 'qemu_only' ] ; then
  mkdir -p ${TMP}
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

#@
#@ sanity
#@
#@   run sanity checks
if [ ${MODE} = 'sanity' ] ; then
  SanityCheck
  exit 0
fi

#@
#@ package_list
#@
#@   regenerate package list
if [ ${MODE} = 'package_list' ] ; then
  readonly PACKAGE_LIST_JAUNTY=http://ports.ubuntu.com/ubuntu-ports/dists/jaunty/main/binary-armel/Packages.bz2
  readonly PACKAGE_LIST_KARMIC=http://ports.ubuntu.com/ubuntu-ports/dists/karmic/main/binary-armel/Packages.bz2
  mkdir -p  ${TMP}
  DownloadOrCopy ${PACKAGE_LIST_KARMIC} ${TMP}/Packages.bz2
  bzcat ${TMP}/Packages.bz2 | egrep '^(Package:|Filename:)' > ${TMP}/Packages
  echo "# BEGIN: update for DEP_FILES_NEEDED"
  for pkg in ${PACKAGES_NEEDED} ; do
    egrep -A 1 "${pkg}\$" ${TMP}/Packages | egrep -o "pool/.*"
  done
  echo "# END: update for DEP_FILES_NEEDED"
  exit 0
fi


echo "ERROR: unknown mode ${MODE}"
exit -1
