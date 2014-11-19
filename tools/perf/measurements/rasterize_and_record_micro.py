# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import time

from telemetry.core.util import TimeoutException
from telemetry.page import page_test
from telemetry.value import scalar


class RasterizeAndRecordMicro(page_test.PageTest):
  def __init__(self, start_wait_time=2, rasterize_repeat=100, record_repeat=100,
               timeout=120, report_detailed_results=False):
    super(RasterizeAndRecordMicro, self).__init__('')
    self._chrome_branch_number = None
    self._start_wait_time = start_wait_time
    self._rasterize_repeat = rasterize_repeat
    self._record_repeat = record_repeat
    self._timeout = timeout
    self._report_detailed_results = report_detailed_results

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

  def ValidateAndMeasurePage(self, page, tab, results):
    try:
      tab.WaitForDocumentReadyStateToBeComplete()
    except TimeoutException:
      pass
    time.sleep(self._start_wait_time)

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
                  "record_repeat_count": %i,
                  "rasterize_repeat_count": %i
                });
    """ % (self._record_repeat, self._rasterize_repeat))

    benchmark_id = tab.EvaluateJavaScript('window.benchmark_results.id')
    if (not benchmark_id):
      raise page_test.MeasurementFailure(
          'Failed to schedule rasterize_and_record_micro')

    tab.WaitForJavaScriptExpression(
        'window.benchmark_results.done', self._timeout)

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
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'record_time_sk_null_canvas', 'ms',
          record_time_sk_null_canvas))
      results.AddValue(scalar.ScalarValue(
          results.current_page, 'record_time_painting_disabled', 'ms',
          record_time_painting_disabled))

    if self._report_detailed_results:
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
