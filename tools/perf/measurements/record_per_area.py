# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from measurements import smoothness
from telemetry.page import page_test
from telemetry.value import scalar


class RecordPerArea(page_test.PageTest):
  def __init__(self):
    super(RecordPerArea, self).__init__('', True)

  def AddCommandLineArgs(self, parser):
    parser.add_option('--start-wait-time', type='float',
                      default=2,
                      help='Wait time before the benchmark is started '
                      '(must be long enought to load all content)')

  def CustomizeBrowserOptions(self, options):
    smoothness.Smoothness.CustomizeBrowserOptions(options)
    options.AppendExtraBrowserArgs([
        '--enable-impl-side-painting',
        '--enable-threaded-compositing',
        '--enable-gpu-benchmarking'
    ])

  def ValidateAndMeasurePage(self, page, tab, results):
    # Wait until the page has loaded and come to a somewhat steady state.
    # Needs to be adjusted for every device (~2 seconds for workstation).
    time.sleep(self.options.start_wait_time)

    # Enqueue benchmark
    tab.ExecuteJavaScript("""
        window.benchmark_results = {};
        window.benchmark_results.done = false;
        chrome.gpuBenchmarking.runMicroBenchmark(
            "picture_record_benchmark",
            function(value) {
              window.benchmark_results.done = true;
              window.benchmark_results.results = value;
            }, [{width: 1, height: 1},
                {width: 250, height: 250},
                {width: 500, height: 500},
                {width: 750, height: 750},
                {width: 1000, height: 1000},
                {width: 256, height: 1024},
                {width: 1024, height: 256}]);
    """)

    tab.WaitForJavaScriptExpression('window.benchmark_results.done', 300)

    all_data = tab.EvaluateJavaScript('window.benchmark_results.results')
    for data in all_data:
      width = data['width']
      height = data['height']
      area = width * height
      time_ms = data['time_ms']

      results.AddValue(scalar.ScalarValue(
          results.current_page, 'area_%07d_%dx%d' % (area, width, height),
          'ms', time_ms))
