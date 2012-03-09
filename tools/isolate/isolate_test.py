#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
VERBOSE = False


class Isolate(unittest.TestCase):
  def setUp(self):
    # The reason is that isolate_test.py --ok is run in a temporary directory
    # without access to isolate.py
    import isolate
    self.isolate = isolate
    self.tempdir = tempfile.mkdtemp()
    self.result = os.path.join(self.tempdir, 'result')
    if VERBOSE:
      print

  def tearDown(self):
    shutil.rmtree(self.tempdir)

  def _expected_tree(self, files):
    self.assertEquals(sorted(files), sorted(os.listdir(self.tempdir)))

  def _expected_result(self, with_hash, files, args, read_only):
    # 4 modes are supported, 0755 (rwx), 0644 (rw), 0555 (rx), 0444 (r)
    min_mode = 0444
    if not read_only:
      min_mode |= 0200
    def mode(filename):
      return (min_mode | 0111) if filename.endswith('.py') else min_mode
    expected = {
      u'command':
        [unicode(sys.executable), u'isolate_test.py'] +
          [unicode(x) for x in args],
      u'files': dict((unicode(f), {u'mode': mode(f)}) for f in files),
    }
    if with_hash:
      for filename in expected[u'files']:
        # Calculate our hash.
        h = hashlib.sha1()
        h.update(open(os.path.join(ROOT_DIR, filename), 'rb').read())
        expected[u'files'][filename][u'sha-1'] = h.hexdigest()

    actual = json.load(open(self.result, 'rb'))
    self.assertEquals(expected, actual)
    return expected

  def _execute(self, args):
    cmd = [
        sys.executable, os.path.join(ROOT_DIR, 'isolate.py'),
        '--root', ROOT_DIR,
        '--result', self.result,
    ]
    if VERBOSE:
      cmd.extend(['-v'] * 3)
      stdout = None
      stderr = None
    else:
      stdout = subprocess.PIPE
      stderr = subprocess.STDOUT
    subprocess.check_call(
        cmd + args, stdout=stdout, stderr=stderr, cwd=ROOT_DIR)

  def test_help_modes(self):
    # Check coherency in the help and implemented modes.
    p = subprocess.Popen(
        [sys.executable, os.path.join(ROOT_DIR, 'isolate.py'), '--help'],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=ROOT_DIR)
    out = p.communicate()[0].splitlines()
    self.assertEquals(0, p.returncode)
    out = out[out.index('') + 1:]
    out = out[:out.index('')]
    modes = [re.match(r'^  (\w+) .+', l) for l in out]
    modes = tuple(m.group(1) for m in modes if m)
    self.assertEquals(self.isolate.VALID_MODES, modes)
    for mode in modes:
      self.assertTrue(hasattr(self, 'test_%s' % mode), mode)
    self._expected_tree([])

  def test_check(self):
    cmd = [
        '--mode', 'check',
        'isolate_test.py',
    ]
    self._execute(cmd)
    self._expected_tree(['result'])
    self._expected_result(False, ['isolate_test.py'], [], False)

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
    self._expected_tree([])

  def test_check_directory_no_slash(self):
    cmd = [
        '--mode', 'check',
        # Trailing slash missing.
        'data',
    ]
    try:
      self._execute(cmd)
      self.fail()
    except subprocess.CalledProcessError:
      pass
    self._expected_tree([])

  def test_hashtable(self):
    cmd = [
        '--mode', 'hashtable',
        '--outdir', self.tempdir,
        'isolate_test.py',
        'data/',
    ]
    self._execute(cmd)
    files = ['isolate_test.py', 'data/test_file1.txt', 'data/test_file2.txt']
    data = self._expected_result(True, files, [], False)
    self._expected_tree(
        [f['sha-1'] for f in data['files'].itervalues()] + ['result'])

  def test_remap(self):
    cmd = [
        '--mode', 'remap',
        '--outdir', self.tempdir,
        'isolate_test.py',
    ]
    self._execute(cmd)
    self._expected_tree(['isolate_test.py', 'result'])
    self._expected_result(False, ['isolate_test.py'], [], False)

  def test_run(self):
    cmd = [
        '--mode', 'run',
        'isolate_test.py',
        '--',
        sys.executable, 'isolate_test.py', '--ok',
    ]
    self._execute(cmd)
    self._expected_tree(['result'])
    self._expected_result(False, ['isolate_test.py'], ['--ok'], False)

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
    self._expected_tree([])


def main():
  global VERBOSE
  VERBOSE = '-v' in sys.argv
  level = logging.DEBUG if VERBOSE else logging.ERROR
  logging.basicConfig(level=level)
  if len(sys.argv) == 1:
    unittest.main()
  if sys.argv[1] == '--ok':
    return 0
  if sys.argv[1] == '--fail':
    return 1

  unittest.main()


if __name__ == '__main__':
  sys.exit(main())
