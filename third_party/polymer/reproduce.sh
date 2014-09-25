#!/bin/bash

# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

cd "$(dirname "$0")"

rm -rf components
bower install
rm -rf components/web-animations-js
find components/core-list -type f -exec chmod -x {} \;

