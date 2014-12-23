#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies mac bundles work with --generator-output.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=[])

  # Bug: xcode-ninja doesn't respect --generator-output
  # cf. https://code.google.com/p/gyp/issues/detail?id=442
  if test.format == 'xcode-ninja':
    test.skip_test()

  MAC_BUNDLE_DIR = 'mac-bundle'
  GYPFILES_DIR = 'gypfiles'
  test.writable(test.workpath(MAC_BUNDLE_DIR), False)
  test.run_gyp('test.gyp',
               '--generator-output=' + test.workpath(GYPFILES_DIR),
               chdir=MAC_BUNDLE_DIR)
  test.writable(test.workpath(MAC_BUNDLE_DIR), True)

  test.build('test.gyp', test.ALL, chdir=GYPFILES_DIR)

  test.pass_test()
