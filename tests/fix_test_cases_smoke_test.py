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
    if VERBOSE:
      cmd = cmd + ['--verbose'] * 3
    logging.info(cmd)
    proc = subprocess.Popen(
        [sys.executable, os.path.join(ROOT_DIR, cmd[0])] + cmd[1:],
        cwd=self.srcdir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    out = proc.communicate()[0]
    if VERBOSE:
      print '\n-----'
      print out.strip()
      print '-----\n'
    self.assertEqual(0, proc.returncode)
    return out

  def test_simple(self):
    # Create a directory with nothing in it and progressively add more stuff.
    isolate = os.path.join(self.srcdir, 'gtest_fake_pass.isolate')
    with open(isolate, 'w') as f:
      # Write a minimal .isolate file.
      f.write(
          str(
              {
                'variables': {
                  'command': [
                    'run_test_cases.py', 'gtest_fake_pass.py',
                  ],
                },
              }))
    def _copy(filename):
      shutil.copy(
          os.path.join(BASE_DIR, 'gtest_fake', filename),
          os.path.join(self.srcdir, filename))
    _copy('gtest_fake_base.py')
    _copy('gtest_fake_pass.py')
    shutil.copy(
        os.path.join(ROOT_DIR, 'run_test_cases.py'),
        os.path.join(self.srcdir, 'run_test_cases.py'))
    shutil.copy(
        os.path.join(ROOT_DIR, 'run_isolated.py'),
        os.path.join(self.srcdir, 'run_isolated.py'))

    logging.debug('1. Create a .isolated file out of the .isolate file.')
    isolated = os.path.join(self.srcdir, 'gtest_fake_pass.isolated')
    out = self._run(['isolate.py', 'check', '-i', isolate, '-s', isolated])
    if not VERBOSE:
      self.assertEqual('', out)

    logging.debug('2. Run fix_test_cases.py on it.')
    # Give up on looking at stdout.
    _ = self._run(['fix_test_cases.py', '-s', isolated])

    logging.debug('3. Asserting the content of the .isolated file.')
    with open(isolated) as f:
      actual_isolated = json.load(f)
    gtest_fake_base_py = os.path.join(self.srcdir, 'gtest_fake_base.py')
    gtest_fake_pass_py = os.path.join(self.srcdir, 'gtest_fake_pass.py')
    run_isolated_py = os.path.join(self.srcdir, 'run_isolated.py')
    run_test_cases_py = os.path.join(self.srcdir, 'run_test_cases.py')
    expected_isolated = {
      u'command': [u'run_test_cases.py', u'gtest_fake_pass.py'],
      u'files': {
        u'gtest_fake_base.py': {
          u'm': 416,
          u'h': unicode(hashlib.sha1(
              open(gtest_fake_base_py, 'rb').read()).hexdigest()),
          u's': os.stat(gtest_fake_base_py).st_size,
        },
        u'gtest_fake_pass.py': {
          u'm': 488,
          u'h': unicode(hashlib.sha1(
              open(gtest_fake_pass_py, 'rb').read()).hexdigest()),
          u's': os.stat(gtest_fake_pass_py).st_size,
        },
        u'run_isolated.py': {
          u'm': 488,
          u'h': unicode(hashlib.sha1(
              open(run_isolated_py, 'rb').read()).hexdigest()),
          u's': os.stat(run_isolated_py).st_size,
        },
        u'run_test_cases.py': {
          u'm': 488,
          u'h': unicode(hashlib.sha1(
              open(run_test_cases_py, 'rb').read()).hexdigest()),
          u's': os.stat(run_test_cases_py).st_size,
        },
      },
      u'os': unicode(run_isolated.get_flavor()),
      u'relative_cwd': u'.',
    }
    if sys.platform == 'win32':
      for value in expected_isolated['files'].itervalues():
        self.assertTrue(value.pop('m'))
    self.assertEquals(expected_isolated, actual_isolated)

    # Now verify the .isolate file was updated! (That's the magical part where
    # you say wow!)
    with open(isolate) as f:
      actual = eval(f.read(), {'__builtins__': None}, None)
    expected = {
      'variables': {
        'command': ['run_test_cases.py', 'gtest_fake_pass.py'],
      },
      'conditions': [
        ['OS=="%s"' % run_isolated.get_flavor(), {
          'variables': {
            'isolate_dependency_tracked': [
              'gtest_fake_base.py',
              'gtest_fake_pass.py',
              'run_isolated.py',
              'run_test_cases.py',
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
