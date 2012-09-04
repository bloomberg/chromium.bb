# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provide common functionality to simulate a google-test executable.

http://code.google.com/p/googletest/
"""

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


def get_footer(number, total):
  return (
    '[----------] Global test environment tear-down\n'
    '[==========] %(number)d test from %(total)d test case ran. (0 ms total)\n'
    '[  PASSED  ] %(number)d test.\n'
    '\n'
    '  YOU HAVE 5 DISABLED TESTS\n'
    '\n'
    '  YOU HAVE 2 tests with ignored failures (FAILS prefix)\n') % {
      'number': number,
      'total': total,
    }
