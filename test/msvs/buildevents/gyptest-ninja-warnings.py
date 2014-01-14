#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that ninja warns about msvs_prebuild/msvs_postbuild.
"""

import sys
import TestGyp


if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['ninja'])

  test.run_gyp('buildevents.gyp')

  test.must_contain_all_lines(test.stdout(),
    ['Warning: msvs_prebuild not supported, dropping. (target main)',
     'Warning: msvs_postbuild not supported, dropping. (target main)'])

  test.pass_test()
