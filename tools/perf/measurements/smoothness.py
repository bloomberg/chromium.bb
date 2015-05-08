# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core.platform import tracing_category_filter
from telemetry.page import page_test
from telemetry.web_perf import timeline_based_measurement


class _CustomResultsWrapper(timeline_based_measurement.ResultsWrapperInterface):

  def _AssertNewValueHasSameInteractionLabel(self, new_value):
    for value in self._results.current_page_run.values:
      if value.name == new_value.name:
        assert value.tir_label == new_value.tir_label, (
          'Smoothness measurement do not support multiple interaction record '
          'labels per page yet. See crbug.com/453109 for more information.')

  def AddValue(self, value):
    value.tir_label = self._result_prefix
    self._AssertNewValueHasSameInteractionLabel(value)
    self._results.AddValue(value)


class Smoothness(page_test.PageTest):
  def __init__(self, needs_browser_restart_after_each_page=False):
    super(Smoothness, self).__init__(needs_browser_restart_after_each_page)
    self._tbm = None

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')
    options.AppendExtraBrowserArgs('--touch-events=enabled')
    options.AppendExtraBrowserArgs('--running-performance-benchmark')

  def WillNavigateToPage(self, page, tab):
    tracing_controller = tab.browser.platform.tracing_controller
    # FIXME: Remove webkit.console when blink.console lands in chromium and
    # the ref builds are updated. crbug.com/386847
    custom_categories = [
        'webkit.console', 'blink.console', 'benchmark', 'trace_event_overhead']
    category_filter = tracing_category_filter.TracingCategoryFilter(
        ','.join(custom_categories))
    self._tbm = timeline_based_measurement.TimelineBasedMeasurement(
        timeline_based_measurement.Options(category_filter),
        _CustomResultsWrapper)
    self._tbm.WillRunUserStory(
        tracing_controller, page.GetSyntheticDelayCategories())

  def ValidateAndMeasurePage(self, _, tab, results):
    tracing_controller = tab.browser.platform.tracing_controller
    self._tbm.Measure(tracing_controller, results)

  def CleanUpAfterPage(self, _, tab):
    tracing_controller = tab.browser.platform.tracing_controller
    self._tbm.DidRunUserStory(tracing_controller)


class Repaint(Smoothness):
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-impl-side-painting',
        '--enable-threaded-compositing',
        '--enable-gpu-benchmarking'
    ])

class SmoothnessWithRestart(Smoothness):
  def __init__(self):
    super(SmoothnessWithRestart, self).__init__(
        needs_browser_restart_after_each_page=True)
