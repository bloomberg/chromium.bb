#!/bin/sh
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Run this script in the mozilla/security/nss/lib/ssl directory in a NSS
# source tree.
#
# Point patches_dir to the src/net/third_party/nss/patches directory in a
# chromium source tree.
patches_dir=/Users/wtc/chrome1/src/net/third_party/nss/patches

patch -p4 < $patches_dir/versionskew.patch

patch -p4 < $patches_dir/renegoscsv.patch

patch -p4 < $patches_dir/cachecerts.patch

patch -p4 < $patches_dir/peercertchain.patch

patch -p4 < $patches_dir/clientauth.patch

patch -p4 < $patches_dir/didhandshakeresume.patch

patch -p4 < $patches_dir/negotiatedextension.patch

patch -p4 < $patches_dir/getrequestedclientcerttypes.patch

patch -p4 < $patches_dir/restartclientauth.patch

patch -p4 < $patches_dir/channelid.patch

patch -p4 < $patches_dir/tlsunique.patch

patch -p4 < $patches_dir/ecpointform.patch

patch -p4 < $patches_dir/secretexporterlocks.patch

patch -p4 < $patches_dir/cbc.patch

patch -p4 < $patches_dir/aes256keylength.patch

patch -p4 < $patches_dir/suitebonly.patch

patch -p4 < $patches_dir/secitemarray.patch

patch -p4 < $patches_dir/unusedvariables.patch

patch -p4 < $patches_dir/handlecertstatus.patch
