#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#@ This script builds the a cross toolchain for arm.
#@ It must be run from the native_client/ directory.
#@
#@ NOTE: There is one-time step required for all machines using this TC
#@
#@ This Toolchain was tested with Ubuntu Lucid
#@
#@ Usage of this TC:
#@  compile: arm-linux-gnueabi-gcc -march=armv7-a -isystem ${JAIL}/usr/include
#@  link:    arm-linux-gnueabi-gcc -L${JAIL}/usr/lib -L${JAIL}/usr/lib/arm-linux-gnueabi
#@                                 -L${JAIL}/lib -L${JAIL}/lib/arm-linux-gnueabi
#@
#@ Usage of this TC with qemu
#@  TODO(robertm)

######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

readonly SCRIPT_DIR=$(dirname $0)

# this where we create the ARMEL "jail"
readonly INSTALL_ROOT=$(pwd)/toolchain/linux_arm-trusted

readonly TMP=/tmp/arm-crosstool-natty

readonly REQUIRED_TOOLS="wget"

readonly MAKE_OPTS="-j8"

######################################################################
# Package Config
######################################################################

# this where we get the cross toolchain from for the manual install:
readonly CROSS_ARM_TC_REPO=http://mirror.pnl.gov/ubuntu
# this is where we get all the armel packages from
readonly ARMEL_REPO=http://ports.ubuntu.com/ubuntu-ports
#
readonly PACKAGE_LIST="${ARMEL_REPO}/dists/natty/main/binary-armel/Packages.bz2"

# Optional:
# gdb-arm-linux-gnueabi
# automake1.9
# libtool
readonly CROSS_ARM_TC_PACKAGES="\
  gcc-4.5-arm-linux-gnueabi-base \
  libc6-armel-cross \
  libc6-dev-armel-cross \
  libgcc1-armel-cross \
  libgomp1-armel-cross \
  linux-libc-dev-armel-cross \
  libgcc1-dbg-armel-cross \
  libgomp1-dbg-armel-cross \
  libstdc++6-4.4-dev-armel-cross \
  binutils-arm-linux-gnueabi \
  gcc-4.5-locales \
  cpp-4.5-arm-linux-gnueabi \
  cpp-arm-linux-gnueabi \
  gcc-4.5-arm-linux-gnueabi \
  libmudflap0-4.5-dev-armel-cross \
  libmudflap0-dbg-armel-cross
"

# NOTE: the package listing here should be updated using the
# GeneratePackageListXXX() functions below
readonly CROSS_ARM_TC_DEP_FILES_64="$(cat ${SCRIPT_DIR}/packagelist.amd64.crosstool)"

readonly CROSS_ARM_TC_DEP_FILES_32="\
${CROSS_ARM_TC_DEP_FILES_64//_amd64.deb/_i386.deb}"

readonly BUILD_ARCH=$(uname -m)
if [ "${BUILD_ARCH}" == "i386" ] ||
   [ "${BUILD_ARCH}" == "i686" ] ; then
  readonly CROSS_ARM_TC_DEP_FILES="${CROSS_ARM_TC_DEP_FILES_32}"
  readonly EXTRA_PACKAGES="make"
elif [ "${BUILD_ARCH}" == "x86_64" ] ; then
  readonly CROSS_ARM_TC_DEP_FILES="${CROSS_ARM_TC_DEP_FILES_64}"
  # 32bit compatibility TCs
  readonly EXTRA_PACKAGES="make ia32-libs libc6-i386"
else
  echo "Unknown build arch '${BUILD_ARCH}'"
  exit -1
fi

# These are good enough for native client
readonly ARMEL_BASE_PACKAGES="\
  libssl-dev \
  libssl0.9.8 \
  libgcc1 \
  libc6 \
  libc6-dev \
  libstdc++6 \
  libx11-dev \
  libx11-6 \
  x11proto-core-dev \
  libxt-dev \
  libxt6 \
  zlib1g \
  zlib1g-dev"

# These are needed for chrome
# NOTE: the package listing here should be updated using the
# GeneratePackageListXXX() functions below
readonly ARMEL_BASE_DEP_FILES="$(cat ${SCRIPT_DIR}/packagelist.natty.armel.base)"

readonly ARMEL_EXTRA_PACKAGES="\
  krb5-multidev \
  libasound2-dev
  libatk1.0-0 \
  libatk1.0-dev \
  libbz2-1.0 \
  libbz2-dev \
  libcairo2 \
  libcairo2-dev \
  libcups2 \
  libcups2-dev \
  libdbus-1-3 \
  libdbus-1-dev \
  libexpat1 \
  libexpat1-dev \
  libfontconfig1 \
  libfontconfig1-dev \
  libfreetype6 \
  libfreetype6-dev \
  libgconf2-4 \
  libgconf2-dev \
  libgdk-pixbuf2.0-0 \
  libgdk-pixbuf2.0-dev \
  libgtk2.0-0 \
  libgtk2.0-dev \
  libglib2.0-0 \
  libglib2.0-dev \
  libgnome-keyring-dev \
  libkrb5-dev \
  libnspr4 \
  libnspr4-dev \
  libnss3 \
  libnss3-dev \
  liborbit2 \
  libpam0g \
  libpam0g-dev \
  libpango1.0-0 \
  libpango1.0-dev \
  libpcre3 \
  libpcre3-dev \
  libpixman-1-0 \
  libpixman-1-dev \
  libpng12-0 \
  libpng12-dev \
  libselinux1 \
  libxext-dev \
  libxext6 \
  libxau-dev \
  libxau6 \
  libxcb1 \
  libxcb1-dev \
  libxcb-render0 \
  libxcb-render0-dev \
  libxcb-shm0 \
  libxcb-shm0-dev \
  libxcomposite1 \
  libxcomposite-dev \
  libxcursor1 \
  libxcursor-dev \
  libxdamage1 \
  libxdamage-dev \
  libxdmcp6 \
  libxfixes3 \
  libxfixes-dev \
  libxi6 \
  libxi-dev \
  libxinerama1 \
  libxinerama-dev \
  libxrandr2 \
  libxrandr-dev \
  libxrender1 \
  libxrender-dev \
  libxss-dev \
  libxtst6 \
  libxtst-dev \
  x11proto-composite-dev \
  x11proto-damage-dev \
  x11proto-fixes-dev \
  x11proto-input-dev \
  x11proto-record-dev \
  x11proto-render-dev \
  x11proto-scrnsaver-dev \
  x11proto-xext-dev"

# NOTE: the package listing here should be updated using the
# GeneratePackageListXXX() functions below
readonly ARMEL_EXTRA_DEP_FILES="$(cat ${SCRIPT_DIR}/packagelist.natty.armel.extra)"

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

  if ! mkdir -p "${TMP}" ; then
     echo "ERROR: ${TMP} can't be created."
    exit -1
  fi

  for tool in ${REQUIRED_TOOLS} ; do
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

######################################################################
# One of these has to be run ONCE per machine
######################################################################

#@
#@ InstallCrossArmBasePackages
#@
#@ This has been tested on 64bit ubuntu natty
#@ For oneiric additional adjustments are necessary
InstallCrossArmBasePackages() {
  sudo apt-get install ${CROSS_ARM_TC_PACKAGES}
}

#@
#@ InstallCrossArmBasePackagesManual
#@
#@ This should work even for 64bit ubuntu lucid machine
#@ The download part is more or less idem-potent, run it until
#@ all files have been downloaded
InstallCrossArmBasePackagesManual() {
  Banner "Install arm cross TC semi-automatically"

  local dest=${TMP}/manual-tc-packages
  mkdir -p ${dest}

  SubBanner "Download packages"
  for i in ${CROSS_ARM_TC_DEP_FILES} ; do
    echo $i
    url=${CROSS_ARM_TC_REPO}/pool/$i
    file=${dest}/$(basename $i)
    DownloadOrCopy ${url} ${file}
  done

  SubBanner "Package Sanity Check"

  for i in ${dest}/*.deb ; do
    ls -l $i
    if [[ ! -s $i ]] ; then
      echo
      echo "ERROR: bad package $i"
      exit -1
    fi
  done

  SubBanner "Possibly install additional standard packages"
  # these are needed for the TC packages we are about to install
  sudo apt-get install libelfg0 libgmpxx4ldbl libmpc2 libppl7 libppl-c2
  sudo apt-get install ${EXTRA_PACKAGES}

  SubBanner "Install cross arm TC packages"
  sudo dpkg -i ${dest}/*.deb
}

######################################################################
#
######################################################################

#@
#@ InstallTrustedLinkerScript
InstallTrustedLinkerScript() {
  local trusted_ld_script=${INSTALL_ROOT}/ld_script_arm_trusted
  # We are using the output of "ld --verbose" which contains
  # the linker script delimited by "=========".
  # We are changing the image start address to 70000000
  # to move the sel_ldr and other images "out of the way"
  Banner "installing trusted linker script to ${trusted_ld_script}"

  arm-linux-gnueabi-ld  --verbose |\
      grep -A 10000 "=======" |\
      grep -v "=======" |\
      sed -e 's/00008000/70000000/g' > ${trusted_ld_script}
}

HacksAndPatches() {
  rel_path=toolchain/linux_arm-trusted
  Banner "Misc Hacks & Patches"
  # these are linker scripts with absolute pathnames in them
  # which we rewrite here
  lscripts="${rel_path}/usr/lib/arm-linux-gnueabi/libpthread.so \
            ${rel_path}/usr/lib/arm-linux-gnueabi/libc.so"

  SubBanner "Rewriting Linker Scripts"
  sed -i -e 's|/usr/lib/arm-linux-gnueabi/||g'  ${lscripts}
  sed -i -e 's|/lib/arm-linux-gnueabi/||g' ${lscripts}

  # This is for chrome's ./build/linux/pkg-config-wrapper
  # which overwrites PKG_CONFIG_PATH internally
  SubBanner "Package Configs Symlink"
  mkdir -p  ${rel_path}/usr/share
  ln -s  ../lib/arm-linux-gnueabi/pkgconfig ${rel_path}/usr/share/pkgconfig
}


InstallMissingArmLibrariesAndHeadersIntoJail() {
  Banner "Install Libs And Headers Into Jail"

  mkdir -p ${TMP}/armel-packages
  mkdir -p ${INSTALL_ROOT}
  for file in $@ ; do
    local package="${TMP}/armel-packages/${file##*/}"
    Banner "installing ${file}"
    DownloadOrCopy ${ARMEL_REPO}/pool/${file} ${package}
    SubBanner "extracting to ${INSTALL_ROOT}"
    if [[ ! -s ${package} ]] ; then
      echo
      echo "ERROR: bad package ${package}"
      exit -1
    fi
    dpkg --fsys-tarfile ${package}\
      | tar -xvf - --exclude=./usr/share -C ${INSTALL_ROOT}
  done
}


CleanupJailSymlinks() {
  Banner "jail symlink cleanup"

  pushd ${INSTALL_ROOT}
  find usr/lib -type l -printf '%p %l\n' | while read link target; do
    # skip links with non-absolute paths
    if [[ ${target} != /* ]] ; then
      continue
    fi
    echo "${link}: ${target}"
    case "${link}" in
      usr/lib/arm-linux-gnueabi/*)
        # Relativize the symlink.
        ln -snfv "../../..${target}" "${link}"
        ;;
      usr/lib/*)
        # Relativize the symlink.
        ln -snfv "../..${target}" "${link}"
        ;;
    esac
    # make sure we catch new bad links
    if [ ! -r "${link}" ]; then
      echo "ERROR: FOUND BAD LINK ${link}"
      exit -1
    fi
  done
  popd
}

#@
#@ BuildAndInstallQemu
#
# Historic Notes:
# Traditionally we were builidng static 32 bit images of qemu on a
# 64bit system which would run then on both x86-32 and x86-64 systems.
# The latest version of qemu contains new dependencies which
# currently make it impossible to build such images on 64bit systems
# We can build a static 64bit qemu but it does not work with
# the sandboxed translators for unknown reason.
# So instead we chose to build 32bit shared images.
#
BuildAndInstallQemu() {
  local saved_dir=$(pwd)
  local tmpdir="${TMP}/qemu.nacl"
  # We have not ported out local patch yet to /qemu-1.0-rc1
  #local patch=$(readlink -f tools/patches/qemu-0.10.1.patch_arm)
  local tarball=$(readlink -f ../third_party/qemu/qemu-1.0-rc1.tar.gz)
  Banner "Building qemu in ${tmpdir}"
  rm -rf ${tmpdir}
  mkdir ${tmpdir}
  cd ${tmpdir}
  SubBanner "Untaring"
  tar zxf  ${tarball}
  cd qemu-1.0rc1
  # We have not ported out local patch yet to /qemu-1.0-rc1
  #SubBanner "Patching"
  #patch -p1 < ${patch}

  SubBanner "Configuring"
  env -i PATH=/usr/bin/:/bin \
    ./configure \
    --extra-cflags="-m32" \
    --extra-ldflags="-Wl,-rpath=/lib32" \
    --disable-system \
    --enable-linux-user \
    --disable-darwin-user \
    --disable-bsd-user \
    --target-list=arm-linux-user \
    --disable-smartcard-nss \
    --disable-sdl

# see above for why we can no longer use -static
#    --static

  SubBanner "Make"
  env -i PATH=/usr/bin/:/bin \
      V=99 make MAKE_OPTS=${MAKE_OPTS}

  SubBanner "Install"
  cp arm-linux-user/qemu-arm ${INSTALL_ROOT}
  cd ${saved_dir}
  cp tools/llvm/qemu_tool.sh ${INSTALL_ROOT}
  ln -sf qemu_tool.sh ${INSTALL_ROOT}/run_under_qemu_arm
}

#@
#@ BuildJail
BuildJail() {
  ClearInstallDir
  InstallMissingArmLibrariesAndHeadersIntoJail \
    ${ARMEL_BASE_DEP_FILES} \
    ${ARMEL_EXTRA_DEP_FILES}
  CleanupJailSymlinks
  InstallTrustedLinkerScript
  HacksAndPatches
  AddChromeWrapperScripts
  BuildAndInstallQemu
  CreateTarBall $1
}

#@
#@ AddChromeWrapperScripts
AddChromeWrapperScripts() {
   SubBanner "Installing Chrome Wrapper"

   cp -a tools/llvm/chrome.cc.arm.sh ${INSTALL_ROOT}/chrome.cc.arm.sh
   cp -a tools/llvm/chrome.cc.arm.sh ${INSTALL_ROOT}/chrome.c++.arm.sh

   cp -a tools/llvm/chrome.cc.host.sh ${INSTALL_ROOT}/chrome.cc.host.sh
   cp -a tools/llvm/chrome.cc.host.sh ${INSTALL_ROOT}/chrome.c++.host.sh

   chmod a+rx ${INSTALL_ROOT}/chrome.c*.sh
}

#@
#@ Regenerate Package List
#@
#@ This will need some manual intervention, e.g.
#@ pool/ needs to be stripped and special characters like may "+" cause problems
GeneratePackageList() {
  DownloadOrCopy ${PACKAGE_LIST} ${TMP}/Packages.bz2
  bzcat ${TMP}/Packages.bz2 | egrep '^(Package:|Filename:)' > ${TMP}/Packages
  echo  ${ARMEL_EXTRA_PACKAGES}
  echo "# BEGIN:"
  for pkg in $@ ; do
    grep  -A 1 "${pkg}\$" ${TMP}/Packages | egrep -o "pool/.*"
  done
  echo "# END:"
}

GeneratePackageListBase() {
  GeneratePackageList "${ARMEL_BASE_PACKAGES}"
}

GeneratePackageListExtra() {
  GeneratePackageList "${ARMEL_EXTRA_PACKAGES}"
}


SanityCheck

if [[ $# -eq 0 ]] ; then
  echo "you must specify a mode on the commandline:"
  echo
  Usage
  exit -1
elif [[ "$(type -t $1)" != "function" ]]; then
  echo "ERROR: unknown function '$1'." >&2
  echo "For help, try:"
  echo "    $0 help"
  exit 1
else
  "$@"
fi
