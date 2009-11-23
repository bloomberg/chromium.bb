#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Select the valgrind built by build-valgrind-for-chromium.sh by default,
# but allow users to override this default without editing scripts and
# without specifying a commandline option

if test x"$CHROME_VALGRIND_BIN" = x
then
  # Figure out which valgrind is installed.  Use most recent one.
  # See build-valgrind-for-chromium.sh and its history for these constants.
  for SVNREV in '10880-redzone' '10880' '10771' '20090715'
  do
    CHROME_VALGRIND_BIN=/usr/local/valgrind-$SVNREV/bin
    test -x $CHROME_VALGRIND_BIN/valgrind && break
  done
fi

if ! test -x $CHROME_VALGRIND_BIN/valgrind
then
  echo "Could not find chromium's version of valgrind."
  echo "Please run build-valgrind-for-chromium.sh or set CHROME_VALGRIND_BIN."
  exit 1
fi
echo "Using ${CHROME_VALGRIND_BIN}/valgrind"
PATH="${CHROME_VALGRIND_BIN}:$PATH"

export THISDIR=`dirname $0`
PYTHONPATH=$THISDIR/../../webkit/tools/layout_tests/:$THISDIR/../python "$THISDIR/chrome_tests.py" "$@"
