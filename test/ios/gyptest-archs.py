#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that device and simulator bundles are built correctly.
"""

import plistlib
import TestGyp
import os
import struct
import subprocess
import sys
import tempfile


def CheckFileType(file, expected):
  proc = subprocess.Popen(['lipo', '-info', file], stdout=subprocess.PIPE)
  o = proc.communicate()[0].strip()
  assert not proc.returncode
  if not expected in o:
    print 'File: Expected %s, got %s' % (expected, o)
    test.fail_test()


if sys.platform == 'darwin':
  test = TestGyp.TestGyp()

  test.run_gyp('test-archs.gyp', chdir='app-bundle')
  test.set_configuration('Default')
  test.build('test-archs.gyp', test.ALL, chdir='app-bundle')

  for filename in ['Test No Archs', 'Test Archs i386', 'Test Archs x86_64']:
    result_file = test.built_file_path(
        '%s.bundle/%s' % (filename, filename), chdir='app-bundle')
    test.must_exist(result_file)

    expected = 'i386'
    if 'x86_64' in filename:
      expected = 'x86_64'
      CheckFileType(result_file, expected)

  test.pass_test()
