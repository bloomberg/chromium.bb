# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the utils/metrics library."""

from __future__ import print_function

import os
import mock

from chromite.lib import cros_test_lib
from chromite.utils import metrics


class MetricsTest(cros_test_lib.TestCase):
  """Test the utils/Metrics library."""

  def testEndToEnd(self):
    """Test the normal usage pattern, end-to-end."""
    # We should start in a clean, unmeasured state.
    self.assertFalse(os.environ.get(metrics.UTILS_METRICS_LOG_ENVVAR))

    # We mock the concept of time.
    with mock.patch('chromite.utils.metrics.time.time') as mock_time:
      mock_time.side_effect = [128.0, 256.0, 512.3]

      events = []
      # Create a fake usage site of the metrics.
      @metrics.collect_metrics
      def measure_things():
        # Now, in here, we should have set up this env-var. This is a bit of
        # invasive white-box testing for sanity purposes.
        self.assertTrue(os.environ.get(metrics.UTILS_METRICS_LOG_ENVVAR))

        # Now, with our pretend timer, let's record some events.
        with metrics.timer('test.timer'):
          metrics.event('test.named_event')

        for event in metrics.read_metrics_events():
          events.append(event)

      # Run the fake scenario.
      measure_things()

      self.assertEqual(len(events), 3)
      self.assertEqual(events[0].timestamp_epoch_millis, 128000)
      self.assertEqual(events[0].op, metrics.OP_START_TIMER)
      self.assertEqual(events[0].name, 'test.timer')

      self.assertEqual(events[1].timestamp_epoch_millis, 256000)
      self.assertEqual(events[1].op, metrics.OP_NAMED_EVENT)
      self.assertEqual(events[1].name, 'test.named_event')

      self.assertEqual(events[2].timestamp_epoch_millis, 512300)
      self.assertEqual(events[2].op, metrics.OP_STOP_TIMER)
      self.assertEqual(events[2].name, 'test.timer')
