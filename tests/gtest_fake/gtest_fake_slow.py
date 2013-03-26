#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simulate a slowly passing google-test executable.

http://code.google.com/p/googletest/
"""

import sys
import time

import gtest_fake_base


TESTS = {
  'Foo': ['Bar1', 'Bar2', 'Bar/3'],
}


def main():
  test_cases, args = gtest_fake_base.parse_args(TESTS, 1)
  duration = int(args[0])

  for i in test_cases:
    time.sleep(float(duration) / 1000.)
    print gtest_fake_base.get_test_output(i, False, duration=str(duration))
  print gtest_fake_base.get_footer(len(test_cases), len(test_cases))
  return 0


if __name__ == '__main__':
  sys.exit(main())
