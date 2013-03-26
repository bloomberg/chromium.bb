#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simulate a crashing-after-pass google-test executable.

http://code.google.com/p/googletest/
"""

import os
import sys

import gtest_fake_base


TESTS = {
  'Foo': ['Bar1', 'Bar2'],
}


def main():
  test_cases, args = gtest_fake_base.parse_args(TESTS, 1)
  temp_dir = args[0]

  result = 0
  for test_case in test_cases:
    filename = os.path.join(temp_dir, test_case)
    # Fails on first run, succeeds on the second.
    should_fail = not os.path.isfile(filename)
    # But it still prints it succeeded.
    print gtest_fake_base.get_test_output(test_case, False)
    result = result or int(should_fail)
    if should_fail:
      with open(filename, 'wb') as f:
        f.write('bang')
  print gtest_fake_base.get_footer(len(test_cases), len(test_cases))
  if result:
    print('OMG I crashed')
    print('Here\'s a stack trace')
  return result


if __name__ == '__main__':
  sys.exit(main())
