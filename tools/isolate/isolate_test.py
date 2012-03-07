#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))


class Isolate(unittest.TestCase):
  def setUp(self):
    self.tempdir = tempfile.mkdtemp()
    self.result = os.path.join(self.tempdir, 'result')

  def tearDown(self):
    shutil.rmtree(self.tempdir)

  def _execute(self, args):
    cmd = [
        sys.executable, os.path.join(ROOT_DIR, 'isolate.py'),
        '--root', ROOT_DIR,
        '--result', self.result,
    ]
    subprocess.check_call(
        cmd + args,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)

  def test_check(self):
    cmd = [
        '--mode', 'check',
        'isolate_test.py',
    ]
    self._execute(cmd)
    self.assertTrue(os.path.isfile(self.result))

  def test_check_non_existant(self):
    cmd = [
        '--mode', 'check',
        'NonExistentFile',
    ]
    try:
      self._execute(cmd)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self.assertFalse(os.path.isfile(self.result))

  def test_run(self):
    cmd = [
        '--mode', 'run',
        'isolate_test.py',
        '--',
        sys.executable, 'isolate_test.py', '--ok',
    ]
    self._execute(cmd)
    self.assertTrue(os.path.isfile(self.result))

  def test_run_fail(self):
    cmd = [
        '--mode', 'run',
        'isolate_test.py',
        '--',
        sys.executable, 'isolate_test.py', '--fail',
    ]
    try:
      self._execute(cmd)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self.assertFalse(os.path.isfile(self.result))


def main():
  if len(sys.argv) == 1:
    unittest.main()
  if sys.argv[1] == '--ok':
    return 0
  if sys.argv[1] == '--fail':
    return 1
  unittest.main()


if __name__ == '__main__':
  sys.exit(main())
