#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import subprocess
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


class ListTestCases(unittest.TestCase):
  def capture(self, cmd):
    p = subprocess.Popen(
        [sys.executable] + cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        cwd=ROOT_DIR,
        universal_newlines=True)
    out, err = p.communicate()
    self.assertEqual('', err)
    self.assertEqual(0, p.returncode)
    return out

  def test_default(self):
    out = self.capture(
        [
          'list_test_cases.py',
          os.path.join('tests', 'gtest_fake', 'gtest_fake_pass.py'),
        ])
    self.assertEqual('Foo.Bar/3\nFoo.Bar1\nFoo.Bar2\n', out)


if __name__ == '__main__':
  logging.basicConfig(
      level=(logging.DEBUG if '-v' in sys.argv else logging.FATAL))
  unittest.main()
