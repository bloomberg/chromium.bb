#!/bin/bash
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script copies all dependencies required for trace collection.
# Usage:
#   deploy.sh builddir outdir bucket
#
# Where:
#   builddir is the build directory for Chrome
#   outdir is the directory where files are deployed
#   bucket is the Google Storage bucket where Chrome is uploaded

builddir=$1
outdir=$2
bucket=$3

# Copy files from tools/android/loading
mkdir -p $outdir/tools/android/loading
cp tools/android/loading/*.py $outdir/tools/android/loading
cp -r tools/android/loading/gce $outdir/tools/android/loading

# Copy other dependencies
mkdir $outdir/third_party
# Use rsync to exclude unwanted files (e.g. the .git directory).
rsync -av --exclude=".*" --exclude "*.pyc" --exclude "*.html" --exclude "*.md" \
  --delete third_party/catapult $outdir/third_party
mkdir $outdir/tools/perf
cp -r tools/perf/chrome_telemetry_build $outdir/tools/perf
mkdir -p $outdir/build/android
cp build/android/devil_chromium.py $outdir/build/android/
cp build/android/video_recorder.py $outdir/build/android/
cp build/android/devil_chromium.json $outdir/build/android/
cp -r build/android/pylib $outdir/build/android/

# Copy the chrome executable to Google Cloud Storage
chrome/tools/build/make_zip.py $builddir chrome/tools/build/linux/FILES.cfg \
  /tmp/linux.zip
gsutil cp /tmp/linux.zip gs://$bucket/chrome/linux.zip
rm /tmp/linux.zip

# Upload Chromium revision
CHROMIUM_REV=$(git merge-base HEAD origin/master)
cat >/tmp/build_metadata.json << EOF
{
  "chromium_rev": "$CHROMIUM_REV"
}
EOF
gsutil cp /tmp/build_metadata.json gs://$bucket/chrome/build_metadata.json
rm /tmp/build_metadata.json

