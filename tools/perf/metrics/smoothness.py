# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import Metric
from metrics import rendering_stats
from metrics import statistics
from telemetry.page import page_measurement
from telemetry.page.perf_tests_helper import FlattenList
from telemetry.core.timeline.model import TimelineModel

TIMELINE_MARKER = 'Smoothness'


class MissingDisplayFrameRateError(page_measurement.MeasurementFailure):
  def __init__(self, name):
    super(MissingDisplayFrameRateError, self).__init__(
        'Missing display frame rate metrics: ' + name)


class NotEnoughFramesError(page_measurement.MeasurementFailure):
  def __init__(self):
    super(NotEnoughFramesError, self).__init__(
        'Page output less than two frames')


class NoSupportedActionError(page_measurement.MeasurementFailure):
  def __init__(self):
    super(NoSupportedActionError, self).__init__(
        'None of the actions is supported by smoothness measurement')


def _GetSyntheticDelayCategoriesFromPage(page):
  if not hasattr(page, 'synthetic_delays'):
    return []
  result = []
  for delay, options in page.synthetic_delays.items():
    options = '%f;%s' % (options.get('target_duration', 0),
                         options.get('mode', 'static'))
    result.append('DELAY(%s;%s)' % (delay, options))
  return result


class SmoothnessMetric(Metric):
  def __init__(self):
    super(SmoothnessMetric, self).__init__()
    self._stats = None
    self._actions = []

  def AddActionToIncludeInMetric(self, action):
    self._actions.append(action)

  def Start(self, page, tab):
    custom_categories = ['webkit.console', 'benchmark']
    custom_categories += _GetSyntheticDelayCategoriesFromPage(page)
    tab.browser.StartTracing(','.join(custom_categories), 60)
    tab.ExecuteJavaScript('console.time("' + TIMELINE_MARKER + '")')
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StartRawDisplayFrameRateMeasurement()

  def Stop(self, page, tab):
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StopRawDisplayFrameRateMeasurement()
    tab.ExecuteJavaScript('console.timeEnd("' + TIMELINE_MARKER + '")')
    tracing_timeline_data = tab.browser.StopTracing()
    timeline_model = TimelineModel(timeline_data=tracing_timeline_data)
    timeline_ranges = [ action.GetActiveRangeOnTimeline(timeline_model)
                        for action in self._actions ]

    renderer_process = timeline_model.GetRendererProcessFromTab(tab)
    self._stats = rendering_stats.RenderingStats(
        renderer_process, timeline_model.browser_process, timeline_ranges)

    if not self._stats.frame_times:
      raise NotEnoughFramesError()

  def SetStats(self, stats):
    """ Pass in a RenderingStats object directly. For unittests that don't call
        Start/Stop.
    """
    self._stats = stats

  def AddResults(self, tab, results):
    if self._stats.mouse_wheel_scroll_latency:
      mean_mouse_wheel_scroll_latency = statistics.ArithmeticMean(
          self._stats.mouse_wheel_scroll_latency,
          len(self._stats.mouse_wheel_scroll_latency))
      mouse_wheel_scroll_latency_discrepancy = statistics.DurationsDiscrepancy(
          self._stats.mouse_wheel_scroll_latency)
      results.Add('mean_mouse_wheel_scroll_latency', 'ms',
                  round(mean_mouse_wheel_scroll_latency, 3))
      results.Add('mouse_wheel_scroll_latency_discrepancy', '',
                  round(mouse_wheel_scroll_latency_discrepancy, 4))

    if self._stats.touch_scroll_latency:
      mean_touch_scroll_latency = statistics.ArithmeticMean(
          self._stats.touch_scroll_latency,
          len(self._stats.touch_scroll_latency))
      touch_scroll_latency_discrepancy = statistics.DurationsDiscrepancy(
          self._stats.touch_scroll_latency)
      results.Add('mean_touch_scroll_latency', 'ms',
                  round(mean_touch_scroll_latency, 3))
      results.Add('touch_scroll_latency_discrepancy', '',
                  round(touch_scroll_latency_discrepancy, 4))

    if self._stats.js_touch_scroll_latency:
      mean_js_touch_scroll_latency = statistics.ArithmeticMean(
          self._stats.js_touch_scroll_latency,
          len(self._stats.js_touch_scroll_latency))
      js_touch_scroll_latency_discrepancy = statistics.DurationsDiscrepancy(
          self._stats.js_touch_scroll_latency)
      results.Add('mean_js_touch_scroll_latency', 'ms',
                  round(mean_js_touch_scroll_latency, 3))
      results.Add('js_touch_scroll_latency_discrepancy', '',
                  round(js_touch_scroll_latency_discrepancy, 4))

    # List of raw frame times.
    frame_times = FlattenList(self._stats.frame_times)
    results.Add('frame_times', 'ms', frame_times)

    # Arithmetic mean of frame times.
    mean_frame_time = statistics.ArithmeticMean(frame_times,
                                                len(frame_times))
    results.Add('mean_frame_time', 'ms', round(mean_frame_time, 3))

    # Absolute discrepancy of frame time stamps.
    frame_discrepancy = statistics.TimestampsDiscrepancy(
        self._stats.frame_timestamps)
    results.Add('jank', 'ms', round(frame_discrepancy, 4))

    # Are we hitting 60 fps for 95 percent of all frames?
    # We use 19ms as a somewhat looser threshold, instead of 1000.0/60.0.
    percentile_95 = statistics.Percentile(frame_times, 95.0)
    results.Add('mostly_smooth', 'score', 1.0 if percentile_95 < 19.0 else 0.0)

    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      for r in tab.browser.platform.GetRawDisplayFrameRateMeasurements():
        if r.value is None:
          raise MissingDisplayFrameRateError(r.name)
        results.Add(r.name, r.unit, r.value)
