#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies file copies using the build tool default.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('copies.gyp', chdir='src')

test.relocate('src', 'relocate/src')

test.build('copies.gyp', chdir='relocate/src')

test.must_match(['relocate', 'src', 'copies-out', 'file1'], "file1 contents\n")

if test.format == 'xcode':
  file2 = ['relocate', 'src', 'build', 'Default', 'copies-out', 'file2']
elif test.format in ('make',):
  file2 = ['relocate', 'src', 'out', 'Default', 'copies-out', 'file2']
else:
  file2 = ['relocate', 'src', 'Default', 'copies-out', 'file2']
test.must_match(file2, "file2 contents\n")

test.pass_test()
