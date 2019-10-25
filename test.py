#!/usr/bin/env vpython
# Copyright 2019 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import os
import sys

import six

THIS_DIR = os.path.dirname(os.path.abspath(__file__))
TESTS_DIR = os.path.join(THIS_DIR, 'tests')
LUCI_DIR = os.path.dirname(THIS_DIR)
COMPONENTS_DIR = os.path.join(LUCI_DIR, 'appengine', 'components')

def main():
  sys.path.insert(0, TESTS_DIR)
  import test_env
  test_env.setup()

  sys.path.insert(0, COMPONENTS_DIR)
  from test_support import parallel_test_runner

  # Need to specify config path explicitly
  # because test_env.setup() changes directory
  cfg = os.path.join(COMPONENTS_DIR, 'test_support', 'unittest.cfg')
  sys.argv.extend(['-c', cfg])

  # execute test runner
  return parallel_test_runner.run_tests(python3=six.PY3)


if __name__ == '__main__':
  main()
