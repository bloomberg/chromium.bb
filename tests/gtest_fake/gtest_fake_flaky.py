#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simulate a flaky google-test executable.

http://code.google.com/p/googletest/
"""

import optparse
import os
import sys

import gtest_fake_base


TESTS = {
  'Foo': [
    'Bar1', 'Bar2', 'Bar3', 'Bar4', 'Bar5', 'Bar6', 'Bar7', 'Bar8', 'Bar9',
  ],
}
TOTAL = sum(len(v) for v in TESTS.itervalues())


def main():
  parser = optparse.OptionParser()
  parser.add_option('--gtest_list_tests', action='store_true')
  parser.add_option('--gtest_print_time', action='store_true')
  parser.add_option('--gtest_filter')
  options, args = parser.parse_args()
  if len(args) != 1:
    parser.error('Need to pass a temporary directory path')

  temp_dir = args[0]

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
    filename = os.path.join(temp_dir, options.gtest_filter)
    # Fails on first run, succeeds on the second.
    should_fail = not os.path.isfile(filename)
    print 'Note: Google Test filter = %s\n' % options.gtest_filter
    print gtest_fake_base.get_test_output(options.gtest_filter, should_fail)
    print gtest_fake_base.get_footer(1, 1)
    if should_fail:
      with open(filename, 'w') as f:
        f.write('bang')
      return 1
    return 0

  for fixture, cases in TESTS.iteritems():
    for case in cases:
      print gtest_fake_base.get_test_output('%s.%s' % (fixture, case), False)
  print gtest_fake_base.get_footer(TOTAL, TOTAL)
  return 1


if __name__ == '__main__':
  sys.exit(main())
