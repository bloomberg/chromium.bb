# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import mergetraces

class GroupByProcessAndThreadIdTest(unittest.TestCase):
  def runTest(self):
    # (sec, usec, 'pid:tid', function address).
    input_trace = [
        (100, 10, '2000:2001', 0x5),
        (100, 11, '2000:2001', 0x3),
        (100, 11, '2000:2000', 0x1),
        (100, 12, '2001:2001', 0x6),
        (100, 13, '2000:2002', 0x8),
        (100, 13, '2001:2002', 0x9),
        (100, 14, '2000:2000', 0x7)
    ]

    # Functions should be grouped by thread-id and PIDs should not be
    # interleaved.
    expected_trace = [
        (100, 10, '2000:2001', 0x5),
        (100, 11, '2000:2001', 0x3),
        (100, 11, '2000:2000', 0x1),
        (100, 14, '2000:2000', 0x7),
        (100, 13, '2000:2002', 0x8),
        (100, 12, '2001:2001', 0x6),
        (100, 13, '2001:2002', 0x9)
    ]

    grouped_trace = mergetraces.GroupByProcessAndThreadId(input_trace)

    self.assertEqual(grouped_trace, expected_trace)
