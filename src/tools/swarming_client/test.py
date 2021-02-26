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
  sys.path.insert(0, COMPONENTS_DIR)

  return run_tests_parralel() or run_tests_sequential()


def run_tests_parralel():
  sys.path.insert(0, TESTS_DIR)
  import test_env
  test_env.setup()

  # Need to specify config path explicitly
  # because test_env.setup() changes directory
  cfg = os.path.join(THIS_DIR, 'unittest.cfg')
  sys.argv.extend(['-c', cfg])

  # enable plugins only on linux
  plugins = []
  if sys.platform.startswith('linux'):
    plugins.append('nose2.plugins.mp')

  # append attribute filter option "--attribute '!no_run'"
  # https://nose2.readthedocs.io/en/latest/plugins/attrib.html
  from test_support import parallel_test_runner
  sys.argv.extend(['--attribute', '!no_run'])

  # execute test runner
  return parallel_test_runner.run_tests(python3=six.PY3, plugins=plugins)


def run_tests_sequential():
  # These tests need to be run as executable
  # because they don't pass when running in parallel
  # or run via test runner
  abs_path = lambda f: os.path.join(THIS_DIR, f)
  test_cmds = [
      [abs_path('tests/swarming_test.py')],
      [abs_path('tests/run_isolated_test.py')],
      [abs_path('tests/isolateserver_test.py')],
      [abs_path('tests/logging_utils_test.py')],
  ]

  # execute test runner
  from test_support import sequential_test_runner
  return sequential_test_runner.run_tests(test_cmds, python3=six.PY3)


if __name__ == '__main__':
  sys.exit(main())
