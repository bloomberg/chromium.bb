#!/usr/bin/env vpython3
# Copyright 2015 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import logging
import os
import subprocess
import sys
import tempfile
import unittest
import re

# Mutates sys.path.
import test_env

from utils import file_path
from utils import logging_utils


# PID YYYY-MM-DD HH:MM:SS.MMM
_LOG_HEADER = r'^%d \d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d\.\d\d\d' % os.getpid()
_LOG_HEADER_PID = r'^\d+ \d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d\.\d\d\d'


class Test(unittest.TestCase):
  # This test fails when running via test runner
  # Need to run in test_seq.py as an executable
  no_run = 1

  def setUp(self):
    super(Test, self).setUp()
    self.tmp = tempfile.mkdtemp(prefix='logging_utils')

  def tearDown(self):
    try:
      file_path.rmtree(self.tmp)
    finally:
      super(Test, self).tearDown()

  def test_capture(self):
    root = logging.RootLogger(logging.DEBUG)
    with logging_utils.CaptureLogs('foo', root) as log:
      root.debug('foo')
      result = log.read()
    expected = _LOG_HEADER + ': DEBUG foo\n$'
    if sys.platform == 'win32':
      expected = expected.replace('\n', '\r\n')
    self.assertTrue(
        re.match(expected.encode('utf-8'), result), (expected, result))

  def test_prepare_logging(self):
    root = logging.RootLogger(logging.DEBUG)
    filepath = os.path.join(self.tmp, 'test.log')
    logging_utils.prepare_logging(filepath, root)
    root.debug('foo')
    with open(filepath, 'rb') as f:
      result = f.read()
    # It'd be nice to figure out a way to ensure it's properly in UTC but it's
    # tricky to do reliably.
    expected = _LOG_HEADER + ' D: foo\n$'
    self.assertTrue(
        re.match(expected.encode('utf-8'), result), (expected, result))

  def test_rotating(self):
    # Create a rotating log. Create a subprocess then delete the file. Make sure
    # nothing blows up.
    # Everything is done in a child process because the called functions mutate
    # the global state.
    r = subprocess.call(
        [sys.executable, '-u', 'phase1.py', self.tmp],
        cwd=os.path.join(test_env.TESTS_DIR, 'logging_utils'))
    self.assertEqual(0, r)
    self.assertEqual({'shared.1.log'}, set(os.listdir(self.tmp)))
    with open(os.path.join(self.tmp, 'shared.1.log'), 'rb') as f:
      lines = f.read().splitlines()
    expected = [
      r' I: Parent1',
      r' I: Child1',
      r' I: Child2',
      r' I: Parent2',
    ]
    for e, l in zip(expected, lines):
      ex = _LOG_HEADER_PID + e + '$'
      self.assertTrue(re.match(ex.encode('utf-8'), l), (ex, l))
    self.assertEqual(len(expected), len(lines))


if __name__ == '__main__':
  test_env.main()
