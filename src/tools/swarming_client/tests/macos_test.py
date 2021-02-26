#!/usr/bin/env vpython
# Copyright 2020 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import os
import sys
import unittest

import six

# Mutates sys.path.
import test_env

from utils import macos


@unittest.skipIf(sys.platform != 'darwin', 'this test is only for mac')
class MacosTest(unittest.TestCase):

  @unittest.skipIf(six.PY2, 'this is only for python3')
  def test_get_errno(self):
    self.assertEqual(macos.get_errno(OSError('MAC Error -43')), -43)

  def test_native_case(self):
    path = os.path.abspath(os.path.basename(__file__))
    self.assertEqual(macos.native_case(path), path)


if __name__ == '__main__':
  test_env.main()
