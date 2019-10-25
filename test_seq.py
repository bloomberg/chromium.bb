#!/usr/bin/env vpython
# Copyright 2019 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import os
import sys

import six

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
LUCI_DIR = os.path.dirname(THIS_DIR)
COMPONENTS_DIR = os.path.join(LUCI_DIR, 'appengine', 'components')


def main():
  # These tests need to be run as executable
  # because they don't pass when running in parallel
  # or run via test runner
  test_files = [
      'tests/swarming_test.py',
      'tests/run_isolated_test.py',
      'tests/isolateserver_test.py',
      'tests/logging_utils_test.py',
  ]
  abs_test_files = [os.path.join(THIS_DIR, t) for t in test_files]

  sys.path.insert(0, COMPONENTS_DIR)
  from test_support import sequential_test_runner

  # execute test runner
  return sequential_test_runner.run_tests(abs_test_files, python3=six.PY3)


if __name__ == '__main__':
  sys.exit(main())
