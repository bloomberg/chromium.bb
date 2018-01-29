#!/bin/sh
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e
export LANG=C
export LC_ALL=C

PROGNAME=$(basename $0)
PROGDIR=$(dirname "$0")

# Print an error message then exit immediately.
panic () {
  echo "ERROR: $@"
  exit 1
}

# Run a command through adb shell, strip the extra \r from the output
# and return the correct status code to detect failures. This assumes
# that the adb shell command prints a final \n to stdout.
# $1+: command to run
# Out: command's stdout
# Return: command's status
# Note: the command's stderr is lost
adb_shell () {
  local TMPOUT="$(mktemp)"
  local LASTLINE RET
  local ADB=${ADB:-adb}

  # The weird sed rule is to strip the final \r on each output line
  # Since 'adb shell' never returns the command's proper exit/status code,
  # we force it to print it as '%%<status>' in the temporary output file,
  # which we will later strip from it.
  echo "[$ADB shell $@ ; ... ]"
  $ADB shell $@ ";" echo "%%\$?" 2>/dev/null | \
      sed -e 's![[:cntrl:]]!!g' > $TMPOUT
  # Get last line in log, which contains the exit code from the command
  LASTLINE=$(sed -e '$!d' $TMPOUT)
  # Extract the status code from the end of the line, which must
  # be '%%<code>'.
  RET=$(echo "$LASTLINE" | \
    awk '{ if (match($0, "%%[0-9]+$")) { print substr($0,RSTART+2); } }')
  # Remove the status code from the last line. Note that this may result
  # in an empty line.
  LASTLINE=$(echo "$LASTLINE" | \
    awk '{ if (match($0, "%%[0-9]+$")) { print substr($0,1,RSTART-1); } }')
  # The output itself: all lines except the status code.
  sed -e '$d' $TMPOUT && printf "%s" "$LASTLINE"
  # Remove temp file.
  rm -f $TMPOUT
  # Exit with the appropriate status.
  return $RET
}

# Command-line parsing.
DO_HELP=
DO_TEST=
for OPT; do
  case $OPT in
    --output-dir=*)
      CHROMIUM_OUTPUT_DIR=${OPT##--output-dir=}
      ;;
    --help|-?)
      DO_HELP=true
      ;;
    -*)
      panic "Invalid option $OPT, see --help."
      ;;
    *)
      if [ -n "$DO_TEST" ]; then
        panic "This script can only take one argument"
      fi
      DO_TEST=$OPT
      ;;
  esac
done

if [ "$DO_HELP" ]; then
  cat <<EOF
Usage: $PROGNAME [options] [test-name]

This script is used to run all integration tests for the Android crazy linker.
You should have rebuilt all binaries with the following commands:

  gn \$OUT_DIR android_crazy_linker_tests

Then call this script with:

  \$OUT_DIR/bin/$PROGNAME

Where \$OUT_DIR is your Chromium output directory, which can be set either
through the CHROMIUM_OUTPUT_DIR environment variable or with the --output-dir
option.

Possible options:

   --help|-?           Print this message.
   --output-dir=<dir>  Manually set the Chromium output directory.

EOF
  exit 0
fi

if [ -z "$CHROMIUM_OUTPUT_DIR" ]; then
  # Check whether this is under $OUTPUT_DIR/bin/
  PROGDIR_PARENT=$(cd "$PROGDIR/.." && pwd)
  if [ -f "$PROGDIR_PARENT/bin/$PROGNAME" ]; then
    CHROMIUM_OUTPUT_DIR=$PROGDIR_PARENT
  fi
fi

if [ -z "$CHROMIUM_OUTPUT_DIR" ]; then
  panic "Please set CHROMIUM_OUTPUT_DIR or use --output-dir, see --help."
fi

if [ ! -d "$CHROMIUM_OUTPUT_DIR" ]; then
  panic "Invalid directory: $CHROMIUM_OUTPUT_DIR"
fi

# The directory on the device where all tests will be copied and run.
# TODO(digit): Create random temporary directory under /data/local/tmp/
RUN_DIR=/data/local/tmp

# The list of files to copy to $RUN_DIR/
LIBRARY_FILES="\
libcrazy_linker_tests_libbar.so \
libcrazy_linker_tests_libbar_with_relro.so \
libcrazy_linker_tests_libbar_with_two_dlopens.so \
libcrazy_linker_tests_libfoo.so \
libcrazy_linker_tests_libfoo_with_relro.so \
libcrazy_linker_tests_libfoo_with_static_constructor.so \
libcrazy_linker_tests_libfoo2.so \
libcrazy_linker_tests_libzoo.so \
libcrazy_linker_tests_libzoo_with_dlopen_handle.so \
"

# TODO(digit): Fix crazy_linker_test_load_library_callbacks and add it
# to the list.
TEST_FILES="\
crazy_linker_bench_load_library \
crazy_linker_test_constructors_destructors \
crazy_linker_test_dl_wrappers \
crazy_linker_test_dl_wrappers_with_system_handle \
crazy_linker_test_dl_wrappers_valid_handles \
crazy_linker_test_load_library \
crazy_linker_test_load_library_depends \
crazy_linker_test_relocated_shared_relro \
crazy_linker_test_search_path_list \
crazy_linker_test_shared_relro \
crazy_linker_test_two_shared_relros \
"

ALL_FILES="$LIBRARY_FILES $TEST_FILES"

# Check that all files were built properly.
MISSING_FILES=
for FILE in $ALL_FILES; do
  if [ ! -f "$CHROMIUM_OUTPUT_DIR/$FILE" ]; then
    echo "ERROR: Missing file: $FILE"
    MISSING_FILES="$MISSING_FILES $FILE"
  fi
done
if [ -n "$MISSING_FILES" ]; then
  panic "Please rebuild all crazy linker tests before using this script"
fi

(cd $CHROMIUM_OUTPUT_DIR && adb push $ALL_FILES $RUN_DIR/) ||
    panic "Could not copy files to Android device"

# Run a single test on the device.
run_test () {
  local TEST_NAME=$1
  shift
  adb_shell LD_LIBRARY_PATH=$RUN_DIR $RUN_DIR/$TEST_NAME "$@"
}

if [ -n "$DO_TEST" ]; then
  run_test "$DO_TEST"
else
  PASSES=0
  FAILURES=0
  TOTAL=0
  for TEST in $TEST_FILES; do
    echo "[ BEGIN     ] $TEST"
    if ! run_test $TEST; then
      echo "[    FAILED ] $TEST"
      FAILURES=$(( $FAILURES + 1 ))
    else
      PASSES=$(( $PASSES + 1 ))
      echo "[   SUCCESS ] $TEST"
    fi
    TOTAL=$(( $TOTAL + 1 ))
  done
  echo "Tests passed: $PASSES"
  echo "Tests failed: $FAILURES"
  echo "Tests total:  $TOTAL"
  if [ "$FAILURES" != "0" ]; then
    exit 1
  fi
  echo "SUCCESS!!"
fi

# TODO(digit): Cleanup by removing copied files from device.
