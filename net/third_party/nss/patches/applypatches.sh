#!/bin/sh
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Run this script in the nss/lib/ssl directory in a NSS source tree.
#
# Point patches_dir to the src/net/third_party/nss/patches directory in a
# chromium source tree.
patches_dir=/Users/sleevi/development/chromium/src/net/third_party/nss/patches

patch -p2 < $patches_dir/cachecerts.patch

patch -p2 < $patches_dir/didhandshakeresume.patch

patch -p2 < $patches_dir/getrequestedclientcerttypes.patch

patch -p2 < $patches_dir/restartclientauth.patch

patch -p2 < $patches_dir/channelid.patch

patch -p2 < $patches_dir/tlsunique.patch

patch -p2 < $patches_dir/secretexporterlocks.patch

patch -p2 < $patches_dir/cachelocks.patch

patch -p2 < $patches_dir/cipherorder.patch

patch -p2 < $patches_dir/sessioncache.patch

patch -p2 < $patches_dir/reorderextensions.patch

patch -p2 < $patches_dir/nobypass.patch
