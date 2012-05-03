#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure macro expansion of $(VCInstallDir) is handled, and specifically
always / terminated for compatibility.
"""

import TestGyp

import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['msvs', 'ninja'])

  CHDIR = 'vs-macros'
  test.run_gyp('vcinstalldir.gyp', chdir=CHDIR)
  test.build('vcinstalldir.gyp', test.ALL, chdir=CHDIR)
  test.pass_test()
