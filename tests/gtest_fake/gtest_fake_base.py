# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provide common functionality to simulate a google-test executable.

http://code.google.com/p/googletest/
"""

import optparse
import sys


def get_test_output_inner(test_name, failed, duration='100'):
  fixture, case = test_name.split('.', 1)
  return (
    '[ RUN      ] %(fixture)s.%(case)s\n'
    '%(result)s %(fixture)s.%(case)s (%(duration)s ms)\n') % {
      'case': case,
      'duration': duration,
      'fixture': fixture,
      'result': '[  FAILED  ]' if failed else '[       OK ]',
  }

def get_test_output(test_name, failed, duration='100'):
  fixture, _ = test_name.split('.', 1)
  return (
    '[==========] Running 1 test from 1 test case.\n'
    '[----------] Global test environment set-up.\n'
    '%(content)s'
    '[----------] 1 test from %(fixture)s\n'
    '[----------] 1 test from %(fixture)s (%(duration)s ms total)\n'
    '\n') % {
      'content': get_test_output_inner(test_name, failed, duration),
      'duration': duration,
      'fixture': fixture,
    }


def get_footer(number, total):
  return (
    '[----------] Global test environment tear-down\n'
    '[==========] %(number)d test from %(total)d test case ran. (30 ms total)\n'
    '[  PASSED  ] %(number)d test.\n'
    '\n'
    '  YOU HAVE 5 DISABLED TESTS\n'
    '\n'
    '  YOU HAVE 2 tests with ignored failures (FAILS prefix)\n') % {
      'number': number,
      'total': total,
    }


def parse_args(all_tests, need_arg):
  """Creates a generic google-test like python script."""
  parser = optparse.OptionParser()
  parser.add_option('--gtest_list_tests', action='store_true')
  parser.add_option('--gtest_print_time', action='store_true')
  parser.add_option('--gtest_filter')
  options, args = parser.parse_args()
  if need_arg != len(args):
    parser.error(
        'Expected %d arguments, got %d; %s' % (
          need_arg, len(args), ' '.join(args)))

  if options.gtest_list_tests:
    for fixture, cases in all_tests.iteritems():
      print '%s.' % fixture
      for case in cases:
        print '  ' + case
    print '  YOU HAVE 2 tests with ignored failures (FAILS prefix)'
    print ''
    sys.exit(0)

  if options.gtest_filter:
    print 'Note: Google Test filter = %s\n' % options.gtest_filter
    test_cases = options.gtest_filter.split(':')
  else:
    test_cases = sum((
        ['%s.%s' % (f, c) for c in all_tests[f]] for f in all_tests), [])
  return sorted(test_cases), args
