#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import re
import subprocess
import sys
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)
sys.path.append(os.path.join(ROOT_DIR, 'tests', 'gtest_fake'))

import gtest_fake_base


def RunTest(test_file, extra_flags):
  target = os.path.join(ROOT_DIR, 'tests', 'gtest_fake', test_file)
  cmd = [
      sys.executable,
      os.path.join(ROOT_DIR, 'run_test_cases.py'),
  ] + extra_flags

  cmd.append(target)
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

    self.filename = 'test.results'

  def tearDown(self):
    if os.path.exists(self.filename):
      os.remove(self.filename)

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

  def _check_results_file(self, expected_file_contents_entries):
    self.assertTrue(os.path.exists(self.filename))

    with open(self.filename) as f:
      file_contents = json.load(f)

    self.assertEqual(len(expected_file_contents_entries), len(file_contents))
    for (entry_name, entry_count) in expected_file_contents_entries:
      self.assertTrue(entry_name in file_contents)
      self.assertEqual(entry_count, len(file_contents[entry_name]))

  def test_simple_pass(self):
    out, err, return_code = RunTest(
        'gtest_fake_pass.py', ['--result', self.filename])

    self.assertEquals(0, return_code)

    expected_out_re = [
      r'\[\d/\d\]   \d\.\d\ds .+',
      r'\[\d/\d\]   \d\.\d\ds .+',
      r'\[\d/\d\]   \d\.\d\ds .+',
      re.escape('Summary:'),
      re.escape('Success:    3 100.00%'),
      re.escape('Flaky:      0  0.00%'),
      re.escape('Fail:       0  0.00%'),
      r'\d+\.\ds Done running 3 tests with 3 executions. \d+\.\d test/s',
    ]
    self._check_results(expected_out_re, out, err)

    expected_result_file_entries = [
        ('Foo.Bar1', 1),
        ('Foo.Bar2', 1),
        ('Foo.Bar3', 1)
    ]
    self._check_results_file(expected_result_file_entries)

  def test_simple_fail(self):
    out, err, return_code = RunTest(
        'gtest_fake_fail.py', ['--result', self.filename])

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
      re.escape('Summary:'),
      re.escape('Baz.Fail failed'),
      re.escape('Success:    3 75.00%'),
      re.escape('Flaky:      0  0.00%'),
      re.escape('Fail:       1 25.00%'),
      r'\d+\.\ds Done running 4 tests with 6 executions. \d+\.\d test/s',
    ]
    self._check_results(expected_out_re, out, err)

    expected_result_file_entries = [
        ('Foo.Bar1', 1),
        ('Foo.Bar2', 1),
        ('Foo.Bar3', 1),
        ('Baz.Fail', 3)
    ]
    self._check_results_file(expected_result_file_entries)

  def test_simple_gtest_list_error(self):
    out, err, return_code = RunTest(
        'gtest_fake_error.py', ['--no-dump'])

    expected_out_re = [
        'Failed to run .+gtest_fake_error.py',
        'stdout:',
        '',
        'stderr:',
        'Unable to list tests'
    ]

    self.assertEqual(1, return_code)
    self._check_results(expected_out_re, out, err)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
