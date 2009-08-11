#!/bin/sh

# Select the valgrind built by build-valgrind-for-chromium.sh by default,
# but allow users to override this default without editing scripts and
# without specifying a commandline option
# SHORTSVNDATE should track value of same var in build-valgrind-for-chromium.sh
SHORTSVNDATE=20090715
CHROME_VALGRIND_BIN="${CHROME_VALGRIND_BIN:-/usr/local/valgrind-$SHORTSVNDATE/bin}"
PATH="${CHROME_VALGRIND_BIN}:$PATH"

export THISDIR=`dirname $0`
PYTHONPATH=$THISDIR/../../webkit/tools/layout_tests/:$THISDIR/../purify:$THISDIR/../python "$THISDIR/chrome_tests.py" "$@"
