#!/usr/bin/env python
# Copyright 2015 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import logging
import os
import sys
import tempfile
import shutil
import unittest
import re

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT_DIR)

from utils import logging_utils

import test_env
test_env.setup_test_env()

from depot_tools import auto_stub

_LOG_HEADER = r'^%s \d\d\d\d-\d\d-\d\d \d\d:\d\d:\d\d\.\d\d\d: ' % os.getpid()


class TestLoggingUtils(auto_stub.TestCase):
  def test_Capture(self):
    root = logging.RootLogger(logging.DEBUG)
    with logging_utils.CaptureLogs('foo', root) as log:
      root.debug('foo')
      result = log.read()
    self.assertTrue(re.match(_LOG_HEADER + 'DEBUG foo\n$', result), result)

  def test_prepare_logging(self):
    root = logging.RootLogger(logging.DEBUG)
    tmp_dir = tempfile.mkdtemp(prefix='logging_utils_test')
    try:
      filepath = os.path.join(tmp_dir, 'test.log')
      logging_utils.prepare_logging(filepath, root)
      root.debug('foo')
      with open(filepath, 'rb') as f:
        result = f.read()
    finally:
      shutil.rmtree(tmp_dir)
    # It'd be nice to figure out a way to ensure it's properly in UTC but it's
    # tricky to do reliably.
    self.assertTrue(re.match(_LOG_HEADER + 'DEBUG foo\n$', result), result)


if __name__ == '__main__':
  unittest.main()
