#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs a test, grab the failures and trace them."""

import json
import os
import subprocess
import sys
import tempfile

import run_test_cases


XVFB_PATH = os.path.join('..', '..', 'testing', 'xvfb.py')


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
  env = os.environ.copy()
  env['RUN_TEST_CASES_RUN_ALL'] = '1'

  print 'Starting trace of %s' % test
  subprocess.call(
      [
        sys.executable, 'isolate.py', 'trace', '-r', result,
        '--', '--gtest_filter=' + test,
      ],
      env=env)

  print 'Starting merge of %s' % test
  return not subprocess.call(
      [sys.executable, 'isolate.py', 'merge', '-r', result])


def run_all(result, shard_index, shard_count):
  """Runs all the tests. Returns the tests that failed or None on failure.

  Assumes run_test_cases.py is implicitly called.
  """
  handle, result_file = tempfile.mkstemp(prefix='run_test_cases')
  os.close(handle)
  env = os.environ.copy()
  env['RUN_TEST_CASES_RESULT_FILE'] = result_file
  env['RUN_TEST_CASES_RUN_ALL'] = '1'
  env['GTEST_SHARD_INDEX'] = str(shard_index)
  env['GTEST_TOTAL_SHARDS'] = str(shard_count)
  cmd = [sys.executable, 'isolate.py', 'run', '-r', result]
  subprocess.call(cmd, env=env)
  if not os.path.isfile(result_file):
    print >> sys.stderr, 'Failed to find %s' % result_file
    return None
  with open(result_file) as f:
    try:
      data = json.load(f)
    except ValueError as e:
      print >> sys.stderr, ('Unable to load json file, %s: %s' %
                            (result_file, str(e)))
      return None
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


def run_normally(executable, test):
  return not subprocess.call([
    sys.executable, XVFB_PATH, os.path.dirname(executable), executable,
    '--gtest_filter=' + test])


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
  print 'Verifying trace...'
  return run(result, test)


def fix_all(result, shard_index, shard_count, executable):
  """Runs all the test cases in a gtest executable and trace the failing tests.

  Returns True on success.

  Makes sure the test passes afterward.
  """
  # These could have adverse side-effects.
  # TODO(maruel): Be more intelligent about it, for now be safe.
  run_test_cases_env = ['RUN_TEST_CASES_RESULT_FILE', 'RUN_TEST_CASES_RUN_ALL']
  for i in run_test_cases.KNOWN_GTEST_ENV_VARS + run_test_cases_env:
    if i in os.environ:
      print >> 'Please unset %s' % i
      return False

  test_cases = run_all(result, shard_index, shard_count)
  if test_cases is None:
    return False

  print '\nFound %d broken test cases.' % len(test_cases)
  if not test_cases:
    return True

  failed_alone = []
  failures = []
  fixed_tests = []
  try:
    for index, test_case in enumerate(test_cases):
      if get_keyboard():
        # Return early.
        return True

      try:
        # Check if the test passes normally, because otherwise there is no
        # reason to trace its failure.
        if not run_normally(executable, test_case):
          print '%s is broken when run alone, please fix the test.' % test_case
          failed_alone.append(test_case)
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

    if failed_alone:
      print ('Test cases that failed normally when run alone (%d):' %
             len(failed_alone))
      for failed in failed_alone:
        print failed
  return not failures


def main():
  parser = run_test_cases.OptionParserWithTestSharding(
      usage='%prog <option> [test]')
  parser.add_option('-d', '--dir', default='../../out/Release',
                    help='The directory containing the the test executable and '
                    'result file. Defaults to %default')
  options, args = parser.parse_args()

  if len(args) != 1:
    parser.error('Use with the name of the test only, e.g. unit_tests')

  basename = args[0]
  executable = os.path.join(options.dir, basename)
  result = '%s.results' % executable
  if sys.platform in('win32', 'cygwin'):
    executable += '.exe'
  if not os.path.isfile(executable):
    print >> sys.stderr, (
        '%s doesn\'t exist, please build %s_run' % (executable, basename))
    return 1
  if not os.path.isfile(result):
    print >> sys.stderr, (
        '%s doesn\'t exist, please build %s_run' % (result, basename))
    return 1

  return not fix_all(result, options.index, options.shards, executable)


if __name__ == '__main__':
  sys.exit(main())
