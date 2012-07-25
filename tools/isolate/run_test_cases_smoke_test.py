#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import subprocess
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))

sys.path.append(os.path.join(ROOT_DIR, 'data', 'gtest_fake'))
import gtest_fake_base


def RunTest(test_file):
  target = os.path.join(ROOT_DIR, 'data', 'gtest_fake', test_file)
  cmd = [
      sys.executable,
      os.path.join(ROOT_DIR, 'run_test_cases.py'),
      '--no-dump',
      target,
  ]
  logging.debug(' '.join(cmd))
  proc = subprocess.Popen(
      cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    # pylint is confused.
  out, err = proc.communicate() or ('', '')

  return (out, err, proc.returncode)


class TraceTestCases(unittest.TestCase):
  def setUp(self):
    # Make sure there's no environment variable that could do side effects.
    os.environ.pop('GTEST_SHARD_INDEX', '')
    os.environ.pop('GTEST_TOTAL_SHARDS', '')

  def _check_results(self, expected_out_re, out, err):
    if sys.platform == 'win32':
      out = out.replace('\r\n', '\n')
    lines = out.splitlines()

    for index in range(len(expected_out_re)):
      line = lines.pop(0)
      self.assertTrue(
          re.match('^%s$' % expected_out_re[index], line),
          (index, expected_out_re[index], repr(line)))
    self.assertEquals([], lines)
    self.assertEquals('', err)

  def test_simple_pass(self):
    out, err, return_code = RunTest('gtest_fake_pass.py')

    self.assertEquals(0, return_code)

    expected_out_re = [
      r'\[\d/\d\]   \d\.\d\ds .+',
      r'\[\d/\d\]   \d\.\d\ds .+',
      r'\[\d/\d\]   \d\.\d\ds .+',
      re.escape('Success:    3 100.00%'),
      re.escape('Flaky:      0  0.00%'),
      re.escape('Fail:       0  0.00%'),
      r'\d+\.\ds Done running 3 tests with 3 executions. \d+\.\d test/s',
    ]

    self._check_results(expected_out_re, out, err)

  def test_simple_fail(self):
    out, err, return_code = RunTest('gtest_fake_fail.py')

    self.assertEquals(1, return_code)

    expected_out_re = [
      r'\[\d/\d\]   \d\.\d\ds .+',
      r'\[\d/\d\]   \d\.\d\ds .+',
      r'\[\d/\d\]   \d\.\d\ds .+',
      r'\[\d/\d\]   \d\.\d\ds .+',
      r'\[\d/\d\]   \d\.\d\ds .+',
      r'\[\d/\d\]   \d\.\d\ds .+',
      re.escape('Note: Google Test filter = Baz.Fail'),
      r'',
    ] + [
        re.escape(l) for l in
        gtest_fake_base.get_test_output('Baz.Fail').splitlines()
    ] + [
      '',
    ] + [
      re.escape(l) for l in gtest_fake_base.get_footer(1, 1).splitlines()
    ] + [
      '',
      re.escape('Success:    3 75.00%'),
      re.escape('Flaky:      0  0.00%'),
      re.escape('Fail:       1 25.00%'),
      r'\d+\.\ds Done running 4 tests with 6 executions. \d+\.\d test/s',
    ]
    self._check_results(expected_out_re, out, err)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
