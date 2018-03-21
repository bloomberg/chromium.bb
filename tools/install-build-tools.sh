#!/bin/bash
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO(btolsch): Download tools to a specific directory instead of current
# working directory.
# GN
sha1=$(curl "https://chromium.googlesource.com/chromium/buildtools/+/master/linux64/gn.sha1?format=TEXT" | base64 --decode)
curl -Lo gn "https://storage.googleapis.com/chromium-gn/$sha1"
chmod a+x gn

# clang-format
sha1=$(curl "https://chromium.googlesource.com/chromium/buildtools/+/master/linux64/clang-format.sha1?format=TEXT" | base64 --decode)
curl -Lo clang-format "https://storage.googleapis.com/chromium-clang-format/$sha1"
chmod a+x clang-format
