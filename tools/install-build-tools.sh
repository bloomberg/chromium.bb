#!/bin/bash
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if [ "$1" != "linux64" -a "$1" != "mac" ]
then
  echo "Usage: $0 linux64|mac"
  exit 1
fi

set -x  # echo on
sha1=$(curl "https://chromium.googlesource.com/chromium/buildtools/+/master/$1/gn.sha1?format=TEXT" | base64 --decode)
curl -Lo gn "https://storage.googleapis.com/chromium-gn/$sha1"
chmod +x gn
sha1=$(curl "https://chromium.googlesource.com/chromium/buildtools/+/master/$1/clang-format.sha1?format=TEXT" | base64 --decode)
curl -Lo clang-format "https://storage.googleapis.com/chromium-clang-format/$sha1"
chmod +x clang-format
set +x  # echo off

# TODO(btolsch): Download tools to a specific directory instead of current
# working directory.
echo
echo "*************************************************************************"
echo "NOTE: gn and clang-format were downloaded to the current directory. If "
echo "this is not the root openscreen source directory, it'd be easiest to "
echo "move them there and run them from there."
echo "*************************************************************************"
