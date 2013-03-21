#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simulate a failing google-test executable.

http://code.google.com/p/googletest/
"""

import optparse
import sys

import gtest_fake_base


TESTS = {
  'Foo': ['Bar1', 'Bar2', 'Bar3'],
  'Baz': ['Fail'],
}
TOTAL = sum(len(v) for v in TESTS.itervalues())


def main():
  parser = optparse.OptionParser()
  parser.add_option('--gtest_list_tests', action='store_true')
  parser.add_option('--gtest_print_time', action='store_true')
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
    should_fail = options.gtest_filter == 'Baz.Fail'
    print gtest_fake_base.get_test_output(options.gtest_filter, should_fail)
    print gtest_fake_base.get_footer(1, 1)
    # Make Baz.Fail fail.
    return int(should_fail)

  for fixture, cases in TESTS.iteritems():
    for case in cases:
      print gtest_fake_base.get_test_output('%s.%s' % (fixture, case), False)
  print gtest_fake_base.get_footer(TOTAL, TOTAL)
  return 1


if __name__ == '__main__':
  sys.exit(main())
