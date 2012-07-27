#!/bin/bash -e

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if [[ -z "$version_full" ]]; then
  echo 'Error: $version_full not set'
  exit 1
fi

# Create a fresh debian/changelog.
export DEBEMAIL="The Chromium Authors <chromium-dev@chromium.org>"
rm -f debian/changelog
debchange --create \
  --package chrome-remote-desktop \
  --distribution lucid \
  --newversion "$version_full" \
  "New Debian package"

dpkg-buildpackage -b -us -uc
