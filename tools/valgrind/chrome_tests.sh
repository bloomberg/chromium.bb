#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Set up some paths and re-direct the arguments to chrome_tests.py

export THISDIR=`dirname $0`

CHROME_VALGRIND=`sh $THISDIR/locate_valgrind.sh`
if [ "$CHROME_VALGRIND" = "" ]
then
  # locate_valgrind.sh failed
  exit 1
fi
echo "Using valgrind binaries from ${CHROME_VALGRIND}"

PATH="${CHROME_VALGRIND}/bin:$PATH"
# We need to set these variables to override default lib paths hard-coded into
# Valgrind binary.
export VALGRIND_LIB="$CHROME_VALGRIND/lib/valgrind"
export VALGRIND_LIB_INNER="$CHROME_VALGRIND/lib/valgrind"

PYTHONPATH=$THISDIR/../python/google "$THISDIR/chrome_tests.py" "$@"
