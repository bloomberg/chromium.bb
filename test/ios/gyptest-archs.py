#!/usr/bin/env python

# Copyright (c) 2013 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that device and simulator bundles are built correctly.
"""

import TestGyp
import TestMac

import collections
import plistlib
import os
import re
import struct
import subprocess
import sys
import tempfile


if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja', 'xcode'])

  test_cases = [
    ('Default', 'TestNoArchs', ['i386']),
    ('Default', 'TestArch32Bits', ['i386']),
    ('Default', 'TestArch64Bits', ['x86_64']),
    ('Default', 'TestMultiArchs', ['i386', 'x86_64']),
    ('Default-iphoneos', 'TestNoArchs', ['armv7']),
    ('Default-iphoneos', 'TestArch32Bits', ['armv7']),
    ('Default-iphoneos', 'TestArch64Bits', ['arm64']),
    ('Default-iphoneos', 'TestMultiArchs', ['armv7', 'arm64']),
  ]

  test.run_gyp('test-archs.gyp', chdir='app-bundle')
  for configuration, target, archs in test_cases:
    is_64_bit_build = ('arm64' in archs or 'x86_64' in archs)
    is_device_build = configuration.endswith('-iphoneos')

    kwds = collections.defaultdict(list)
    if test.format == 'xcode' and is_device_build:
      configuration, sdk = configuration.split('-')
      kwds['arguments'].extend(['-sdk', sdk])

    # TODO(sdefresne): remove those special-cases once the bots have been
    # updated to use a more recent version of Xcode.
    if TestMac.Xcode.Version() < '0500':
      if is_64_bit_build:
        continue
      if test.format == 'xcode':
        arch = 'i386'
        if is_device_build:
          arch = 'armv7'
        kwds['arguments'].extend(['-arch', arch])
    elif TestMac.Xcode.Version() >= '0510':
      if target == 'TestNoArchs':
        continue

    test.set_configuration(configuration)
    filename = '%s.bundle/%s' % (target, target)
    test.build('test-archs.gyp', target, chdir='app-bundle', **kwds)
    result_file = test.built_file_path(filename, chdir='app-bundle')

    test.must_exist(result_file)
    TestMac.CheckFileType(test, result_file, archs)

  test.pass_test()
