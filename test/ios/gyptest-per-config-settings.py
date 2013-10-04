#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that device and simulator bundles are built correctly.
"""

import TestGyp
import subprocess
import sys


def CheckFileType(file, expected):
  proc = subprocess.Popen(['lipo', '-info', file], stdout=subprocess.PIPE)
  o = proc.communicate()[0].strip()
  assert not proc.returncode
  if not expected in o:
    print 'File: Expected %s, got %s' % (expected, o)
    test.fail_test()


if sys.platform == 'darwin':
  # TODO(justincohen): Enable this in xcode too once ninja can codesign and bots
  # are configured with signing certs.
  test = TestGyp.TestGyp(formats=['ninja'])

  test.run_gyp('test-device.gyp', chdir='app-bundle')

  for configuration in ['Default-iphoneos', 'Default']:
    test.set_configuration(configuration)
    test.build('test-device.gyp', test.ALL, chdir='app-bundle')
    result_file = test.built_file_path('Test App Gyp.bundle/Test App Gyp',
                                       chdir='app-bundle')
    test.must_exist(result_file)
    if configuration == 'Default-iphoneos':
      CheckFileType(result_file, 'armv7')
    else:
      CheckFileType(result_file, 'i386')

  test.pass_test()
