#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""List all the test cases for a google test.

See more info at http://code.google.com/p/googletest/.
"""

import optparse
import subprocess
import sys


class Failure(Exception):
  pass


def fix_python_path(cmd):
  """Returns the fixed command line to call the right python executable."""
  out = cmd[:]
  if out[0] == 'python':
    out[0] = sys.executable
  elif out[0].endswith('.py'):
    out.insert(0, sys.executable)
  return out


def gtest_list_tests(executable):
  cmd = [executable, '--gtest_list_tests']
  cmd = fix_python_path(cmd)
  try:
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  except OSError, e:
    raise Failure('Failed to run %s\n%s' % (executable, str(e)))
  out, err = p.communicate()
  if p.returncode:
    raise Failure('Failed to run %s\n%s' % (executable, err), p.returncode)
  # pylint: disable=E1103
  if err and not err.startswith('Xlib:  extension "RANDR" missing on display '):
    raise Failure('Unexpected spew:\n%s' % err, 1)
  return out


def filter_shards(tests, index, shards):
  """Filters the shards.

  Watch out about integer based arithmetics.
  """
  # The following code could be made more terse but I liked the extra clarity.
  assert 0 <= index < shards
  total = len(tests)
  quotient, remainder = divmod(total, shards)
  # 1 item of each remainder is distributed over the first 0:remainder shards.
  # For example, with total == 5, index == 1, shards == 3
  # min_bound == 2, max_bound == 4.
  min_bound = quotient * index + min(index, remainder)
  max_bound = quotient * (index + 1) + min(index + 1, remainder)
  return tests[min_bound:max_bound]


def _starts_with(a, b, prefix):
  return a.startswith(prefix) or b.startswith(prefix)


def filter_bad_tests(tests, disabled=False, fails=False, flaky=False):
  out = []
  for test in tests:
    fixture, case = test.split('.', 1)
    if not disabled and _starts_with(fixture, case, 'DISABLED_'):
      continue
    if not fails and _starts_with(fixture, case, 'FAILS_'):
      continue
    if not flaky and _starts_with(fixture, case, 'FLAKY_'):
      continue
    out.append(test)
  return out


def parse_gtest_cases(out):
  """Expected format is a concatenation of this:
  TestFixture1
     TestCase1
     TestCase2
  """
  tests = []
  fixture = None
  lines = out.splitlines()
  while lines:
    line = lines.pop(0)
    if not line:
      break
    if not line.startswith('  '):
      fixture = line
    else:
      case = line[2:]
      if case.startswith('YOU HAVE'):
        # It's a 'YOU HAVE foo bar' line. We're done.
        break
      assert ' ' not in case
      tests.append(fixture + case)
  return tests


def list_test_cases(executable, index, shards, disabled, fails, flaky):
  """Retuns the list of test cases according to the specified criterias."""
  tests = parse_gtest_cases(gtest_list_tests(executable))
  if shards:
    tests = filter_shards(tests, index, shards)
  return filter_bad_tests(tests, disabled, fails, flaky)


def main():
  """CLI frontend to validate arguments."""
  parser = optparse.OptionParser(
      usage='%prog <options> [gtest]')
  parser.add_option(
      '-d', '--disabled',
      action='store_true',
      help='Include DISABLED_ tests')
  parser.add_option(
      '-f', '--fails',
      action='store_true',
      help='Include FAILS_ tests')
  parser.add_option(
      '-F', '--flaky',
      action='store_true',
      help='Include FLAKY_ tests')
  parser.add_option(
      '-i', '--index',
      type='int',
      help='Shard index to run')
  parser.add_option(
      '-s', '--shards',
      type='int',
      help='Total number of shards to calculate from the --index to run')
  options, args = parser.parse_args()
  if len(args) != 1:
    parser.error('Please provide the executable to run')

  if bool(options.shards) != bool(options.index is not None):
    parser.error('Use both --index X --shards Y or none of them')

  try:
    tests = list_test_cases(
        args[0],
        options.index,
        options.shards,
        options.disabled,
        options.fails,
        options.flaky)
    for test in tests:
      print test
  except Failure, e:
    print e.args[0]
    return e.args[1]
  return 0


if __name__ == '__main__':
  sys.exit(main())
