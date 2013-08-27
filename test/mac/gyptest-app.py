#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that app bundles are built correctly.
"""

import TestGyp

import os
import sys


def ls(path):
  '''Returns a list of all files in a directory, relative to the directory.'''
  result = []
  for dirpath, _, files in os.walk(path):
    for f in files:
      result.append(os.path.join(dirpath, f)[len(path) + 1:])
  return result


if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  test.run_gyp('test.gyp', chdir='app-bundle')

  test.build('test.gyp', test.ALL, chdir='app-bundle')

  # Binary
  test.built_file_must_exist('Test App Gyp.app/Contents/MacOS/Test App Gyp',
                             chdir='app-bundle')

  # Info.plist
  info_plist = test.built_file_path('Test App Gyp.app/Contents/Info.plist',
                                    chdir='app-bundle')
  test.must_exist(info_plist)
  test.must_contain(info_plist, 'com.google.Test App Gyp')  # Variable expansion
  test.must_not_contain(info_plist, '${MACOSX_DEPLOYMENT_TARGET}');

  # Resources
  test.built_file_must_exist(
      'Test App Gyp.app/Contents/Resources/English.lproj/InfoPlist.strings',
      chdir='app-bundle')
  test.built_file_must_exist(
      'Test App Gyp.app/Contents/Resources/English.lproj/MainMenu.nib',
      chdir='app-bundle')

  # Packaging
  test.built_file_must_exist('Test App Gyp.app/Contents/PkgInfo',
                             chdir='app-bundle')
  test.built_file_must_match('Test App Gyp.app/Contents/PkgInfo', 'APPLause',
                             chdir='app-bundle')

  # Check that no other files get added to the bundle.
  if set(ls(test.built_file_path('Test App Gyp.app', chdir='app-bundle'))) != \
     set(['Contents/MacOS/Test App Gyp',
          'Contents/Info.plist',
          'Contents/Resources/English.lproj/InfoPlist.strings',
          'Contents/Resources/English.lproj/MainMenu.nib',
          'Contents/PkgInfo',
          ]):
    test.fail_test()

  test.pass_test()
