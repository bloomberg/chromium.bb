# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import power
from telemetry.page import page_measurement
from telemetry.timeline import model
from telemetry.value import scalar


class ImageDecoding(page_measurement.PageMeasurement):
  def __init__(self):
    super(ImageDecoding, self).__init__()
    self._power_metric = None

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')
    power.PowerMetric.CustomizeBrowserOptions(options)

  def WillStartBrowser(self, browser):
    self._power_metric = power.PowerMetric(browser)

  def WillNavigateToPage(self, page, tab):
    tab.ExecuteJavaScript("""
        if (window.chrome &&
            chrome.gpuBenchmarking &&
            chrome.gpuBenchmarking.clearImageCache) {
          chrome.gpuBenchmarking.clearImageCache();
        }
    """)
    self._power_metric.Start(page, tab)
    # FIXME: bare 'devtools' is for compatibility with older reference versions
    # only and may eventually be removed.
    # FIXME: Remove webkit.console when blink.console lands in chromium and
    # the ref builds are updated. crbug.com/386847
    tab.browser.StartTracing(
        'disabled-by-default-devtools.timeline*,' +
            'devtools,webkit.console,blink.console')

  def StopBrowserAfterPage(self, browser, page):
    return not browser.tabs[0].ExecuteJavaScript("""
        window.chrome &&
            chrome.gpuBenchmarking &&
            chrome.gpuBenchmarking.clearImageCache;
    """)

  def MeasurePage(self, page, tab, results):
    timeline_data = tab.browser.StopTracing()
    timeline_model = model.TimelineModel(timeline_data)
    self._power_metric.Stop(page, tab)
    self._power_metric.AddResults(tab, results)

    def _IsDone():
      return tab.EvaluateJavaScript('isDone')

    decode_image_events = timeline_model.GetAllEventsOfName('Decode Image')

    # If it is a real image page, then store only the last-minIterations
    # decode tasks.
    if (hasattr(page,
               'image_decoding_measurement_limit_results_to_min_iterations') and
        page.image_decoding_measurement_limit_results_to_min_iterations):
      assert _IsDone()
      min_iterations = tab.EvaluateJavaScript('minIterations')
      decode_image_events = decode_image_events[-min_iterations:]

    durations = [d.duration for d in decode_image_events]
    assert durations, 'Failed to find "Decode Image" trace events.'

    image_decoding_avg = sum(durations) / len(durations)
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'ImageDecoding_avg', 'ms', image_decoding_avg,
        description='Average decode time for images in 4 different '
                    'formats: gif, png, jpg, and webp. The image files are '
                    'located at chrome/test/data/image_decoding.'))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'ImageLoading_avg', 'ms',
        tab.EvaluateJavaScript('averageLoadingTimeMs()')))

  def CleanUpAfterPage(self, page, tab):
    if tab.browser.is_tracing_running:
      tab.browser.StopTracing()
