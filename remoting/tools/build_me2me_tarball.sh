#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build a tarball distribution of the Virtual Me2Me host for Linux.
# This script should be run from the top-level "src" directory - the output
# tarball will be created in the parent directory (to avoid accidentally
# committing it if "git commit -a" is typed).
# To do a clean build, remove the out/Release directory before running this
# script.

set -eu

REVISION="$(git svn find-rev HEAD)"
if [ -z "$REVISION" ]; then
  echo "svn revision number not found"
  REVISION_SUFFIX=""
else
  REVISION_SUFFIX="_r$REVISION"
fi

echo "Building..."
make -j25 BUILDTYPE=Release remoting_host_keygen remoting_me2me_host

FILES="\
  remoting/tools/gaia_auth.py \
  remoting/tools/keygen.py \
  remoting/tools/me2me_virtual_host.py \
  out/Release/remoting_host_keygen \
  out/Release/remoting_me2me_host \
"

TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TEMP_DIR"' EXIT

TARBALL_DIR="virtual_me2me"
mkdir "$TEMP_DIR/$TARBALL_DIR"

for file in $FILES; do
  cp "$file" "$TEMP_DIR/$TARBALL_DIR"
  strip "$TEMP_DIR/$TARBALL_DIR/$file" 2>/dev/null || true
done

TARBALL="../virtual_me2me$REVISION_SUFFIX.tgz"

tar -zcf "$TARBALL" -C "$TEMP_DIR" "$TARBALL_DIR"

echo "Tarball built: $TARBALL"
