#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT_DIR = os.path.dirname(BASE_DIR)
sys.path.insert(0, ROOT_DIR)

import isolate

TARGET_UTIL_PATH = os.path.join(BASE_DIR, 'gtest_fake', 'gtest_fake_base.py')
TARGET_PATH = os.path.join(BASE_DIR, 'gtest_fake', 'gtest_fake_fail.py')


class IsolateTestCases(unittest.TestCase):
  def setUp(self):
    self.tempdir = None

    self.initial_cwd = ROOT_DIR
    if sys.platform == 'win32':
      # Windows has no kernel mode concept of current working directory.
      self.initial_cwd = None

    # There's 2 kinds of references to python, self.executable,
    # self.real_executable. It depends how python was started and on which OS.
    self.executable = unicode(sys.executable)
    if sys.platform == 'darwin':
      # /usr/bin/python is a thunk executable that decides which version of
      # python gets executed.
      suffix = '.'.join(map(str, sys.version_info[0:2]))
      if os.access(self.executable + suffix, os.X_OK):
        # So it'll look like /usr/bin/python2.7
        self.executable += suffix

    self.real_executable = isolate.trace_inputs.get_native_path_case(
        self.executable)
    # Make sure there's no environment variable that could cause side effects.
    os.environ.pop('GTEST_SHARD_INDEX', '')
    os.environ.pop('GTEST_TOTAL_SHARDS', '')

  def tearDown(self):
    if self.tempdir:
      shutil.rmtree(self.tempdir)

  def test_simple(self):
    # Create a directory and re-use tests/gtest_fake/gtest_fake_pass.isolate.
    self.tempdir = tempfile.mkdtemp(prefix='isolate_test_cases_test')
    basename = os.path.join(self.tempdir, 'gtest_fake_pass')
    isolated = basename + '.isolated'

    # Create a proper .isolated file.
    cmd = [
      sys.executable, 'isolate.py',
      'check',
      '-V', 'FLAG', 'run',
      '-i', os.path.join('tests', 'gtest_fake', 'gtest_fake_pass.isolate'),
      '-r', isolated,
    ]
    if VERBOSE:
      cmd.extend(['-v'] * 3)
    subprocess.check_call(cmd, cwd=ROOT_DIR)

    cmd = [
        sys.executable,
        os.path.join(ROOT_DIR, 'isolate_test_cases.py'),
        # Forces 4 parallel jobs.
        '--jobs', '4',
        '--timeout', '0',
        '--result', isolated,
    ]
    if VERBOSE:
      cmd.extend(['-v'] * 3)
    logging.debug(' '.join(cmd))
    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = proc.communicate() or ('', '')  # pylint is confused.
    self.assertEqual(0, proc.returncode, (out, err))
    lines = out.splitlines()
    expected_out_re = [
      r'\[1/3\]   \d\.\d\ds .+',
      r'\[2/3\]   \d\.\d\ds .+',
      r'\[3/3\]   \d\.\d\ds .+',
      #r'\d+\.\ds Done post-processing logs\. Parsing logs\.',
      #r'\d+\.\ds Done parsing logs\.',
      #r'\d+\.\ds Done stripping root\.',
      #r'\d+\.\ds Done flattening\.',
    ]
    self.assertEqual(len(expected_out_re), len(lines), (out, err))
    for index in range(len(expected_out_re)):
      self.assertTrue(
          re.match('^%s$' % expected_out_re[index], lines[index]),
          '%d\n%r\n%r\n%r' % (
            index, expected_out_re[index], lines[index], out))
    # Junk is printed on win32.
    if sys.platform != 'win32' and not VERBOSE:
      self.assertEqual('', err)

    test_cases = (
      'Foo.Bar1',
      'Foo.Bar2',
      'Foo.Bar3',
    )
    expected = {
      'conditions': [
        ['OS=="linux"', {
          'variables': {
            'isolate_dependency_tracked': [
              'gtest_fake_base.py',
              'gtest_fake_pass.py',
            ],
          },
        }],
      ],
    }
    for test_case in test_cases:
      with open(basename + '.' + test_case + '.isolate', 'r') as f:
        result = eval(f.read(), {'__builtins__': None}, None)
        self.assertEqual(expected, result)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
