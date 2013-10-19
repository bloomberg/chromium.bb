# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import statistics
from metrics import Metric


class SmoothnessMetric(Metric):
  def __init__(self, rendering_stats):
    super(SmoothnessMetric, self).__init__()
    self.stats_ = rendering_stats

  def AddResults(self, tab, results):
    # List of raw frame times.
    results.Add('frame_times', 'ms', self.stats_.frame_times)

    # Arithmetic mean of frame times.
    mean_frame_time = statistics.ArithmeticMean(self.stats_.frame_times,
                                                len(self.stats_.frame_times))
    results.Add('mean_frame_time', 'ms', round(mean_frame_time, 3))

    # Absolute discrepancy of frame time stamps.
    jank = statistics.FrameDiscrepancy(self.stats_.frame_timestamps)
    results.Add('jank', '', round(jank, 4))

    # Are we hitting 60 fps for 95 percent of all frames? (Boolean value)
    # We use 17ms as a slightly looser threshold, instead of 1000.0/60.0.
    results.Add('mostly_smooth', '',
        statistics.Percentile(self.stats_.frame_times, 95.0) < 17.0)
