#!/bin/sh

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script can be used by waterfall sheriffs to fetch the status
# of Valgrind bots on the memory waterfall and test if their local
# suppressions match the reports on the waterfall.

set -e

THISDIR=$(dirname "${0}")
LOGS_DIR=$THISDIR/waterfall.tmp

fetch_logs() {
  # Fetch Valgrind logs from the waterfall {{{1

  WATERFALL_PAGE="http://build.chromium.org/buildbot/memory/builders"

  rm -rf "$LOGS_DIR" # Delete old logs
  mkdir "$LOGS_DIR"

  echo "Fetching the list of builders..."
  wget $WATERFALL_PAGE -O "$LOGS_DIR/builders" -q
  SLAVES=$(grep "<a href=\"builders\/" "$LOGS_DIR/builders" | \
           sed "s/.*<a href=\"builders\///" | sed "s/\".*//" | \
           sort | uniq)

  for S in $SLAVES
  do
    SLAVE_URL=$WATERFALL_PAGE/$S
    SLAVE_NAME=$(echo $S | sed "s/%20/ /g" | sed "s/%28/(/g" | sed "s/%29/)/g")
    echo -n "Fetching builds by slave '${SLAVE_NAME}'"
    wget $SLAVE_URL -O "$LOGS_DIR/slave_${S}" -q

    # We speed up the 'fetch' step by skipping the builds/tests which succeeded.
    # TODO(timurrrr): OTOH, we won't be able to check
    # if some suppression is not used anymore.
    LIST_OF_BUILDS=$(grep "<a href=\"\.\./builders/.*/builds/[0-9]\+.*failed" \
                     "$LOGS_DIR/slave_$S" | grep -v "failed compile" | \
                     sed "s/.*\/builds\///" | sed "s/\".*//" | head -n 2)

    for BUILD in $LIST_OF_BUILDS
    do
      BUILD_RESULTS="$LOGS_DIR/slave_${S}_build_${BUILD}"
      wget $SLAVE_URL/builds/$BUILD -O "$BUILD_RESULTS" -q
      LIST_OF_TESTS=$(grep "<a href=\"[0-9]\+/steps/memory.*/logs/stdio\"" \
                      "$BUILD_RESULTS" | \
                      sed "s/.*a href=\"//" | sed "s/\".*//")
      for TEST in $LIST_OF_TESTS
      do
        REPORT_FILE=$(echo "report_${S}_$TEST" | sed "s/\/logs\/stdio//" | \
                    sed "s/\/steps//" | sed "s/\//_/g")
        echo -n "."
        wget $SLAVE_URL/builds/$TEST -O "$LOGS_DIR/$REPORT_FILE" -q
        echo $SLAVE_URL/builds/$TEST >> "$LOGS_DIR/$REPORT_FILE"
      done
    done
    echo " DONE"
  done
  # }}}
}

match_suppressions() {
  PYTHONPATH=$THISDIR/../python/google \
             python "$THISDIR/test_suppressions.py" "$LOGS_DIR/report_"*
}

if [ "$1" = "fetch" ]
then
  fetch_logs
elif [ "$1" = "match" ]
then
  match_suppressions
else
  THISNAME=$(basename "${0}")
  echo "Usage: $THISNAME fetch|match"
  echo "  fetch - Fetch Valgrind logs from the memory waterfall"
  echo "  match - Test the local suppression files against the downloaded logs"
fi
