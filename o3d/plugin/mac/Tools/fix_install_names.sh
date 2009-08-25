#!/bin/sh

# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Since the web-plugin is not the main executable, we need to change
# the @executable_path reference to @loader_path for our embedded frameworks
#

# First take care of Breakpad.framework
/usr/bin/install_name_tool -change \
  @executable_path/../Frameworks/Breakpad.framework/Versions/A/Breakpad \
  @loader_path/../Frameworks/Breakpad.framework/Versions/A/Breakpad \
  "$1"

# Cg.framework
/usr/bin/install_name_tool -change \
  @executable_path/../Library/Frameworks/Cg.framework/Cg \
  @loader_path/../Frameworks/Cg.framework/Cg \
  "$1"
