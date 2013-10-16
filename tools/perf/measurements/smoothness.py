# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import smoothness
from metrics.rendering_stats import RenderingStats
from telemetry.page import page_test
from telemetry.page import page_measurement


class DidNotScrollException(page_measurement.MeasurementFailure):
  def __init__(self):
    super(DidNotScrollException, self).__init__('Page did not scroll')


class MissingDisplayFrameRate(page_measurement.MeasurementFailure):
  def __init__(self, name):
    super(MissingDisplayFrameRate, self).__init__(
        'Missing display frame rate metrics: ' + name)


class NoSupportedActionException(page_measurement.MeasurementFailure):
  def __init__(self):
    super(NoSupportedActionException, self).__init__(
        'None of the actions is supported by smoothness measurement')


def GetTimelineMarkerLabelsFromAction(compound_action):
  timeline_marker_labels = []
  if not isinstance(compound_action, list):
    compound_action = [compound_action]
  for action in compound_action:
    if action.GetTimelineMarkerLabel():
      timeline_marker_labels.append(action.GetTimelineMarkerLabel())
  if not timeline_marker_labels:
    raise NoSupportedActionException()
  return timeline_marker_labels


class Smoothness(page_measurement.PageMeasurement):
  def __init__(self):
    super(Smoothness, self).__init__('smoothness')
    self._metrics = None
    self._trace_result = None

  def CustomizeBrowserOptions(self, options):
    smoothness.SmoothnessMetrics.CustomizeBrowserOptions(options)

  def CanRunForPage(self, page):
    return hasattr(page, 'smoothness')

  def WillRunAction(self, page, tab, action):
    # TODO(ermst): Remove "webkit" category after Blink r157377 is picked up by
    # the reference builds.
    tab.browser.StartTracing('webkit,webkit.console,benchmark', 60)
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StartRawDisplayFrameRateMeasurement()
    self._metrics = smoothness.SmoothnessMetrics(tab)
    if action.CanBeBound():
      self._metrics.BindToAction(action)
    else:
      self._metrics.Start()
      tab.ExecuteJavaScript(
          'console.time("' + smoothness.RENDER_PROCESS_MARKER + '")')

  def DidRunAction(self, page, tab, action):
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StopRawDisplayFrameRateMeasurement()
    if not action.CanBeBound():
      tab.ExecuteJavaScript(
          'console.timeEnd("' + smoothness.RENDER_PROCESS_MARKER + '")')
      self._metrics.Stop()
    self._trace_result = tab.browser.StopTracing()

  def MeasurePage(self, page, tab, results):
    rendering_stats_deltas = self._metrics.deltas

    # TODO(ernstm): remove numFramesSentToScreen when RenderingStats
    # cleanup CL was picked up by the reference build.
    if 'frameCount' in rendering_stats_deltas:
      frame_count = rendering_stats_deltas.get('frameCount', 0)
    else:
      frame_count = rendering_stats_deltas.get('numFramesSentToScreen', 0)

    if not (frame_count > 0):
      raise DidNotScrollException()

    timeline = self._trace_result.AsTimelineModel()
    render_process_marker = smoothness.FindTimelineMarkers(
        timeline, smoothness.RENDER_PROCESS_MARKER)
    compound_action = page_test.GetCompoundActionFromPage(
        page, self._action_name_to_run)
    timeline_marker_labels = GetTimelineMarkerLabelsFromAction(compound_action)
    timeline_markers = smoothness.FindTimelineMarkers(timeline,
        timeline_marker_labels)

    benchmark_stats = RenderingStats(render_process_marker,
                                     timeline_markers,
                                     rendering_stats_deltas,
                                     self._metrics.is_using_gpu_benchmarking)
    smoothness.CalcResults(benchmark_stats, results)

    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      for r in tab.browser.platform.GetRawDisplayFrameRateMeasurements():
        if r.value is None:
          raise MissingDisplayFrameRate(r.name)
        results.Add(r.name, r.unit, r.value)
