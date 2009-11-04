#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to run layout tests under valgrind
# Example:
#  sh $0 LayoutTests/fast
# Caveats:
#  More of an example than a universal script.
#  Must be run from src directory.
#  Uses our standard suppressions; edit
#    tools/valgrind/memcheck/suppressions.txt
#  to disable any for bugs you're trying to reproduce.

# Copied from valgrind.sh
if test x"$CHROME_VALGRIND_BIN" = x
then
  # Figure out which valgrind is installed.  Use most recent one.
  # See build-valgrind-for-chromium.sh and its history for these constants.
  for SVNREV in 10880-redzone 10880 10771 20090715
  do
    CHROME_VALGRIND_BIN=/usr/local/valgrind-$SVNREV/bin
    test -x $CHROME_VALGRIND_BIN/valgrind && break
  done
fi

if ! test -x $CHROME_VALGRIND_BIN/valgrind
then
  echo "Could not find chromium's version of valgrind."
  echo "Please run build-valgrind-for-chromium.sh or set CHROME_VALGRIND_BIN."
  echo "Defaulting to system valgrind."
else
  echo "Using ${CHROME_VALGRIND_BIN}/valgrind."
  PATH="${CHROME_VALGRIND_BIN}:$PATH"
fi

cat > vlayout-wrapper.sh <<"_EOF_"
#!/bin/sh
valgrind --suppressions=tools/valgrind/memcheck/suppressions.txt --tool=memcheck --smc-check=all --num-callers=30 --trace-children=yes --leak-check=full --show-possible=no --log-file=vlayout-%p.log --gen-suppressions=all --track-origins=yes "$@"
_EOF_
chmod +x vlayout-wrapper.sh

rm -f vlayout-*.log
export BROWSER_WRAPPER=`pwd`/vlayout-wrapper.sh
export G_SLICE=always-malloc
export NSS_DISABLE_ARENA_FREE_LIST=1
sh webkit/tools/layout_tests/run_webkit_tests.sh --run-singly -v --noshow-results --time-out-ms=200000 --nocheck-sys-deps --debug "$@"

# Have to wait a bit for the output files to finish
nfiles=`ls vlayout-*.log | wc -l`
while true
do
  ndone=`egrep -l "LEAK SUMMARY|no leaks are possible" vlayout-*.log | wc -l`
  if test $nfiles = $ndone
  then
    break
  fi
  echo "Waiting for valgrind to finish..."
  sleep 1
done
cat vlayout-*.log
