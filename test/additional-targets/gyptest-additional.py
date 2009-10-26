#!/usr/bin/env python

# Copyright (c) 2009 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies simple actions when using an explicit build target of 'all'.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('all.gyp', chdir='src')
test.relocate('src', 'relocate/src')

# Build all.
test.build('all.gyp', chdir='relocate/src')

# Output is as expected.
file_content = 'Hello from emit.py\n'
if test.format == 'xcode':
  test.must_not_exist('relocate/src/dir1/build/Default/out.txt')
  test.must_match('relocate/src/dir1/build/Default/out2.txt', file_content)
  test.must_not_exist('relocate/src/dir1/build/Default/lib1.dylib')
elif test.format == 'make':
  test.must_not_exist('relocate/src/out/Default/out.txt')
  test.must_match('relocate/src/out/Default/out2.txt', file_content)
  test.must_not_exist('relocate/src/out/Default/obj.target/dir1/lib1.so')
elif test.format == 'scons':
  test.must_not_exist('relocate/src/dir1/Default/out.txt')
  test.must_match('relocate/src/Default/out2.txt', file_content)
  test.must_not_exist('relocate/src/dir1/Default/lib/lib1.so')
else:
  test.must_not_exist('relocate/src/dir1/Default/out.txt')
  test.must_match('relocate/src/Default/out2.txt', file_content)
  test.must_not_exist('relocate/src/dir1/Default/lib1.dll')

# Build the action explicitly.
if test.format == 'make':
  test.build('actions.gyp', 'action1_target', chdir='relocate/src')
else:
  test.build('actions.gyp', 'action1_target', chdir='relocate/src/dir1')

# Check that things got run.
file_content = 'Hello from emit.py\n'
if test.format == 'xcode':
  test.must_match('relocate/src/dir1/build/Default/out.txt', file_content)
elif test.format == 'make':
  test.must_match('relocate/src/out/Default/out.txt', file_content)
else:
  test.must_match('relocate/src/dir1/Default/out.txt', file_content)

# Build the shared library explicitly.
if test.format == 'make':
  test.build('actions.gyp', 'lib1', chdir='relocate/src')
else:
  test.build('actions.gyp', 'lib1', chdir='relocate/src/dir1')

# Check that things got run.
if test.format == 'xcode':
  test.must_exist('relocate/src/dir1/build/Default/lib1.dylib')
elif test.format == 'make':
  # TODO(mmoss) Make consistent with scons, with 'dir1' before 'out/Default'?
  test.must_exist('relocate/src/out/Default/lib/dir1/lib1.so')
elif test.format == 'scons':
  test.must_exist('relocate/src/dir1/Default/lib/lib1.so')
else:
  test.must_exist('relocate/src/dir1/Default/lib1.dll')

test.pass_test()
