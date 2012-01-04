#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script must be run from the native_client/tools directory.
# It canonicalizes the timestamps of all the files in the argument
# directories to the svn revision date of the last commit that might
# have changed the toolchain contents.  This way, if all the files
# are identical in two builds, the tarballs will be identical too.

# Fail if any command fails or if any undefined $variable is evaluated.
set -u -e -o pipefail

# Choose the appropriate svn command to use (taken from glibc_revision.sh).
if [ "`uname -s`" = Darwin -o "`uname -o`" != "Cygwin" ]; then
  SVN=svn
else
  SVN=svn.bat
fi

# These are the files whose commits represent "changing the toolchain".
timestamp_files='Makefile REVISIONS'

# Get the time a file was last changed by a subversion commit (YYYYMMDDHHMMSS).
# svn info emits a line like:
#       Text Last Updated: 2011-12-22 00:44:12 +0000 (Thu, 22 Dec 2011)
# This function turns that into:
#       20111222004412
# Such strings are comparable as integers to deduce their order in time.
svn_timestamp() {
  LC_ALL=C TZ=UTC $SVN info "$1" |
  sed -n 's/^Text Last Updated: \([0-9]\{4\}\)-\([0-9]\{1,2\}\)-\([0-9]\{1,2\}\) \([0-9]\{1,2\}\):\([0-9]\{1,2\}\):\([0-9]\{1,2\}\) .*$/\1\2\3\4\5\6/p'
}

# Find the latest timestamp among the $timestamp_files.
timestamp=0
for timestamp_file in $timestamp_files; do
  file_timestamp=`svn_timestamp "$timestamp_file"`
  if [ "$file_timestamp" -gt $timestamp ]; then
    timestamp=$file_timestamp
  fi
done

if [ $timestamp -eq 0 ]; then
  echo >&2 "Failed to glean svn revision timestamp from $timestamp_files"
  exit 1
fi

# Turn that comparable number into the format "touch -t" wants.
# That is, "YYYYMMDDHHMMSS" becomes "YYYYMMDDHHMM.SS".
touch_timestamp="${timestamp%??}.${timestamp#????????????}"

# Use that as the modification time of all the files that will go into the
# toolchain tarball.
find "$@" -print0 | TZ=UTC xargs -0 touch -m -t "$touch_timestamp"
