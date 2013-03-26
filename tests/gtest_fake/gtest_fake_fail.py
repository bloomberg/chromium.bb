#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simulate a failing google-test executable.

http://code.google.com/p/googletest/
"""

import sys

import gtest_fake_base


TESTS = {
  'Foo': ['Bar1', 'Bar2', 'Bar3'],
  'Baz': ['Fail'],
}


def main():
  test_cases, _ = gtest_fake_base.parse_args(TESTS, 0)

  result = 0
  for test_case in test_cases:
    # Make Baz.Fail fail.
    should_fail = test_case == 'Baz.Fail'
    result = result or int(should_fail)
    print gtest_fake_base.get_test_output(test_case, should_fail)
  print gtest_fake_base.get_footer(len(test_cases), len(test_cases))
  return result


if __name__ == '__main__':
  sys.exit(main())
