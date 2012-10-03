#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies build of a static_library with the standalone_static_library flag set.
"""

import os
import subprocess
import sys
import TestGyp

test = TestGyp.TestGyp()

test.run_gyp('mylib.gyp')
test.build('mylib.gyp', target='prog')

# Verify that the static library is copied to PRODUCT_DIR
built_lib = test.built_file_basename('mylib', type=test.STATIC_LIB)
path = test.built_file_path(built_lib)
test.must_exist(path)

# Verify that the program runs properly
expect = 'hello from mylib.c\n'
test.run_built_executable('prog', stdout=expect)

# Verify that libmylib.a contains symbols.  "ar -x" fails on a 'thin' archive.
if test.format in ('make', 'ninja') and sys.platform.startswith('linux'):
  retcode = subprocess.call(['ar', '-x', path])
  assert retcode == 0

test.pass_test()