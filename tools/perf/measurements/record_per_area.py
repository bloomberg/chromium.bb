# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from metrics import smoothness
from telemetry.core import util
from telemetry.page import page_measurement

class RecordPerArea(page_measurement.PageMeasurement):
  def __init__(self):
    super(RecordPerArea, self).__init__('', True)

  def AddCommandLineOptions(self, parser):
    parser.add_option('--start-wait-time', dest='start_wait_time',
                      default=2,
                      help='Wait time before the benchmark is started ' +
                      '(must be long enought to load all content)')

  def CustomizeBrowserOptions(self, options):
    smoothness.SmoothnessMetrics.CustomizeBrowserOptions(options)
    options.AppendExtraBrowserArgs([
        '--enable-impl-side-painting',
        '--force-compositing-mode',
        '--enable-threaded-compositing',
        '--enable-gpu-benchmarking'
    ])

  def MeasurePage(self, page, tab, results):
    # Wait until the page has loaded and come to a somewhat steady state.
    # Needs to be adjusted for every device (~2 seconds for workstation).
    time.sleep(float(self.options.start_wait_time))

    # Enqueue benchmark
    tab.ExecuteJavaScript("""
        window.benchmark_results = {};
        window.benchmark_results.done = false;
        chrome.gpuBenchmarking.runMicroBenchmark(
            "picture_record_benchmark",
            function(value) {
              window.benchmark_results.done = true;
              window.benchmark_results.results = value;
            });
    """)

    def _IsDone():
      return tab.EvaluateJavaScript(
          'window.benchmark_results.done', timeout=120)
    util.WaitFor(_IsDone, timeout=120)

    all_data = tab.EvaluateJavaScript('window.benchmark_results.results')
    for data in all_data:
      results.Add('time_for_area_%07d' % (data['area']), 'ms', data['time_ms'])
