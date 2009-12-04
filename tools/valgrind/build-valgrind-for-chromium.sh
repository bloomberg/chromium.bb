#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to build valgrind for use with chromium
#
# Can also be used to just prepare patched source tarball with MAKE_TARBALL=yes
# or to just build from a patched source tarball with USE_TARBALL=yes.
# These are useful when configuring jailed buildbots which are not allowed to
# fetch valgrind source via svn.  Most users won't need those options.

# Checkout by date doesn't work unless you specify the friggin' timezone
VALGRIND_SVN_REV=10880
# And svn isn't smart enough to figure out what rev of the linked tree to get
VEX_SVN_REV=1914
# and TSAN may be out of sync, so you have to check that out by rev anyway
TSAN_SVN_REV=1274

# suffix for build and install dir to denote our set of patches (may be empty)
PATCHLEVEL=-redzone

DIRNAME=valgrind-${VALGRIND_SVN_REV}${PATCHLEVEL}

THISDIR=$(dirname "${0}")
THISDIR=$(cd "${THISDIR}" && /bin/pwd)

case "x${1}" in
x|x/*) ;;
*)
  echo "Usage: sh build-valgrind-for-chromium.sh [prefix]"
  echo "Prefix is optional, but if present, must be the absolute path to where"
  echo "you want to install valgrind's bin, include, and lib directories."
  echo "Prefix defaults to /usr/local/$DIRNAME."
  echo "Will use sudo to do the install if you don't own the parent of prefix."
  exit 1
  ;;
esac

set -x
set -e

if test "x${USE_TARBALL}" = "xyes" && test "x${MAKE_TARBALL}" = "xyes"
then
  echo Set only one of MAKE_TARBALL or USE_TARBALL to yes
  exit 1
fi

# Clean checkout our untar
test -d "$DIRNAME" && rm -rf ./"$DIRNAME"
mkdir -p "$DIRNAME"

if test "x${USE_TARBALL}" != "xyes"
then
  # Check out latest version that following patches known to apply against
  svn co -r "${VALGRIND_SVN_REV}" "svn://svn.valgrind.org/valgrind/trunk" "$DIRNAME"

  cd "$DIRNAME"

  # Make sure svn gets the right version of the external VEX repo, too
  svn update -r "${VEX_SVN_REV}" VEX/

  # Work around bug https://bugs.kde.org/show_bug.cgi?id=162848
  # "fork() not handled properly"
  patch -p0 < "${THISDIR}/fork.patch"

  # Add feature bug https://bugs.kde.org/show_bug.cgi?id=201170
  # "Want --show-possible option so I can ignore the bazillion possible leaks..."
  patch -p0 < "${THISDIR}/possible.patch"

  # Add feature bug https://bugs.kde.org/show_bug.cgi?id=205000
  # "Need library load address in log files"
  patch -p0 < "${THISDIR}/xml-loadadr.patch"

  # Make red zone 64 bytes bigger to catch more buffer overruns
  patch -p0 < "${THISDIR}/redzone.patch"

  # Fix/work around https://bugs.kde.org/show_bug.cgi?id=210481
  # which prevented valgrind from handling v8 on 64 bits
  patch -p0 < "${THISDIR}/vbug210481.patch"

  # Fix/work around https://bugs.kde.org/show_bug.cgi?id=205541
  # which prevented valgrind from handling wine
  patch -p0 < "${THISDIR}/vbug205541.patch"

  # Add intercepts for tcmalloc memory functions.
  patch -p0 < "${THISDIR}/intercept_tcmalloc.patch"


  if [ "${INSTALL_TSAN}" = "yes" ]
  then
    # Add ThreadSanitier to the installation.
    # ThreadSanitizer is an experimental dynamic data race detector.
    # See http://code.google.com/p/data-race-test/wiki/ThreadSanitizer
    svn checkout -r "${TSAN_SVN_REV}" http://data-race-test.googlecode.com/svn/trunk/tsan tsan
    mkdir tsan/tests
    touch tsan/tests/Makefile.am
    patch -p 0 < tsan/valgrind.patch
  fi

  sh autogen.sh
  if test -L install-sh
  then
    # replace symlink with actual contents!
    cp install-sh install-sh.new
    mv -f install-sh.new install-sh
    chmod +x install-sh
  fi

  # MacOSX before Snow Leopoard needs newer gdb to be able to handle -O1 chrome
  # Kludgily download and unpack the sources in a subdirectory.
  if test `uname` = Darwin || test "x${MAKE_TARBALL}" = "xyes"
  then
    curl http://www.opensource.apple.com/tarballs/gdb/gdb-1344.tar.gz | tar -xzf -
  fi
  cd ..

fi

if test "x${MAKE_TARBALL}" = "xyes"
then
  tar -czvf "$DIRNAME".tgz "$DIRNAME"
fi

if test "x${USE_TARBALL}" = "xyes"
then
  tar -xzvf "$DIRNAME".tgz
fi

if test "x${MAKE_TARBALL}" != "xyes"
then
  cd "$DIRNAME"

  OVERRIDE_LD_DIR="${THISDIR}/override_ld"
  if ld --version | grep gold
  then
    # build/install-build-deps leaves original ld around, try using that
    if test -x /usr/bin/ld.orig
    then
      echo "Using /usr/bin/ld.orig instead of gold to link valgrind"
      test -d "${OVERRIDE_LD_DIR}" && rm -rf "${OVERRIDE_LD_DIR}"
      mkdir "${OVERRIDE_LD_DIR}"
      ln -s /usr/bin/ld.orig "${OVERRIDE_LD_DIR}/ld"
      PATH="${OVERRIDE_LD_DIR}:${PATH}"
    # Ubuntu diverts original ld to ld.single when it installs binutils-gold
    elif test -x /usr/bin/ld.single
    then
      echo "Using /usr/bin/ld.single instead of gold to link valgrind"
      test -d "${OVERRIDE_LD_DIR}" && rm -rf "${OVERRIDE_LD_DIR}"
      mkdir "${OVERRIDE_LD_DIR}"
      ln -s /usr/bin/ld.single "${OVERRIDE_LD_DIR}/ld"
      PATH="${OVERRIDE_LD_DIR}:${PATH}"
    else
      echo "Cannot build valgrind with gold.  Please switch to normal /usr/bin/ld, rerun this script, then switch back to gold."
      exit 1
    fi
  fi

  # Desired parent directory for valgrind's bin, include, etc.
  PREFIX="${1:-/usr/local/$DIRNAME}"
  parent_of_prefix=$(dirname "${PREFIX}")
  if test ! -d "${parent_of_prefix}"
  then
    echo "Directory ${parent_of_prefix} does not exist"
    exit 1
  fi

  ./configure --prefix="${PREFIX}"
  make -j4

  if ./vg-in-place true
  then
    echo built valgrind passes smoke test, good
  else
    echo built valgrind fails smoke test
    exit 1
  fi

  test -d "${OVERRIDE_LD_DIR}" && rm -rf "${OVERRIDE_LD_DIR}"

  # Build and install gdb if needed
  case `uname` in
  Darwin)
    cd gdb-1344/src
    ./configure --prefix="${PREFIX}"
    # gdb makefile is not yet parallel-safe
    make
    if test -w "${parent_of_prefix}"
    then
       make install
    else
       sudo make install
    fi
    cd ../..
    ;;
  esac

  # Finally install valgrind.
  # Don't use sudo if we own the destination
  if test -w "${parent_of_prefix}"
  then
     make install
  else
     sudo make install
  fi

  cd ..
fi
