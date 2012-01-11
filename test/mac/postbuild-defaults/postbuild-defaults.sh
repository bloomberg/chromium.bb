#!/bin/bash
# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

PLIST="${BUILT_PRODUCTS_DIR}"/Test.app/Contents/Info  # No trailing .plist
echo $(defaults read "${PLIST}" "CFBundleName") > "${BUILT_PRODUCTS_DIR}/result"
