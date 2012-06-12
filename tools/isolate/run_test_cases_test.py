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
    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    # pylint is confused.
    out, err = proc.communicate() or ('', '')
    self.assertEquals(0, proc.returncode)
    expected = (
      'Note: Google Test filter = Baz.Fail\n'
      '\n'
      '%(test_output)s\n'
      '%(test_footer)s\n'
      '\n'
      'Success:    3 75.00%%\n'
      'Flaky:      0  0.00%%\n'
      'Fail:       1 25.00%%\n') % {
        'test_output': gtest_fake.get_test_output('Baz.Fail'),
        'test_footer': gtest_fake.get_footer(1),
      }

    self.assertTrue(
        out.startswith(expected),
        '\n'.join(['XXX', expected, 'XXX', out[:len(expected)], 'XXX']))
    remaining_actual = out[len(expected):]
    regexp = (
        r'\d+\.\ds Done running 4 tests with 6 executions. \d+\.\d test/s'
        + '\n')
    self.assertTrue(re.match(regexp, remaining_actual), remaining_actual)
    # Progress junk went to stderr.
    self.assertTrue(err.startswith('\r'), err)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
