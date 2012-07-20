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
import gtest_fake


class TraceTestCases(unittest.TestCase):
  def test_simple(self):
    target = os.path.join(ROOT_DIR, 'data', 'gtest_fake', 'gtest_fake.py')
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
    self.assertEquals(0, proc.returncode)
    if sys.platform == 'win32':
      out = out.replace('\r\n', '\n')
    lines = out.splitlines()
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
      re.escape(l) for l in gtest_fake.get_test_output('Baz.Fail').splitlines()
    ] + [
      '',
    ] + [
      re.escape(l) for l in gtest_fake.get_footer(1).splitlines()
    ] + [
      '',
      re.escape('Success:    3 75.00%'),
      re.escape('Flaky:      0  0.00%'),
      re.escape('Fail:       1 25.00%'),
      r'\d+\.\ds Done running 4 tests with 6 executions. \d+\.\d test/s',
    ]
    for index in range(len(expected_out_re)):
      line = lines.pop(0)
      self.assertTrue(
          re.match('^%s$' % expected_out_re[index], line),
          (index, expected_out_re[index], repr(line)))
    self.assertEquals([], lines)
    self.assertEquals('', err)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
