#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))


class Isolate(unittest.TestCase):
  def setUp(self):
    # The reason is that isolate_test.py --ok is run in a temporary directory
    # without access to isolate.py
    import isolate
    self.isolate = isolate
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

  def test_help_modes(self):
    # Check coherency in the help and implemented modes.
    p = subprocess.Popen(
        [sys.executable, os.path.join(ROOT_DIR, 'isolate.py'), '--help'],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    out = p.communicate()[0].splitlines()
    self.assertEquals(0, p.returncode)
    out = out[out.index('') + 1:]
    out = out[:out.index('')]
    modes = [re.match(r'^  (\w+) .+', l) for l in out]
    modes = tuple(m.group(1) for m in modes if m)
    self.assertEquals(self.isolate.VALID_MODES, modes)
    for mode in modes:
      self.assertTrue(hasattr(self, 'test_%s' % mode), mode)

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

  def test_hashtable(self):
    cmd = [
        '--mode', 'hashtable',
        '--outdir', self.tempdir,
        'isolate_test.py',
    ]
    self._execute(cmd)
    # Calculate our hash.
    h = hashlib.sha1()
    h.update(open(__file__, 'rb').read())
    digest = h.hexdigest()
    self.assertEquals(
      '{"files": {"isolate_test.py": {"sha1": "%s"}}}' % digest,
      open(self.result, 'rb').read())
    self.assertEquals(
        sorted([digest, 'result']), sorted(os.listdir(self.tempdir)))

  def test_remap(self):
    cmd = [
        '--mode', 'remap',
        '--outdir', self.tempdir,
        'isolate_test.py',
    ]
    self._execute(cmd)
    self.assertEquals('isolate_test.py\n', open(self.result, 'rb').read())
    self.assertEquals(
        ['isolate_test.py', 'result'], sorted(os.listdir(self.tempdir)))

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
