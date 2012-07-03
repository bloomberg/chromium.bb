#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simulate a google-test executable.

http://code.google.com/p/googletest/
"""

import optparse
import sys


TESTS = {
  'Foo': ['Bar1', 'Bar2', 'Bar3'],
  'Baz': ['Fail'],
}
TOTAL = sum(len(v) for v in TESTS.itervalues())


def get_test_output(test_name):
  fixture, case = test_name.split('.', 1)
  return (
    '[==========] Running 1 test from 1 test case.\n'
    '[----------] Global test environment set-up.\n'
    '[----------] 1 test from %(fixture)s\n'
    '[ RUN      ] %(fixture)s.%(case)s\n'
    '[       OK ] %(fixture)s.%(case)s (0 ms)\n'
    '[----------] 1 test from %(fixture)s (0 ms total)\n'
    '\n') % {
      'fixture': fixture,
      'case': case,
    }


def get_footer(number):
  return (
    '[----------] Global test environment tear-down\n'
    '[==========] %(number)d test from %(total)d test case ran. (0 ms total)\n'
    '[  PASSED  ] %(number)d test.\n'
    '\n'
    '  YOU HAVE 5 DISABLED TESTS\n'
    '\n'
    '  YOU HAVE 2 tests with ignored failures (FAILS prefix)\n') % {
      'number': number,
      'total': TOTAL,
    }


def main():
  parser = optparse.OptionParser()
  parser.add_option('--gtest_list_tests', action='store_true')
  parser.add_option('--gtest_filter')
  options, args = parser.parse_args()
  if args:
    parser.error('Failed to process args %s' % args)

  if options.gtest_list_tests:
    for fixture, cases in TESTS.iteritems():
      print '%s.' % fixture
      for case in cases:
        print '  ' + case
    print '  YOU HAVE 2 tests with ignored failures (FAILS prefix)'
    print ''
    return 0

  if options.gtest_filter:
    # Simulate running one test.
    print 'Note: Google Test filter = %s\n' % options.gtest_filter
    print get_test_output(options.gtest_filter)
    print get_footer(1)
    # Make Baz.Fail fail.
    return options.gtest_filter == 'Baz.Fail'

  for fixture, cases in TESTS.iteritems():
    for case in cases:
      print get_test_output('%s.%s' % (fixture, case))
  print get_footer(TOTAL)
  return 6


if __name__ == '__main__':
  sys.exit(main())
