# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from perf_tools import scrolling_benchmark

class TextureUploadBenchmark(scrolling_benchmark.ScrollingBenchmark):
  def MeasurePage(self, page, tab, results):
    rendering_stats_deltas = self.ScrollPageFully(page, tab)

    num_frames_sent_to_screen = rendering_stats_deltas['numFramesSentToScreen']

    dropped_percent = (
      rendering_stats_deltas['droppedFrameCount'] /
      float(num_frames_sent_to_screen))

    if (('totalCommitCount' not in rendering_stats_deltas)
        or rendering_stats_deltas['totalCommitCount'] == 0) :
      averageCommitTimeMs = 0
    else :
      averageCommitTimeMs = (
          1000 * rendering_stats_deltas['totalCommitTimeInSeconds'] /
          rendering_stats_deltas['totalCommitCount'])

    results.Add('dropped_percent', '%', round(dropped_percent * 100, 1))
    results.Add('texture_upload_count', 'count',
                rendering_stats_deltas['textureUploadCount'])
    results.Add('average_commit_time', 'ms', averageCommitTimeMs)
