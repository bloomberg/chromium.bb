# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for utils.py."""

from __future__ import print_function

from datetime import datetime, date

from google.protobuf.timestamp_pb2 import Timestamp

from chromite.lib import cros_test_lib
from chromite.lib.luci import utils

class TestTimeParsingFunctions(cros_test_lib.MockTestCase):
  """Test time parsing functions in luci/utils.py."""

  def testTimestampToDatetime(self):
    # Test None input.
    self.assertIsNone(utils.TimestampToDatetime(None))
    # Test empty input.
    time1 = Timestamp()
    self.assertIsNone(utils.TimestampToDatetime(time1))
    # Test valid input.
    time1.GetCurrentTime()
    formatted_time = utils.TimestampToDatetime(time1)
    self.assertIsNotNone(formatted_time)
    self.assertTrue(isinstance(formatted_time, datetime))

  def testDateToTimestamp(self):
    result = utils.DatetimeToTimestamp(date(2019, 4, 15))
    self.assertEqual(result.seconds, 1555286400)
    result = utils.DatetimeToTimestamp(date(2019, 4, 15), end_of_day=True)
    self.assertEqual(result.seconds, 1555372740)
    with self.assertRaises(AssertionError):
      utils.DatetimeToTimestamp(None)
