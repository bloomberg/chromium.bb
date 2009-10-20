#!/usr/bin/env python

"""
Verifies simple actions when using an explicit build target of 'all'.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('bare.gyp', chdir='src')
test.relocate('src', 'relocate/src')
test.build_default('bare.gyp', chdir='relocate/src')

file_content = 'Hello from bare.py\n'

if test.format == 'xcode':
  test.must_match('relocate/src/build/Default/out.txt', file_content)
elif test.format == 'make':
  test.must_match('relocate/src/out/Default/out.txt', file_content)
else:
  test.must_match('relocate/src/Default/out.txt', file_content)

test.pass_test()
