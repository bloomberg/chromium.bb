# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import time

from telemetry.core.util import TimeoutException
from telemetry.page import page_test
from telemetry.value import scalar


class RasterizeAndRecordMicro(page_test.PageTest):
  def __init__(self):
    super(RasterizeAndRecordMicro, self).__init__('')
    self._chrome_branch_number = None

  @classmethod
  def AddCommandLineArgs(cls, parser):
    parser.add_option('--start-wait-time', type='float',
                      default=2,
                      help='Wait time before the benchmark is started '
                      '(must be long enought to load all content)')
    parser.add_option('--rasterize-repeat', type='int',
                      default=100,
                      help='Repeat each raster this many times. Increase '
                      'this value to reduce variance.')
    parser.add_option('--record-repeat', type='int',
                      default=100,
                      help='Repeat each record this many times. Increase '
                      'this value to reduce variance.')
    parser.add_option('--timeout', type='int',
                      default=120,
                      help='The length of time to wait for the micro '
                      'benchmark to finish, expressed in seconds.')
    parser.add_option('--report-detailed-results',
                      action='store_true',
                      help='Whether to report additional detailed results.')

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-impl-side-painting',
        '--enable-threaded-compositing',
        '--enable-gpu-benchmarking'
    ])

  def DidStartBrowser(self, browser):
    # TODO(vmpstr): Remove this temporary workaround when reference build has
    # been updated to branch 1713 or later.
    backend = browser._browser_backend # pylint: disable=W0212
    self._chrome_branch_number = getattr(backend, 'chrome_branch_number', None)
    if (not self._chrome_branch_number or
        (sys.platform != 'android' and self._chrome_branch_number < 1713)):
      raise page_test.TestNotSupportedOnPlatformFailure(
          'rasterize_and_record_micro requires Chrome branch 1713 '
          'or later. Skipping measurement.')

  def MeasurePage(self, page, tab, results):
    try:
      tab.WaitForDocumentReadyStateToBeComplete()
    except TimeoutException:
      pass
    time.sleep(self.options.start_wait_time)

    record_repeat = self.options.record_repeat
    rasterize_repeat = self.options.rasterize_repeat
    # Enqueue benchmark
    tab.ExecuteJavaScript("""
        window.benchmark_results = {};
        window.benchmark_results.done = false;
        window.benchmark_results.id =
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

    benchmark_id = tab.EvaluateJavaScript('window.benchmark_results.id')
    if (not benchmark_id):
      raise page_test.MeasurementFailure(
          'Failed to schedule rasterize_and_record_micro')

    tab.WaitForJavaScriptExpression(
        'window.benchmark_results.done', self.options.timeout)

    data = tab.EvaluateJavaScript('window.benchmark_results.results')

    pixels_recorded = data['pixels_recorded']
    record_time = data['record_time_ms']
    pixels_rasterized = data['pixels_rasterized']
    rasterize_time = data['rasterize_time_ms']

    results.AddValue(scalar.ScalarValue(
        results.current_page, 'pixels_recorded', 'pixels', pixels_recorded))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'record_time', 'ms', record_time))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'pixels_rasterized', 'pixels', pixels_rasterized))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'rasterize_time', 'ms', rasterize_time))

    # TODO(skyostil): Remove this temporary workaround when reference build has
    # been updated to branch 1931 or later.
    if ((self._chrome_branch_number and self._chrome_branch_number >= 1931) or
        sys.platform == 'android'):
      record_time_sk_null_canvas = data['record_time_sk_null_canvas_ms']
      record_time_painting_disabled = data['record_time_painting_disabled_ms']
      record_time_skrecord = data['record_time_skrecord_ms']
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'record_time_sk_null_canvas', 'ms',
          record_time_sk_null_canvas))
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'record_time_painting_disabled', 'ms',
          record_time_painting_disabled))
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'record_time_skrecord', 'ms',
          record_time_skrecord))

    if self.options.report_detailed_results:
      pixels_rasterized_with_non_solid_color = \
          data['pixels_rasterized_with_non_solid_color']
      pixels_rasterized_as_opaque = \
          data['pixels_rasterized_as_opaque']
      total_layers = data['total_layers']
      total_picture_layers = data['total_picture_layers']
      total_picture_layers_with_no_content = \
          data['total_picture_layers_with_no_content']
      total_picture_layers_off_screen = \
          data['total_picture_layers_off_screen']

      results.AddValue(scalar.ScalarValue(
          results.current_page, 'pixels_rasterized_with_non_solid_color',
          'pixels', pixels_rasterized_with_non_solid_color))
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'pixels_rasterized_as_opaque', 'pixels',
          pixels_rasterized_as_opaque))
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'total_layers', 'count', total_layers))
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'total_picture_layers', 'count',
          total_picture_layers))
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'total_picture_layers_with_no_content', 'count',
          total_picture_layers_with_no_content))
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'total_picture_layers_off_screen', 'count',
          total_picture_layers_off_screen))
