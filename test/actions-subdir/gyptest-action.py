#!/usr/bin/env python

"""
Test actions that output to PRODUCT_DIR.
"""

import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('none.gyp', chdir='src')

test.build_all('none.gyp', chdir='src')

file_content = 'Hello from make-file.py\n'
subdir_file_content = 'Hello from make-subdir-file.py\n'

if test.format == 'xcode':
  test.must_match('src/build/Default/file.out', file_content)
  test.must_match('src/build/Default/subdir_file.out', subdir_file_content)
elif test.format == 'make':
  test.must_match('src/out/Default/file.out', file_content)
  test.must_match('src/out/Default/subdir_file.out', subdir_file_content)
else:
  test.must_match('src/Default/file.out', file_content)
  test.must_match('src/Default/subdir_file.out', subdir_file_content)

test.pass_test()
