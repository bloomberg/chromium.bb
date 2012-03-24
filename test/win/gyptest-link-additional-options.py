#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Make sure additional options are handled.
"""

import TestGyp

import os
import subprocess
import sys

if sys.platform == 'win32':
  test = TestGyp.TestGyp(formats=['ninja', 'msvs'])

  CHDIR = 'linker-flags'
  test.run_gyp('additional-options.gyp', chdir=CHDIR)
  test.build('additional-options.gyp', test.ALL, chdir=CHDIR)

  def HasDynamicBase(exe):
    full_path = test.built_file_path(exe, chdir=CHDIR)
    proc = subprocess.Popen(['dumpbin', '/headers', full_path],
        stdout=subprocess.PIPE)
    output = proc.communicate()[0]
    return '                   Dynamic base' in output


  # Default is to have dynamic base, so it should be on for this one.
  if not HasDynamicBase('test_additional_none.exe'):
    test.fail_test()

  if HasDynamicBase('test_additional_few.exe'):
    test.fail_test()

  test.pass_test()
