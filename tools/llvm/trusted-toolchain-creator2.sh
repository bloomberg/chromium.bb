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
#@ Usage of this TC for compilation:
#@  TODO(robertm)
#@
#@ Usage of this TC with qemu
#@  TODO(robertm)

######################################################################
# Config
######################################################################

set -o nounset
set -o errexit

CROSS_ARM_TC_PACKAGES="\
  binutils-arm-linux-gnueabi \
  cpp-4.5-arm-linux-gnueabi \
  cpp-arm-linux-gnueabi \
  gcc-4.5-arm-linux-gnueabi \
  gcc-4.5-arm-linux-gnueabi-base \
  libc6-armel-cross \
  libc6-dev-armel-cross \
  libgcc1-armel-cross \
  libgomp1-armel-cross \
  linux-libc-dev-armel-cross \
  gcc-4.5-locales \
  libmudflap0-4.5-dev-armel-cross \
  libgcc1-dbg-armel-cross \
  libgomp1-dbg-armel-cross \
  libmudflap0-dbg-armel-cross \
  libstdc++6-4.4-dev-armel-cross"

# Optional:
# gdb-arm-linux-gnueabi
# automake1.9
# libtool
CROSS_ARM_TC_DEP_FILES="\
  universe/g/gcc-4.5-armel-cross/cpp-4.5-arm-linux-gnueabi_4.5.2-8ubuntu3cross1.47_amd64.deb \
  universe/g/gcc-4.5-armel-cross/g++-4.5-arm-linux-gnueabi_4.5.2-8ubuntu3cross1.47_amd64.deb \
  universe/g/gcc-4.5-armel-cross/gcc-4.5-arm-linux-gnueabi_4.5.2-8ubuntu3cross1.47_amd64.deb \
  universe/g/gcc-4.5-armel-cross/gcc-4.5-arm-linux-gnueabi-base_4.5.2-8ubuntu3cross1.47_amd64.deb \
  universe/g/gcc-4.5-armel-cross/libgomp1-armel-cross_4.5.2-8ubuntu3cross1.47_all.deb \
  universe/g/gcc-4.5-armel-cross/libmudflap0-armel-cross_4.5.2-8ubuntu3cross1.47_all.deb \
  universe/g/gcc-4.5-armel-cross/libstdc++6-4.5-dev-armel-cross_4.5.2-8ubuntu3cross1.47_all.deb \
  universe/g/gcc-4.5-armel-cross/libstdc++6-armel-cross_4.5.2-8ubuntu3cross1.47_all.deb \
  universe/g/gcc-4.5-armel-cross/libgcc1-armel-cross_4.5.2-8ubuntu3cross1.62_all.deb \
  universe/a/armel-cross-toolchain-base/binutils-arm-linux-gnueabi_2.21.0.20110327-2ubuntu2cross1.62_amd64.deb \
  universe/a/armel-cross-toolchain-base/libc6-armel-cross_2.13-0ubuntu13cross1.62_all.deb \
  universe/a/armel-cross-toolchain-base/libc6-dev-armel-cross_2.13-0ubuntu13cross1.62_all.deb \
  universe/a/armel-cross-toolchain-base/linux-libc-dev-armel-cross_2.6.38-8.42cross1.62_all.deb \
  main/c/cloog-ppl/libcloog-ppl0_0.15.9-2_amd64.deb \
  main/m/mpfr4/libmpfr4_3.0.0-7_amd64.deb \
  main/c/cloog-ppl/libcloog-ppl0_0.15.9-2_amd64.deb"

readonly ARMEL_PACKAGES="\
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
  libxt6"

readonly ARMEL_DEP_FILES="\
  main/o/openssl/libssl-dev_0.9.8g-16ubuntu3_armel.deb \
  main/o/openssl/libssl0.9.8_0.9.8g-16ubuntu3_armel.deb \
  main/g/gcc-4.5/libgcc1_4.5.2-8ubuntu4_armel.deb \
  main/e/eglibc/libc6_2.13-0ubuntu13_armel.deb \
  main/e/eglibc/libc6-dev_2.13-0ubuntu13_armel.deb \
  main/g/gcc-4.5/libstdc++6_4.5.2-8ubuntu4_armel.deb \
  main/libx/libx11/libx11-dev_1.4.2-1ubuntu3_armel.deb \
  main/libx/libx11/libx11-6_1.4.2-1ubuntu3_armel.deb \
  main/x/x11proto-core/x11proto-core-dev_7.0.20-1_all.deb \
  main/libx/libxt/libxt-dev_1.0.9-1ubuntu1_armel.deb \
  main/libx/libxt/libxt6_1.0.9-1ubuntu1_armel.deb"

# this where we get the cross toolchain from for the manual install:
readonly CROSS_ARM_TC_REPO=http://mirror.pnl.gov/ubuntu/pool
# this is where we get all the armel packages from
readonly ARMEL_REPO=http://ports.ubuntu.com/ubuntu-ports/pool
#
readonly PACKAGE_LIST=http://ports.ubuntu.com/ubuntu-ports/dists/natty/main/binary-armel/Packages.bz2
# this where we create the ARMEL "jail"
readonly INSTALL_ROOT=$(pwd)/toolchain/linux_arm_natty_jail

readonly TMP=/tmp/arm-crosstool-natty

readonly REQUIRED_TOOLS="wget"

readonly  MAKE_OPTS="-j8"

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

# This has been tested on 64bit ubuntu natty
InstallCrossArmBasePackages() {
  sudo apt-get install ${CROSS_ARM_TC_PACKAGES}
}

# This should work even for 64bit ubuntu lucid machine
# The download part is more or less idem-potent, run it until
# all files have been downloaded
InstallCrossArmBasePackagesManual() {
  mkdir -p ${TMP}

  for i in ${CROSS_ARM_TC_DEP_FILES} ; do
    echo $i
    url=${CROSS_ARM_TC_REPO}/$i
    dst=${TMP}/$(basename $i)
    DownloadOrCopy ${url} ${dst}
  done

  sudo dpkg -i ${TMP}/*.deb
}


######################################################################
#
######################################################################

#@
#@
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

# libssl.so and libcrypto.so cannot be used because they have
# a dependency on libz.so. We don't download libz, although
# we probably should. For now, remove the .so and let the linker
# use the .a instead.
HacksAndPatches() {
  Banner "Misc Hacks & Patches"
  # these are linker scripts with absolute pathnames in them
  # which we rewrite here
  sed -i -e 's|/usr/lib/arm-linux-gnueabi/||g' \
    toolchain/linux_arm_natty_jail/usr/lib/arm-linux-gnueabi/libpthread.so \
    toolchain/linux_arm_natty_jail/usr/lib/arm-linux-gnueabi/libc.so
  sed -i -e 's|/lib/arm-linux-gnueabi/||g' \
    toolchain/linux_arm_natty_jail/usr/lib/arm-linux-gnueabi/libpthread.so \
    toolchain/linux_arm_natty_jail/usr/lib/arm-linux-gnueabi/libc.so

  # libssl.so and libcrypto.so cannot be used because they have
  # a dependency on libz.so. We don't download libz, although
  # we probably should. For now, remove the .so and let the linker
  # use the .a instead.
  rm -fv toolchain/linux_arm_natty_jail/usr/lib/libcrypto.so
  rm -fv toolchain/linux_arm_natty_jail/usr/lib/libssl.so.0.9.8
  rm -fv toolchain/linux_arm_natty_jail/usr/lib/libssl.so
  rm -fv toolchain/linux_arm_natty_jail/lib/libssl.so.0.9.8
}


InstallMissingArmLibrariesAndHeadersIntoJail() {
  mkdir -p ${TMP}
  mkdir -p ${INSTALL_ROOT}
  for file in ${ARMEL_DEP_FILES} ; do
    local package="${TMP}/${file##*/}"
    Banner "installing ${file}"
    DownloadOrCopy ${ARMEL_REPO}/${file} ${package}
    SubBanner "extracting to ${INSTALL_ROOT}"
    dpkg --fsys-tarfile ${package}\
      | tar -xvf - --exclude=./usr/share -C ${INSTALL_ROOT}
  done

  Banner "symlink cleanup"
  (
    cd ${INSTALL_ROOT}
    find usr/lib -type l -printf '%p %l\n' | while read link target; do
      case "${target}" in
        /*)
          # Relativize the symlink.
          ln -snfv "../..${target}" "${link}"
          ;;
      esac
      # Just in case there are dangling links other than ones
      # that just needed to be relativized, remove them.
      if [ ! -r "${link}" ]; then
        rm -fv "${link}"
      fi
    done
  )
}

#@
#@
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
  # HACK: eliminate this after we we retire the old script
  sed -i -e "s|/arm-2009q3/arm-none-linux-gnueabi/libc||"  \
    ${INSTALL_ROOT}/qemu_tool.sh
}

#@
#@ Regenerat Package List
#@ This will need some manual intervention, e.g.
#@ pool/ needs to be stripped and special characters like "+" cause problems
GeneratePackageList() {
  DownloadOrCopy ${PACKAGE_LIST} ${TMP}/Packages.bz2
  bzcat ${TMP}/Packages.bz2 | egrep '^(Package:|Filename:)' > ${TMP}/Packages
  echo "# BEGIN: update for ARMEL_DEP_FILES"
  for pkg in ${ARMEL_PACKAGES} ; do
    egrep -A 1 "${pkg}\$" ${TMP}/Packages | egrep -o "pool/.*"
  done
  echo "# END: update for ARMEL_DEP_FILES"
}

#@
#@
BuildJail() {
  ClearInstallDir
  InstallMissingArmLibrariesAndHeadersIntoJail
  InstallTrustedLinkerScript
  HacksAndPatches
  BuildAndInstallQemu
  CreateTarBall $1
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
