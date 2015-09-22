#!/bin/bash -e
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This tool is used to update libvpx source code with the latest git
# repository.
#
# Make sure you run this in a git checkout of deps/third_party/libvpx_new!

# Usage:
#
# $ ./update_libvpx.sh [branch | revision | file or url containing a revision]
# When specifying a branch it may be necessary to prefix with origin/

# Tools required for running this tool:
#
# 1. Linux / Mac
# 2. git

export LC_ALL=C

# Location for the remote git repository.
GIT_REPO="https://chromium.googlesource.com/webm/libvpx"

GIT_BRANCH="origin/master"
LIBVPX_SRC_DIR="source/libvpx"
BASE_DIR=`pwd`

if [ -n "$1" ]; then
  GIT_BRANCH="$1"
  if [ -f "$1"  ]; then
    GIT_BRANCH=$(<"$1")
  elif [[ $1 = http* ]]; then
    GIT_BRANCH=`curl $1`
  fi
fi

prev_hash="$(egrep "^Commit: [[:alnum:]]" README.chromium | awk '{ print $2 }')"
echo "prev_hash:$prev_hash"

rm -rf $LIBVPX_SRC_DIR
mkdir $LIBVPX_SRC_DIR
cd $LIBVPX_SRC_DIR

# Start a local git repo.
git clone $GIT_REPO .

# Switch the content to the latest git repo.
git checkout -b tot $GIT_BRANCH

add="$(git diff-index --diff-filter=A $prev_hash | grep -v vp10/ | \
tr -s [:blank:] ' ' | cut -f6 -d\ )"
delete="$(git diff-index --diff-filter=D $prev_hash | grep -v vp10/ | \
tr -s [:blank:] ' ' | cut -f6 -d\ )"

# Get the current commit hash.
hash=$(git log -1 --format="%H")

# README reminder.
echo "Update README.chromium:"
echo "==============="
echo "Date: $(date +"%A %B %d %Y")"
echo "Branch: master"
echo "Commit: $hash"
echo "==============="
echo ""

# Commit message header.
echo "Commit message:"
echo "==============="
echo "libvpx: Pull from upstream"
echo ""

# Output the current commit hash.
echo "Current HEAD: $hash"
echo ""

# Output log for upstream from current hash.
if [ -n "$prev_hash" ]; then
  echo "git log from upstream:"
  pretty_git_log="$(git log \
                    --no-merges \
                    --topo-order \
                    --pretty="%h %s" \
                    --max-count=20 \
                    $prev_hash..$hash)"
  if [ -z "$pretty_git_log" ]; then
    echo "No log found. Checking for reverts."
    pretty_git_log="$(git log \
                      --no-merges \
                      --topo-order \
                      --pretty="%h %s" \
                      --max-count=20 \
                      $hash..$prev_hash)"
  fi
  echo "$pretty_git_log"
  # If it makes it to 20 then it's probably skipping even more.
  if [ `echo "$pretty_git_log" | wc -l` -eq 20 ]; then
    echo "<...>"
  fi
fi

# Commit message footer.
echo ""
echo "TBR=tomfinegan@chromium.org"
echo "==============="

# Git is useless now, remove the local git repo.
# Remove vp10 to cut down on code churn since it is not currently built.
rm -rf .git vp10

# Add and remove files.
echo "$add" | xargs -I {} git add {}
echo "$delete" | xargs -I {} git rm --ignore-unmatch {}

# Find empty directories and remove them.
find . -type d -empty -exec git rm {} \;

# Mark the scripts as executable so presubmit doesn't complain. The important
# ones already have the x bit set but some of the files they include do not.
chmod 755 build/make/*.sh build/make/*.pl configure

# The build system doesn't support files with the same name. Search vp8, vp9 and vpx_dsp
# for such files.
duplicate_files=$(find vp8 vp9 vpx_dsp -type f | xargs basename | sort | uniq -d | \
  egrep -v '(^exports_(dec|enc)|\.h)$' | \
  xargs -I {} find vp8 vp9 vpx_dsp -name {})

if [ -n "$duplicate_files" ]; then
  echo "WARNING: DUPLICATE FILES FOUND"
  echo "It would be a good idea to resolve these before attempting to roll"
  echo $duplicate_files
fi

cd $BASE_DIR
