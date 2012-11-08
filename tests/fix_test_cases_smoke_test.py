#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(BASE_DIR)
sys.path.insert(0, ROOT_DIR)

import run_isolated


class FixTestCases(unittest.TestCase):
  def setUp(self):
    self.tempdir = tempfile.mkdtemp(prefix='fix_test_case')
    self.srcdir = os.path.join(self.tempdir, 'srcdir')
    os.mkdir(self.srcdir)

  def tearDown(self):
    if self.tempdir:
      if VERBOSE:
        # If -v is used, this means the user wants to do further analisys on
        # the data.
        print('Leaking %s' % self.tempdir)
      else:
        shutil.rmtree(self.tempdir)

  def _run(self, cmd):
    logging.info(cmd)
    proc = subprocess.Popen(
        [sys.executable, os.path.join(ROOT_DIR, cmd[0])] + cmd[1:],
        cwd=self.srcdir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    out = proc.communicate()[0]
    self.assertEqual(0, proc.returncode, out)
    return out

  def test_simple(self):
    # Create a directory with nothing in it and progressively add more stuff.
    isolate = os.path.join(self.srcdir, 'gtest_fake_pass.isolate')
    with open(isolate, 'w') as f:
      # Write a minimal .isolate file.
      f.write('{\'variables\':{\'command\': [\'gtest_fake_pass.py\']}}')
    def _copy(filename):
      shutil.copy(
          os.path.join(BASE_DIR, 'gtest_fake', filename),
          os.path.join(self.srcdir, filename))
    _copy('gtest_fake_base.py')
    _copy('gtest_fake_pass.py')

    # Create a .isolated file out of it.
    isolated = os.path.join(self.srcdir, 'gtest_fake_pass.isolated')
    out = self._run(['isolate.py', 'check', '-i', isolate, '-s', isolated])
    self.assertEqual('', out)

    out = self._run(['fix_test_cases.py', '-s', isolated])
    expected = (
        'Updating %s\nDidn\'t find any test case to run\n' %
            os.path.join(self.srcdir, 'gtest_fake_pass.isolate'))
    self.assertEqual(expected, out)

    # Assert the content of the .isolated file.
    with open(isolated) as f:
      actual_isolated = json.load(f)
    gtest_fake_base_py = os.path.join(self.srcdir, 'gtest_fake_base.py')
    gtest_fake_pass_py = os.path.join(self.srcdir, 'gtest_fake_pass.py')
    expected_isolated = {
      u'command': [u'gtest_fake_pass.py'],
      u'files': {
        u'gtest_fake_base.py': {
          u'mode': 416,
          u'sha-1': unicode(hashlib.sha1(
              open(gtest_fake_base_py, 'rb').read()).hexdigest()),
          u'size': os.stat(gtest_fake_base_py).st_size,
        },
        u'gtest_fake_pass.py': {
          u'mode': 488,
          u'sha-1': unicode(hashlib.sha1(
              open(gtest_fake_pass_py, 'rb').read()).hexdigest()),
          u'size': os.stat(gtest_fake_pass_py).st_size,
        },
      },
      u'os': unicode(run_isolated.get_flavor()),
      u'relative_cwd': u'.',
    }
    self.assertTrue(
        actual_isolated['files']['gtest_fake_base.py'].pop('timestamp'))
    self.assertTrue(
        actual_isolated['files']['gtest_fake_pass.py'].pop('timestamp'))
    if sys.platform == 'win32':
      expected_isolated['files']['gtest_fake_base.py'].pop('mode')
      expected_isolated['files']['gtest_fake_pass.py'].pop('mode')
    self.assertEquals(expected_isolated, actual_isolated)

    # Now verify the .isolate file was updated! (That's the magical part where
    # you say wow!)
    with open(isolate) as f:
      actual = eval(f.read(), {'__builtins__': None}, None)
    expected = {
      'variables': {
        'command': ['gtest_fake_pass.py'],
      },
      'conditions': [
        ['OS=="%s"' % run_isolated.get_flavor(), {
          'variables': {
            'isolate_dependency_tracked': [
              'gtest_fake_base.py',
              'gtest_fake_pass.py',
            ],
          },
        }],
      ],
    }
    self.assertEqual(expected, actual)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
