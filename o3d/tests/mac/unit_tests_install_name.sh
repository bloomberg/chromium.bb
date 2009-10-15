#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

/usr/bin/install_name_tool -change \
  @executable_path/../Library/Frameworks/Cg.framework/Cg \
  @executable_path/Library/Frameworks/Cg.framework/Cg \
  "${BUILT_PRODUCTS_DIR}/${EXECUTABLE_PATH}"
