#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Scripts to run a test, grab the failures and trace them."""

import json
import os
import subprocess
import sys
import tempfile

import run_test_cases


if sys.platform == 'win32':
  import msvcrt  # pylint: disable=F0401

  def get_keyboard():
    """Returns a letter from the keyboard if any.

    This function returns immediately.
    """
    if msvcrt.kbhit():
      return ord(msvcrt.getch())

else:
  import select

  def get_keyboard():
    """Returns a letter from the keyboard if any, as soon as he pressed enter.

    This function returns (almost) immediately.

    The library doesn't give a way to just get the initial letter.
    """
    if select.select([sys.stdin], [], [], 0.00001)[0]:
      return sys.stdin.read(1)


def trace_and_merge(result, test):
  """Traces a single test case and merges the result back into .isolate."""
  subprocess.call(
      [
        sys.executable, 'isolate.py', 'trace', '-r', result,
        '--', '--gtest_filter=' + test,
      ])
  return not subprocess.call(
      [sys.executable, 'isolate.py', 'merge', '-r', result])


def run_all(result):
  """Runs all the tests. Returns the tests that failed or None on failure.

  Assumes run_test_cases.py is implicitly called.
  """
  handle, result_file = tempfile.mkstemp(prefix='run_test_cases')
  os.close(handle)
  env = os.environ.copy()
  env['RUN_TEST_CASES_RESULT_FILE'] = result_file
  subprocess.call(
      [sys.executable, 'isolate.py', 'run', '-r', result], env=env)
  if not os.path.isfile(result_file):
    print >> sys.stderr, 'Failed to find %s' % result_file
    return None
  with open(result_file) as f:
    data = json.load(f)
  os.remove(result_file)
  return [
    test for test, runs in data.iteritems()
    if not any(not run['returncode'] for run in runs)
  ]


def run(result, test):
  """Runs a single test case in an isolated environment.

  Returns True if the test passed.
  """
  return not subprocess.call([
    sys.executable, 'isolate.py', 'run', '-r', result,
    '--', '--gtest_filter=' + test,
  ])


def diff_and_commit(test):
  """Prints the diff and commit."""
  subprocess.call(['git', 'diff'])
  subprocess.call(['git', 'commit', '-a', '-m', test])


def trace_and_verify(result, test):
  """Traces a test case, updates .isolate and makes sure it passes afterward.

  Return None if the test was already passing,  True on success.
  """
  trace_and_merge(result, test)
  diff_and_commit(test)
  return run(result, test)


def fix_all(result):
  """Runs all the test cases in a gtest executable and trace the failing tests.

  Returns True on success.

  Makes sure the test passes afterward.
  """
  # These could have adverse side-effects.
  # TODO(maruel): Be more intelligent about it, for now be safe.
  for i in run_test_cases.KNOWN_GTEST_ENV_VARS:
    if i in os.environ:
      print >> 'Please unset %s' % i
      return False

  test_cases = run_all(result)
  if test_cases is None:
    return False

  print '\nFound %d broken test cases.' % len(test_cases)
  if not test_cases:
    return True

  failures = []
  fixed_tests = []
  try:
    for index, test_case in enumerate(test_cases):
      if get_keyboard():
        # Return early.
        return True

      try:
        if run(result, test_case):
          continue

        if not trace_and_verify(result, test_case):
          failures.append(test_case)
          print 'Failed to fix %s' % test_case
        else:
          fixed_tests.append(test_case)
      except:  # pylint: disable=W0702
        failures.append(test_case)
        print 'Failed to fix %s' % test_case
      print '%d/%d' % (index+1, len(test_cases))
  finally:
    print 'Test cases fixed (%d):' % len(fixed_tests)
    for fixed_test in fixed_tests:
      print '  %s' % fixed_test
    print ''

    print 'Test cases still failing (%d):' % len(failures)
    for failure in failures:
      print '  %s' % failure
  return not failures


def main():
  if len(sys.argv) != 2:
    print >> sys.stderr, 'Use with the name of the test only, e.g. "unit_tests"'
    return 1

  basename = sys.argv[1]
  executable = '../../out/Release/%s' % basename
  result = '%s.results' % executable
  if sys.platform == 'win32':
    executable += '.exe'
  if not os.path.isfile(executable):
    print >> sys.stderr, (
        '%s doesn\'t exist, please build %s_run' % (executable, basename))
    return 1
  if not os.path.isfile(result):
    print >> sys.stderr, (
        '%s doesn\'t exist, please build %s_run' % (result, basename))
    return 1

  return not fix_all(result)


if __name__ == '__main__':
  sys.exit(main())
