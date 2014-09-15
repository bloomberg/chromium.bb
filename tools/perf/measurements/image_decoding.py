# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import power

from telemetry.core.platform import tracing_category_filter
from telemetry.core.platform import tracing_options
from telemetry.page import page_test
from telemetry.timeline import model
from telemetry.value import scalar


class ImageDecoding(page_test.PageTest):
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

    options = tracing_options.TracingOptions()
    options.enable_chrome_trace = True
    # FIXME: Remove the timeline category when impl-side painting is on
    # everywhere.
    category_filter = tracing_category_filter.TracingCategoryFilter(
        'disabled-by-default-devtools.timeline')

    # FIXME: Remove webkit.console when blink.console lands in chromium and
    # the ref builds are updated. crbug.com/386847
    # FIXME: Remove the devtools.timeline category when impl-side painting is
    # on everywhere.
    categories = [
        'blink',
        'devtools.timeline',
        'webkit.console',
        'blink.console'
    ]
    for c in categories:
      category_filter.AddIncludedCategory(c)
    tab.browser.platform.tracing_controller.Start(options, category_filter)

  def StopBrowserAfterPage(self, browser, page):
    return not browser.tabs[0].ExecuteJavaScript("""
        window.chrome &&
            chrome.gpuBenchmarking &&
            chrome.gpuBenchmarking.clearImageCache;
    """)

  def ValidateAndMeasurePage(self, page, tab, results):
    timeline_data = tab.browser.platform.tracing_controller.Stop()
    timeline_model = model.TimelineModel(timeline_data)
    self._power_metric.Stop(page, tab)
    self._power_metric.AddResults(tab, results)

    def _IsDone():
      return tab.EvaluateJavaScript('isDone')

    decode_image_events = timeline_model.GetAllEventsOfName(
        'ImageFrameGenerator::decode')
    # FIXME: Remove this when impl-side painting is on everywhere.
    if not decode_image_events:
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
    assert durations, 'Failed to find image decode trace events.'

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
    tracing_controller = tab.browser.platform.tracing_controller
    if tracing_controller.is_tracing_running:
      tracing_controller.Stop()
