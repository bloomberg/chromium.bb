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


def gtest_list_tests(executable):
  cmd = [executable, '--gtest_list_tests']
  p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  out, err = p.communicate()
  if p.returncode:
    raise Failure('Failed to run %s\n%s' % (executable, err), p.returncode)
  # pylint: disable=E1103
  if err and not err.startswith('Xlib:  extension "RANDR" missing on display '):
    raise Failure('Unexpected spew:\n%s' % err, 1)
  return out


def _starts_with(a, b, prefix):
  return a.startswith(prefix) or b.startswith(prefix)


def parse_gtest_cases(out, disabled=False, fails=False, flaky=False):
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

      if not disabled and _starts_with(fixture, case, 'DISABLED_'):
        continue
      if not fails and _starts_with(fixture, case, 'FAILS_'):
        continue
      if not flaky and _starts_with(fixture, case, 'FLAKY_'):
        continue

      tests.append(fixture + case)
  return tests


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
  options, args = parser.parse_args()
  if len(args) != 1:
    parser.error('Please provide the executable to run')

  try:
    out = gtest_list_tests(args[0])
    tests = parse_gtest_cases(
        out, options.disabled, options.fails, options.flaky)
    for test in tests:
      print test
  except Failure, e:
    print e.args[0]
    return e.args[1]
  return 0


if __name__ == '__main__':
  sys.exit(main())
