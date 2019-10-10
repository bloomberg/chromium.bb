#!/usr/bin/env bash
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

uname="$(uname -s)"
case "${uname}" in
    Linux*)     env="linux64";;
    Darwin*)    env="mac";;
esac

echo "Assuming we are running in $env..."

ninja_version="v1.9.0"
ninja_zipfile=""
case "$env" in
  linux64)    ninja_zipfile="ninja-linux.zip";;
  mac)        ninja_zipfile="ninja-mac.zip";;
esac

GOOGLE_STORAGE_URL="https://storage.googleapis.com"
BUILDTOOLS_ROOT=$(git rev-parse --show-toplevel)/buildtools/$env
if [ ! -d $BUILDTOOLS_ROOT ]; then
  mkdir -p $BUILDTOOLS_ROOT
fi

pushd $BUILDTOOLS_ROOT
set -x  # echo on
sha1=$(tail -c+1 $BUILDTOOLS_ROOT/clang-format.sha1)
curl -Lo clang-format "$GOOGLE_STORAGE_URL/chromium-clang-format/$sha1"
chmod +x clang-format
curl -L "https://github.com/ninja-build/ninja/releases/download/${ninja_version}/${ninja_zipfile}" | funzip > ninja
chmod +x ninja
set +x  # echo off
popd

