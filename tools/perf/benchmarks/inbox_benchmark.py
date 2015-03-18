# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets import inbox
from telemetry import benchmark
from telemetry.web_perf import timeline_based_measurement

@benchmark.Disabled  # http://crbug.com/452257
class Inbox(benchmark.Benchmark):
  """Runs the timeline based measurement against inbox pageset."""
  test = timeline_based_measurement.TimelineBasedMeasurement

  def CreatePageSet(self, options):
    return inbox.InboxPageSet()
