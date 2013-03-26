#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simulate a missing test in google-test executable.

http://code.google.com/p/googletest/
"""

import optparse
import os
import sys

import gtest_fake_base


TESTS = {
  'Foo': [
    'Bar1', 'Bar2', 'Bar3',
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
    print 'Note: Google Test filter = %s\n' % options.gtest_filter
    test_cases = sorted(options.gtest_filter.split(':'))
  else:
    test_cases = sorted(
        sum((['%s.%s' % (f, c) for c in TESTS[f]] for f in TESTS), []))

  result = 0
  for test_case in test_cases:
    if test_case == 'Foo.Bar1':
      filename = os.path.join(temp_dir, test_case)
      should_fail = not os.path.isfile(filename)
    else:
      should_fail = False
    if test_case == 'Foo.Bar2':
      # Never run it.
      continue
    print gtest_fake_base.get_test_output(test_case, should_fail)
    if should_fail:
      result = 1
      with open(filename, 'wb') as f:
        f.write('bang')
  print gtest_fake_base.get_footer(len(test_cases), len(test_cases))
  return result


if __name__ == '__main__':
  sys.exit(main())
