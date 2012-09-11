# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

import multi_page_benchmark
import util
import chrome_remote_control

class DidNotScrollException(multi_page_benchmark.MeasurementFailure):
  def __init__(self):
    super(DidNotScrollException, self).__init__("Page did not scroll")

def _CalcScrollResults(rendering_stats):
  num_frames_sent_to_screen = rendering_stats['numFramesSentToScreen']

  mean_frame_time_seconds = (
    rendering_stats['totalTimeInSeconds'] / float(num_frames_sent_to_screen))

  dropped_percent = (
    rendering_stats['droppedFrameCount'] /
    float(num_frames_sent_to_screen))

  return {
      "mean_frame_time_ms": util.RoundTo3DecimalPlaces(
          mean_frame_time_seconds * 1000),
      "dropped_percent": util.RoundTo1DecimalPlace(
         dropped_percent * 100)
      }

class ScrollingBenchmark(multi_page_benchmark.MultiPageBenchmark):
  def __init__(self):
    super(ScrollingBenchmark, self).__init__()
    self.use_gpu_bencharking_extension = True

  def ScrollPageFully(self, tab):
    scroll_js_path = os.path.join(os.path.dirname(__file__), 'scroll.js')
    scroll_js = open(scroll_js_path, 'r').read()

    # Run scroll test.
    tab.runtime.Execute(scroll_js)
    tab.runtime.Execute("""
      window.__scrollTestResult = null;
      new __ScrollTest(function(rendering_stats) {
        window.__scrollTestResult = rendering_stats;
      });
    """)

    # Poll for scroll benchmark completion.
    chrome_remote_control.WaitFor(
        lambda: tab.runtime.Evaluate('window.__scrollTestResult'), 60)

    rendering_stats = tab.runtime.Evaluate('window.__scrollTestResult')

    if not (rendering_stats["numFramesSentToScreen"] > 0):
      raise DidNotScrollException()
    return rendering_stats

  def CustomizeBrowserOptions(self, options):
    if self.use_gpu_bencharking_extension:
      options.extra_browser_args.append('--enable-gpu-benchmarking')

  def MeasurePage(self, page, tab):
    rendering_stats = self.ScrollPageFully(tab)
    return _CalcScrollResults(rendering_stats)


def Main():
  return multi_page_benchmark.Main(ScrollingBenchmark())
