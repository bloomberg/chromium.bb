#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#@ This script builds the (trusted) cross toolchain for arm.
#@ It must be run from the native_client/ directory.
#@
#@ The toolchain consists primarily of a jail with arm header and libraries.
#@ It also provides additional tools such as QEMU.
#@ It does NOT provide the actual cross compiler anymore.
#@ The cross compiler is now coming straight from a Debian package.
#@ So there is a one-time step required for all machines using this TC:
#@
#@  tools/trusted_cross_toolchains/trusted-toolchain-creator.armhf.trusty.sh InstallCrossArmBasePackages
#@
#@ Generally this script is invoked as:
#@
#@  tools/trusted_cross_toolchains/trusted-toolchain-creator.armhf.trusty.sh <mode> <args>*
#@
#@ List of modes:

######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

readonly DIST=trusty
readonly SCRIPT_DIR=$(dirname $0)
readonly NACL_ROOT=$(pwd)
# this where we create the ARMHF sysroot
readonly INSTALL_ROOT=${NACL_ROOT}/toolchain/linux_x86/arm_trusted
readonly TAR_ARCHIVE=$(dirname ${NACL_ROOT})/out/sysroot_arm_trusted_${DIST}.tgz
readonly TMP=$(dirname ${NACL_ROOT})/out/sysroot_arm_trusted_${DIST}
readonly REQUIRED_TOOLS="wget"
readonly MAKE_OPTS="-j8"

######################################################################
# Package Config
######################################################################

# this where we get the cross toolchain from for the manual install:
readonly CROSS_ARM_TC_REPO=http://archive.ubuntu.com/ubuntu
# this is where we get all the armhf packages from
readonly ARMHF_REPO=http://ports.ubuntu.com/ubuntu-ports

readonly PACKAGE_LIST="${ARMHF_REPO}/dists/${DIST}/main/binary-armhf/Packages.bz2"
readonly PACKAGE_LIST2="${ARMHF_REPO}/dists/${DIST}-security/main/binary-armhf/Packages.bz2"

# Packages for the host system
readonly CROSS_ARM_TC_PACKAGES="\
  g++-arm-linux-gnueabihf"

# Jail packages: these are good enough for native client
# NOTE: the package listing here should be updated using the
# GeneratePackageListXXX() functions below
readonly ARMHF_BASE_PACKAGES="\
  libssl-dev \
  libssl1.0.0 \
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

# Additional jail packages needed to build chrome
# NOTE: the package listing here should be updated using the
# GeneratePackageListXXX() functions below
readonly ARMHF_BASE_DEP_LIST="${SCRIPT_DIR}/packagelist.${DIST}.armhf.base"
readonly ARMHF_BASE_DEP_FILES="$(cat ${ARMHF_BASE_DEP_LIST})"

readonly ARMHF_EXTRA_PACKAGES="\
  comerr-dev \
  krb5-multidev \
  libasound2 \
  libasound2-dev \
  libatk1.0-0 \
  libatk1.0-dev \
  libcairo2 \
  libcairo2-dev \
  libcairo-gobject2 \
  libcairo-script-interpreter2 \
  libcomerr2 \
  libcups2 \
  libcups2-dev \
  libdbus-1-3 \
  libdbus-1-dev \
  libexpat1 \
  libexpat1-dev \
  libffi6 \
  libfontconfig1 \
  libfontconfig1-dev \
  libfreetype6 \
  libfreetype6-dev \
  libgconf-2-4 \
  libgconf2-4 \
  libgconf2-dev \
  libgpg-error0 \
  libgpg-error-dev \
  libgcrypt11 \
  libgcrypt11-dev \
  libgdk-pixbuf2.0-0 \
  libgdk-pixbuf2.0-dev \
  libgnutls26 \
  libgnutlsxx27 \
  libgnutls-dev \
  libgnutls-openssl27 \
  libgssapi-krb5-2 \
  libgssrpc4 \
  libgtk2.0-0 \
  libgtk2.0-dev \
  libglib2.0-0 \
  libglib2.0-dev \
  libgnome-keyring0 \
  libgnome-keyring-dev \
  libkadm5clnt-mit9 \
  libkadm5srv-mit9 \
  libkdb5-7 \
  libkrb5-3 \
  libkrb5-dev \
  libkrb5support0 \
  libk5crypto3 \
  libnspr4 \
  libnspr4-dev \
  libnss3 \
  libnss3-dev \
  libnss-db \
  liborbit2 \
  libcap-dev \
  libcap2 \
  libpam0g \
  libpam0g-dev \
  libpango-1.0-0 \
  libpango1.0-dev \
  libpangocairo-1.0-0 \
  libpangoft2-1.0-0 \
  libpangoxft-1.0-0 \
  libpci3 \
  libpci-dev \
  libpcre3 \
  libpcre3-dev \
  libpcrecpp0 \
  libpixman-1-0 \
  libpixman-1-dev \
  libpng12-0 \
  libpng12-dev \
  libpulse0 \
  libpulse-dev \
  libpulse-mainloop-glib0 \
  libselinux1 \
  libspeechd2 \
  libspeechd-dev \
  libstdc++-4.8-dev \
  libudev1 \
  libudev-dev \
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
  libxss1 \
  libxss-dev \
  libxtst6 \
  libxtst-dev \
  speech-dispatcher \
  x11proto-composite-dev \
  x11proto-damage-dev \
  x11proto-fixes-dev \
  x11proto-input-dev \
  x11proto-kb-dev \
  x11proto-randr-dev \
  x11proto-record-dev \
  x11proto-render-dev \
  x11proto-scrnsaver-dev \
  x11proto-xext-dev"

# NOTE: the package listing here should be updated using the
# GeneratePackageListXXX() functions below
readonly ARMHF_EXTRA_DEP_LIST="${SCRIPT_DIR}/packagelist.${DIST}.armhf.extra"
readonly ARMHF_EXTRA_DEP_FILES="$(cat ${ARMHF_EXTRA_DEP_LIST})"

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


ChangeDirectory() {
  # Change direcotry to top 'native_client' directory.
  cd $(dirname ${BASH_SOURCE})
  cd ../..
}


# TODO(robertm): consider wiping all of ${BASE_DIR}
ClearInstallDir() {
  Banner "clearing dirs in ${INSTALL_ROOT}"
  rm -rf ${INSTALL_ROOT}/*
}


CreateTarBall() {
  Banner "creating tar ball ${TAR_ARCHIVE}"
  tar cfz ${TAR_ARCHIVE} -C ${INSTALL_ROOT} .
}

######################################################################
# One of these has to be run ONCE per machine
######################################################################

#@
#@ InstallCrossArmBasePackages
#@
#@      Install packages needed for arm cross compilation.
InstallCrossArmBasePackages() {
  sudo apt-get install ${CROSS_ARM_TC_PACKAGES}
}

######################################################################
#
######################################################################

HacksAndPatches() {
  rel_path=toolchain/linux_x86/arm_trusted
  Banner "Misc Hacks & Patches"
  # these are linker scripts with absolute pathnames in them
  # which we rewrite here
  lscripts="${rel_path}/usr/lib/arm-linux-gnueabihf/libpthread.so \
            ${rel_path}/usr/lib/arm-linux-gnueabihf/libc.so"

  SubBanner "Rewriting Linker Scripts"
  sed -i -e 's|/usr/lib/arm-linux-gnueabihf/||g' ${lscripts}
  sed -i -e 's|/lib/arm-linux-gnueabihf/||g' ${lscripts}
}


InstallMissingArmLibrariesAndHeadersIntoJail() {
  Banner "Install Libs And Headers Into Jail"

  mkdir -p ${TMP}/armhf-packages
  mkdir -p ${INSTALL_ROOT}
  for file in $@ ; do
    local package="${TMP}/armhf-packages/${file##*/}"
    Banner "installing ${file}"
    DownloadOrCopy ${ARMHF_REPO}/pool/${file} ${package}
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
      usr/lib/arm-linux-gnueabihf/*)
        # Relativize the symlink.
        ln -snfv "../../..${target}" "${link}"
        ;;
      usr/lib/*)
        # Relativize the symlink.
        ln -snfv "../..${target}" "${link}"
        ;;
    esac
  done

  find usr/lib -type l -printf '%p %l\n' | while read link target; do
    # Make sure we catch new bad links.
    if [ ! -r "${link}" ]; then
      echo "ERROR: FOUND BAD LINK ${link}"
      exit -1
    fi
  done
  popd
}

#@
#@ BuildAndInstallQemu
#@
#@     Build ARM emulator including some patches for better tracing
#
# Historic Notes:
# Traditionally we were building static 32 bit images of qemu on a
# 64bit system which would run then on both x86-32 and x86-64 systems.
# The latest version of qemu contains new dependencies which
# currently make it impossible to build such images on 64bit systems
# We can build a static 64bit qemu but it does not work with
# the sandboxed translators for unknown reason.
# So instead we chose to build 32bit shared images.
#

readonly QEMU_TARBALL=qemu-1.0.1.tar.gz
readonly QEMU_SHA=4d08b5a83538fcd7b222bec6f1c584da8d12497a
readonly QEMU_DIR=qemu-1.0.1

# TODO(sbc): update to version 2.3.0
#readonly QEMU_TARBALL=qemu-2.3.0.tar.bz2
#readonly QEMU_SHA=373d74bfafce1ca45f85195190d0a5e22b29299e
#readonly QEMU_DIR=qemu-2.3.0

readonly QEMU_URL=http://wiki.qemu-project.org/download/${QEMU_TARBALL}
readonly QEMU_PATCH=$(readlink -f ../third_party/qemu/${QEMU_DIR}.patch_arm)


BuildAndInstallQemu() {
  local saved_dir=$(pwd)
  local tmpdir="${TMP}/qemu.nacl"

  Banner "Building qemu in ${tmpdir}"

  if [ -z "${DEBIAN_I386_SYSROOT:-}" ]; then
    echo "Please set \$DEBIAN_I386_SYSROOT to the location of a debian/stable"
    echo "32-bit sysroot"
    echo "e.g. <chrome>/chrome/installer/linux/debian_wheezy_i386-sysroot"
    echo "Which itself is setup by chrome's install-debian.wheezy.sysroot.py"
    exit 1
  fi

  if [ ! -d "${DEBIAN_I386_SYSROOT:-}" ]; then
    echo "\$DEBIAN_I386_SYSROOT does not exist: $DEBIAN_I386_SYSROOT"
    exit 1
  fi

  if [ -n "${QEMU_URL}" ]; then
    if [[ ! -f ${TMP}/${QEMU_TARBALL} ]]; then
      wget -O ${TMP}/${QEMU_TARBALL} $QEMU_URL
    fi

    echo "${QEMU_SHA}  ${TMP}/${QEMU_TARBALL}" | sha1sum --check -
  else
    if [[ ! -f "$QEMU_TARBALL" ]] ; then
      echo "ERROR: missing qemu tarball: $QEMU_TARBALL"
      exit 1
    fi
  fi

  rm -rf ${tmpdir}
  mkdir ${tmpdir}
  cd ${tmpdir}
  SubBanner "Untaring ${QEMU_TARBALL}"
  tar xf ${TMP}/${QEMU_TARBALL}
  cd ${QEMU_DIR}

  SubBanner "Patching ${QEMU_PATCH}"
  patch -p1 < ${QEMU_PATCH}

  SubBanner "Configuring"
  set -x
  env -i PATH=/usr/bin/:/bin LIBS=-lrt \
    ./configure \
    --extra-cflags="-m32 --sysroot=$DEBIAN_I386_SYSROOT" \
    --extra-ldflags="-Wl,-rpath-link=$DEBIAN_I386_SYSROOT/lib/i386-linux-gnu" \
    --disable-system \
    --disable-docs \
    --enable-linux-user \
    --disable-bsd-user \
    --target-list=arm-linux-user \
    --disable-smartcard-nss \
    --disable-sdl

# see above for why we can no longer use -static
#    --static

  SubBanner "Make"
  env -i PATH=/usr/bin/:/bin make MAKE_OPTS=${MAKE_OPTS}

  SubBanner "Install ${INSTALL_ROOT}"
  cp arm-linux-user/qemu-arm ${INSTALL_ROOT}
  cd ${saved_dir}
  cp tools/trusted_cross_toolchains/qemu_tool_arm.sh ${INSTALL_ROOT}
  ln -sf qemu_tool_arm.sh ${INSTALL_ROOT}/run_under_qemu_arm
  set +x
}

#@
#@ BuildJail
#@
#@    Build everything and package it
BuildJail() {
  ClearInstallDir
  InstallMissingArmLibrariesAndHeadersIntoJail \
    ${ARMHF_BASE_DEP_FILES} \
    ${ARMHF_EXTRA_DEP_FILES}
  CleanupJailSymlinks
  HacksAndPatches
  BuildAndInstallQemu
  CreateTarBall
}

#
# GeneratePackageList
#
#     Looks up package names in ${TMP}/Packages and write list of URLs
#     to output file.
#
GeneratePackageList() {
  local output_file=$1
  echo "Updating: ${output_file}"
  /bin/rm -f ${output_file}
  shift
  for pkg in $@ ; do
    local pkg_full=$(grep -A 1 "${pkg}\$" ${TMP}/Packages | tail -1 | egrep -o "pool/.*")
    if [[ -z ${pkg_full} ]]; then
        echo "ERROR: missing package: $pkg"
        exit 1
    fi
    echo $pkg_full | sed "s/^pool\///" >> $output_file
  done
  # sort -o does an in-place sort of this file
  sort $output_file -o $output_file
}

#@
#@ UpdatePackageLists
#@
#@     Regenerate the armhf package lists such that they contain an up-to-date
#@     list of URLs within the ubuntu archive.
#@
UpdatePackageLists() {
  local package_list="${TMP}/Packages.${DIST}.bz2"
  local package_list2="${TMP}/Packages.${DIST}-security.bz2"
  DownloadOrCopy ${PACKAGE_LIST} ${package_list}
  DownloadOrCopy ${PACKAGE_LIST2} ${package_list2}
  bzcat ${package_list} ${package_list2} | egrep '^(Package:|Filename:)' > ${TMP}/Packages

  GeneratePackageList ${ARMHF_BASE_DEP_LIST} "${ARMHF_BASE_PACKAGES}"
  GeneratePackageList ${ARMHF_EXTRA_DEP_LIST} "${ARMHF_EXTRA_PACKAGES}"
}

if [[ $# -eq 0 ]] ; then
  echo "ERROR: you must specify a mode on the commandline"
  echo
  Usage
  exit -1
elif [[ "$(type -t $1)" != "function" ]]; then
  echo "ERROR: unknown function '$1'." >&2
  echo "For help, try:"
  echo "    $0 help"
  exit 1
else
  ChangeDirectory
  SanityCheck
  "$@"
fi
