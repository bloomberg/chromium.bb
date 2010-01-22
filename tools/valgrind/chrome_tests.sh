#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Select the valgrind built by build-valgrind-for-chromium.sh by default,
# but allow users to override this default without editing scripts and
# without specifying a commandline option

export THISDIR=`dirname $0`

# User may use his own valgrind by giving its path with CHROME_VALGRIND env.
if [ "$CHROME_VALGRIND" = "" ]
then
  # Guess which binaries we should use by uname
  case "$(uname -a)" in
  *Linux*x86_64*)
    PLATFORM="linux_x64"
    ;;
  *Linux*86*)
    PLATFORM="linux_x86"
    ;;
  *Darwin*9.[678].[01]*i386*)
    # Didn't test other kernels.
    PLATFORM="mac"
    ;;
  *)
    echo "Unknown platform:"
    uname -a
    echo "We'll try to search for valgrind binaries installed in /usr/local"
    PLATFORM=
  esac

  if [ "$PLATFORM" != "" ]
  then
    # The binaries should be in third_party/valgrind
    # (checked out from deps/third_party/valgrind/binaries).
    CHROME_VALGRIND="$THISDIR/../../third_party/valgrind/$PLATFORM"

    # TODO(timurrrr): readlink -f is not present on Mac...
    if [ "$PLATFORM" != "mac" ]
    then
      # Get rid of all "../" dirs
      CHROME_VALGRIND=`readlink -f $CHROME_VALGRIND`
    fi

    if ! test -x $CHROME_VALGRIND/bin/valgrind
    then
      # We couldn't find the binaries in third_party/valgrind
      CHROME_VALGRIND=""
    fi
  fi

  if [ "$CHROME_VALGRIND" = "" ]
  then
    # Couldn't find the binaries checked out from SVN.
    # Let's try to find out valgrind in /usr/local. Use the most recent one.
    # See build-valgrind-for-chromium.sh and its history for these constants.
    for SVNREV in '10880-redzone' '10880'
    do
      CHROME_VALGRIND=/usr/local/valgrind-$SVNREV
      test -x $CHROME_VALGRIND/bin/valgrind && break
    done
  fi
fi

if ! test -x $CHROME_VALGRIND/bin/valgrind
then
  # TODO(timurrrr): give a link to instructions on what to do in this case
  # once the SVN-binaries Valgrind deployment method is out of experimental.
  echo "Could not find valgrind binaries in ${CHROME_VALGRIND}."
  echo "Please run build-valgrind-for-chromium.sh or set CHROME_VALGRIND."
  exit 1
fi

# We need to set these variables to override default lib paths hard-coded into
# Valgrind binary.
export VALGRIND_LIB="$CHROME_VALGRIND/lib/valgrind"
export VALGRIND_LIB_INNER="$CHROME_VALGRIND/lib/valgrind"

echo "Using valgrind binaries from ${CHROME_VALGRIND}"
PATH="${CHROME_VALGRIND}/bin:$PATH"

PYTHONPATH=$THISDIR/../../webkit/tools/layout_tests/webkitpy/layout_tests:$THISDIR/../python "$THISDIR/chrome_tests.py" "$@"
