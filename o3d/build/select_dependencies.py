#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This file selects which of the sets of dependencies are used
   when Chrome includes o3d.gyp"""

import os.path
import sys

has_assets = os.path.exists(os.path.join("..", "o3d_assets"))
if has_assets:
  print "o3d_all.gyp"
else:
  print "o3d_minimal.gyp"
sys.exit(0)
