#!/usr/bin/env python

"""
Verifies simple actions when using an explicit build target of 'all'.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('all.gyp', chdir='src')
test.relocate('src', 'relocate/src')

# Build all.
test.build('all.gyp', chdir='relocate/src')

# Check that nothing gets emitted from all.
if test.format == 'xcode':
  test.must_not_exist('relocate/src/dir1/build/Default/out.txt')
elif test.format == 'make':
  test.must_not_exist('relocate/src/out/Default/out.txt')
else:
  test.must_not_exist('relocate/src/dir1/Default/out.txt')

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

test.pass_test()
