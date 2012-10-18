#!/bin/bash -e

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if [[ -z "$version_full" ]]; then
  src_root=./../../../..
  remoting_version_path=$src_root/remoting/VERSION
  chrome_version_path=$src_root/chrome/VERSION
  version_helper=$src_root/chrome/tools/build/version.py

  # TODO(lambroslambrou): Refactor to share the logic with remoting.gyp.
  version_major=$($version_helper -f $chrome_version_path \
                  -f $remoting_version_path -t "@MAJOR@")
  version_minor=$($version_helper -f $remoting_version_path \
                  -t "@REMOTING_PATCH@")
  version_build=$($version_helper -f $chrome_version_path \
                  -f $remoting_version_path -t "@BUILD@")
  version_patch=$($version_helper -f $chrome_version_path \
                  -f $remoting_version_path -t "@PATCH@")
  version_full="$version_major.$version_minor.$version_build.$version_patch"
fi

if [[ ! "$version_full" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Error: Invalid \$version_full value: $version_full" >&2
  exit 1
fi

echo "Building version $version_full"

# Create a fresh debian/changelog.
export DEBEMAIL="The Chromium Authors <chromium-dev@chromium.org>"
rm -f debian/changelog
debchange --create \
  --package chrome-remote-desktop \
  --distribution lucid \
  --newversion "$version_full" \
  "New Debian package"

dpkg-buildpackage -b -us -uc
