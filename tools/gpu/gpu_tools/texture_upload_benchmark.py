# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from chrome_remote_control import multi_page_benchmark
from gpu_tools import scrolling_benchmark

class TextureUploadBenchmark(scrolling_benchmark.ScrollingBenchmark):
  def MeasurePage(self, _, tab):
    rendering_stats_deltas = self.ScrollPageFully(tab)

    if (("totalCommitCount" not in rendering_stats_deltas)
        or rendering_stats_deltas["totalCommitCount"] == 0) :
      averageCommitTimeMs = 0
    else :
      averageCommitTimeMs = (
          1000 * rendering_stats_deltas["totalCommitTimeInSeconds"] /
          rendering_stats_deltas["totalCommitCount"])

    return {
      'texture_upload_count': rendering_stats_deltas['textureUploadCount'],
      'average_commit_time_ms': averageCommitTimeMs
    }

def Main():
  return multi_page_benchmark.Main(TextureUploadBenchmark())
