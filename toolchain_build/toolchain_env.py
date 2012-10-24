#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Add the native_client/build directory to the import order."""

import os
import sys


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)
BUILD_DIR = os.path.join(NACL_DIR, 'build')
sys.path.insert(0, BUILD_DIR)
