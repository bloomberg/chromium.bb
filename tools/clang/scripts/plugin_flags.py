#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script returns the flags that should be used when GYP_DEFINES contains
# clang_use_chrome_plugins. The flags are stored in a script so that they can
# be changed on the bots without requiring a master restart.

import os
import sys

# Path constants. (All of these should be absolute paths.)
THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_DIR = os.path.abspath(os.path.join(THIS_DIR, '..', '..', '..'))
CLANG_LIB_PATH = os.path.join(CHROMIUM_DIR, 'third_party', 'llvm-build',
                              'Release+Asserts', 'lib')

if sys.platform == 'darwin':
  LIBSUFFIX = 'dylib'
else:
  LIBSUFFIX = 'so'

LIB_PATH = os.path.join(
    CLANG_LIB_PATH,
    'libFindBadConstructs.' + LIBSUFFIX)

print ('-Xclang -load -Xclang %s'
       ' -Xclang -add-plugin -Xclang find-bad-constructs') % LIB_PATH
