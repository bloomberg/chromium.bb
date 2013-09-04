#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies things related to bundle resources.
"""

import TestGyp

import sys

if sys.platform == 'darwin':
  # set |match| to ignore build stderr output.
  test = TestGyp.TestGyp(formats=['ninja', 'make', 'xcode'])

  CHDIR = 'bundle-resources'
  test.run_gyp('test.gyp', chdir=CHDIR)

  test.build('test.gyp', test.ALL, chdir=CHDIR)

  test.built_file_must_match('resource.app/Contents/Resources/secret.txt',
                             'abc\n', chdir=CHDIR)
  test.built_file_must_match('source_rule.app/Contents/Resources/secret.txt',
                             'ABC\n', chdir=CHDIR)

  # TODO(thakis): This currently fails with make.
  if test.format != 'make':
    test.built_file_must_match(
        'resource_rule.app/Contents/Resources/secret.txt', 'ABC\n', chdir=CHDIR)

  test.pass_test()
