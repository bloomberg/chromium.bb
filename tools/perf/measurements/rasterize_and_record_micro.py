# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import time

from telemetry.core.util import TimeoutException
from telemetry.page import page_measurement

class RasterizeAndRecordMicro(page_measurement.PageMeasurement):
  def __init__(self):
    super(RasterizeAndRecordMicro, self).__init__('', True)

  def AddCommandLineOptions(self, parser):
    parser.add_option('--start-wait-time', dest='start_wait_time',
                      default=2,
                      help='Wait time before the benchmark is started ' +
                      '(must be long enought to load all content)')
    parser.add_option('--rasterize-repeat', dest='rasterize_repeat',
                      default=100,
                      help='Repeat each raster this many times. Increase ' +
                      'this value to reduce variance.')
    parser.add_option('--record-repeat', dest='record_repeat',
                      default=100,
                      help='Repeat each record this many times. Increase ' +
                      'this value to reduce variance.')
    parser.add_option('--timeout', dest='timeout',
                      default=120,
                      help='The length of time to wait for the micro ' +
                      'benchmark to finish, expressed in seconds.')

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-impl-side-painting',
        '--force-compositing-mode',
        '--enable-threaded-compositing',
        '--enable-gpu-benchmarking'
    ])

  def MeasurePage(self, page, tab, results):
    # TODO(vmpstr): Remove this temporary workaround when reference build has
    # been updated to branch 1713 or later.
    backend = tab.browser._browser_backend # pylint: disable=W0212
    if (not hasattr(backend, 'chrome_branch_number') or
        (sys.platform != 'android' and backend.chrome_branch_number < 1713)):
      print ('Warning: rasterize_and_record_micro requires Chrome branch 1713 '
             'or later. Skipping measurement.')
      sys.exit(0)

    try:
      tab.WaitForJavaScriptExpression("document.readyState == 'complete'", 10)
    except TimeoutException:
      pass
    time.sleep(float(self.options.start_wait_time))

    record_repeat = self.options.record_repeat
    rasterize_repeat = self.options.rasterize_repeat
    # Enqueue benchmark
    tab.ExecuteJavaScript("""
        window.benchmark_results = {};
        window.benchmark_results.done = false;
        window.benchmark_results.scheduled =
            chrome.gpuBenchmarking.runMicroBenchmark(
                "rasterize_and_record_benchmark",
                function(value) {
                  window.benchmark_results.done = true;
                  window.benchmark_results.results = value;
                }, {
                  "record_repeat_count": """ + str(record_repeat) + """,
                  "rasterize_repeat_count": """ + str(rasterize_repeat) + """
                });
    """)

    scheduled = tab.EvaluateJavaScript('window.benchmark_results.scheduled')
    if (not scheduled):
      raise page_measurement.MeasurementFailure(
          'Failed to schedule rasterize_and_record_micro')

    tab.WaitForJavaScriptExpression(
        'window.benchmark_results.done', self.options.timeout)

    data = tab.EvaluateJavaScript('window.benchmark_results.results')

    pixels_recorded = data['pixels_recorded']
    record_time = data['record_time_ms']
    pixels_rasterized = data['pixels_rasterized']
    rasterize_time = data['rasterize_time_ms']

    results.Add('pixels_recorded', '', pixels_recorded)
    results.Add('record_time', 'ms', record_time)
    results.Add('pixels_rasterized', '', pixels_rasterized)
    results.Add('rasterize_time', 'ms', rasterize_time)

