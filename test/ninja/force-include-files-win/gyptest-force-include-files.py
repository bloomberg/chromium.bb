#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure ForcedIncludeFiles is emulated on Windows.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['ninja'])

  test.run_gyp('force-include-files.gyp')

  # We can't use existence tests because any case will pass, so we check the
  # contents of ninja files directly since that's what we're most concerned
  # with anyway.
  subninja = open(test.built_file_path('obj/my_target.ninja')).read()
  if not '/FIstring' in subninja:
    test.fail_test()
  if not '/FIvector' in subninja:
    test.fail_test()
  if not '/FIlist' in subninja:
    test.fail_test()

  test.pass_test()
