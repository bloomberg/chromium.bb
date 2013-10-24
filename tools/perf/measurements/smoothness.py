# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import smoothness
from metrics import rendering_stats
from telemetry.page import page_test
from telemetry.page import page_measurement
from telemetry.core.timeline.model import MarkerMismatchError
from telemetry.core.timeline.model import MarkerOverlapError


class NotEnoughFramesError(page_measurement.MeasurementFailure):
  def __init__(self):
    super(NotEnoughFramesError, self).__init__(
        'Page output less than two frames')


class MissingDisplayFrameRateError(page_measurement.MeasurementFailure):
  def __init__(self, name):
    super(MissingDisplayFrameRateError, self).__init__(
        'Missing display frame rate metrics: ' + name)


class NoSupportedActionError(page_measurement.MeasurementFailure):
  def __init__(self):
    super(NoSupportedActionError, self).__init__(
        'None of the actions is supported by smoothness measurement')


def GetTimelineMarkerLabelsFromAction(compound_action):
  timeline_marker_labels = []
  if not isinstance(compound_action, list):
    compound_action = [compound_action]
  for action in compound_action:
    if action.GetTimelineMarkerLabel():
      timeline_marker_labels.append(action.GetTimelineMarkerLabel())
  if not timeline_marker_labels:
    raise NoSupportedActionError()
  return timeline_marker_labels


class Smoothness(page_measurement.PageMeasurement):
  def __init__(self):
    super(Smoothness, self).__init__('smoothness')
    self._trace_result = None

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def CanRunForPage(self, page):
    return hasattr(page, 'smoothness')

  def WillRunAction(self, page, tab, action):
    # TODO(ermst): Remove "webkit" category after Blink r157377 is picked up by
    # the reference builds.
    tab.browser.StartTracing('webkit,webkit.console,benchmark', 60)
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StartRawDisplayFrameRateMeasurement()
    tab.ExecuteJavaScript(
        'console.time("' + rendering_stats.RENDER_PROCESS_MARKER + '")')

  def DidRunAction(self, page, tab, action):
    tab.ExecuteJavaScript(
        'console.timeEnd("' + rendering_stats.RENDER_PROCESS_MARKER + '")')
    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      tab.browser.platform.StopRawDisplayFrameRateMeasurement()
    self._trace_result = tab.browser.StopTracing()

  def MeasurePage(self, page, tab, results):
    timeline = self._trace_result.AsTimelineModel()
    render_process_marker = timeline.FindTimelineMarkers(
        rendering_stats.RENDER_PROCESS_MARKER)
    compound_action = page_test.GetCompoundActionFromPage(
        page, self._action_name_to_run)
    timeline_marker_labels = GetTimelineMarkerLabelsFromAction(compound_action)
    try:
      timeline_markers = timeline.FindTimelineMarkers(timeline_marker_labels)
    except MarkerMismatchError:
      # TODO(ernstm): re-raise exception as MeasurementFailure when the
      # reference build was updated.
      timeline_markers = render_process_marker
    except MarkerOverlapError as e:
      raise page_measurement.MeasurementFailure(str(e))

    stats = rendering_stats.RenderingStats(
        render_process_marker, timeline_markers)

    if not stats.frame_times:
      raise NotEnoughFramesError()

    smoothness_metric = smoothness.SmoothnessMetric(stats)
    smoothness_metric.AddResults(tab, results)

    if tab.browser.platform.IsRawDisplayFrameRateSupported():
      for r in tab.browser.platform.GetRawDisplayFrameRateMeasurements():
        if r.value is None:
          raise MissingDisplayFrameRateError(r.name)
        results.Add(r.name, r.unit, r.value)
