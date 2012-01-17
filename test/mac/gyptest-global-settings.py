#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that the global xcode_settings processing doesn't throw.
Regression test for http://crbug.com/109163
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])
  test.run_gyp('src/dir2/dir2.gyp', chdir='global-settings', depth='src')
  # If run_gyp doesn't throw, this test passes.
  test.pass_test()
