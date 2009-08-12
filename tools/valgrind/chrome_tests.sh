#!/bin/sh

# Select the valgrind built by build-valgrind-for-chromium.sh by default,
# but allow users to override this default without editing scripts and
# without specifying a commandline option

if test x"$CHROME_VALGRIND_BIN" = x
then
  # Figure out which valgrind is installed.  Use most recent one.
  # See build-valgrind-for-chromium.sh and its history for these constants.
  for SVNREV in '10771' '{2009-07-15}'
  do
    SHORTSVNREV=`echo $SVNREV | tr -d '{\-}'`
    CHROME_VALGRIND_BIN=/usr/local/valgrind-$SHORTSVNREV/bin
    test -x $CHROME_VALGRIND_BIN/valgrind && break
  done
fi

if ! test -x $CHROME_VALGRIND_BIN/valgrind
then
  echo "Could not find chromium's version of valgrind."
  echo "Please run build-valgrind-for-chromium.sh or set CHROME_VALGRIND_BIN."
  exit 1
fi
PATH="${CHROME_VALGRIND_BIN}:$PATH"

export THISDIR=`dirname $0`
PYTHONPATH=$THISDIR/../../webkit/tools/layout_tests/:$THISDIR/../purify:$THISDIR/../python "$THISDIR/chrome_tests.py" "$@"
