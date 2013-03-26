#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simulate a slowly passing google-test executable.

http://code.google.com/p/googletest/
"""

import optparse
import sys
import time

import gtest_fake_base


TESTS = {
  'Foo': sorted(
    [
      'Bar1',
      'Bar2',
      'Bar/3',
    ]),
}


def main():
  parser = optparse.OptionParser()
  parser.add_option('--gtest_list_tests', action='store_true')
  parser.add_option('--gtest_print_time', action='store_true')
  parser.add_option('--gtest_filter')
  options, args = parser.parse_args()
  if len(args) != 1:
    parser.error('Failed to process args %s' % args)

  duration = int(args[0])
  if options.gtest_list_tests:
    for fixture, cases in TESTS.iteritems():
      print '%s.' % fixture
      for case in cases:
        print '  ' + case
    print '  YOU HAVE 2 tests with ignored failures (FAILS prefix)'
    print ''
    return 0

  if options.gtest_filter:
    print 'Note: Google Test filter = %s\n' % options.gtest_filter
    test_cases = sorted(options.gtest_filter.split(':'))
  else:
    test_cases = sorted(
        sum((['%s.%s' % (f, c) for c in TESTS[f]] for f in TESTS), []))

  for i in test_cases:
    time.sleep(float(duration) / 1000.)
    print gtest_fake_base.get_test_output(i, False, duration=str(duration))
  print gtest_fake_base.get_footer(len(test_cases), len(test_cases))
  return 0


if __name__ == '__main__':
  sys.exit(main())
