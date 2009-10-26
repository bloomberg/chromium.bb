#!/bin/bash

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a small script for manually launching valgrind, along with passing
# it the suppression file, and some helpful arguments (automatically attaching
# the debugger on failures, etc).  Run it from your repo root, something like:
#  $ sh ./tools/valgrind/valgrind.sh ./sconsbuild/Debug/chrome
#
# This is mostly intended for running the chrome browser interactively.
# To run unit tests, you probably want to run chrome_tests.sh instead.
# That's the script used by the valgrind buildbot.

setup_memcheck() {
  # Prefer a 32-bit gdb if it's available.
  GDB="/usr/bin/gdb32";
  if [ ! -x $GDB ]; then
    GDB="gdb"
  fi

  # Prompt to attach gdb when there was an error detected.
  DEFAULT_TOOL_FLAGS=("--db-command=$GDB -nw %f %p" "--db-attach=yes" \
                      # Overwrite newly allocated or freed objects
                      # with 0x41 to catch inproper use.
                      "--malloc-fill=41" "--free-fill=41")
}

setup_tsan() {
  IGNORE_FILE="$(cd `dirname "$0"` && pwd)/tsan/ignores.txt"
  DEFAULT_TOOL_FLAGS=("--announce-threads" "--pure-happens-before=yes" \
                      "--ignore=$IGNORE_FILE")
}

setup_unknown() {
  echo "Unknown tool \"$TOOL_NAME\" specified, the result is not guaranteed"
  DEFAULT_TOOL_FLAGS=()
}

set -e

if [ $# -eq 0 ]; then
  echo "usage: <command to run> <arguments ...>"
  exit 1
fi

TOOL_NAME="memcheck"
declare -a DEFAULT_TOOL_FLAGS[0]

# Select a tool different from memcheck with --tool=TOOL as a first argument
TMP_STR=`echo $1 | sed 's/^\-\-tool=//'`
if [ "$TMP_STR" != "$1" ]; then
  TOOL_NAME="$TMP_STR"
  shift
fi

if echo "$@" | grep "\-\-tool" ; then
  echo "--tool=TOOL must be the first argument" >&2
  exit 1
fi

case $TOOL_NAME in
  memcheck*)  setup_memcheck;;
  tsan*)      setup_tsan;;
  *)          setup_unknown;;
esac

SUPPRESSIONS="$(cd `dirname "$0"` && pwd)/$TOOL_NAME/suppressions.txt"

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

set -x

# G_SLICE=always-malloc: make glib use system malloc
# NSS_DISABLE_ARENA_FREE_LIST=1: make nss use system malloc
# G_DEBUG=fatal_warnings: make  GTK abort on any critical or warning assertions.
# If it crashes on you in the Options menu, you hit bug 19751,
# comment out the G_DEBUG=fatal_warnings line.
#
# GTEST_DEATH_TEST_USE_FORK=1: make gtest death tests valgrind-friendly
#
# When everyone has the latest valgrind, we might want to add
#  --show-possible=no
# to ignore possible but not definite leaks.

G_SLICE=always-malloc \
NSS_DISABLE_ARENA_FREE_LIST=1 \
G_DEBUG=fatal_warnings \
GTEST_DEATH_TEST_USE_FORK=1 \
valgrind \
  --tool=$TOOL_NAME \
  --trace-children=yes \
  --suppressions="$SUPPRESSIONS" \
  "${DEFAULT_TOOL_FLAGS[@]}" \
  "$@"
