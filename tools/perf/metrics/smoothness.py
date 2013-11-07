# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import Metric
from metrics import rendering_stats
from metrics import statistics
from telemetry.core.timeline.model import MarkerMismatchError
from telemetry.core.timeline.model import MarkerOverlapError
from telemetry.page import page_measurement

TIMELINE_MARKER = 'SmoothnessMetric'


class NotEnoughFramesError(page_measurement.MeasurementFailure):
  def __init__(self):
    super(NotEnoughFramesError, self).__init__(
        'Page output less than two frames')


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


class SmoothnessMetric(Metric):
  def __init__(self, compound_action):
    super(SmoothnessMetric, self).__init__()
    self._stats = None
    self._compound_action = compound_action

  def Start(self, page, tab):
    # TODO(ermst): Remove "webkit" category after Blink r157377 is picked up by
    # the reference builds.
    tab.browser.StartTracing('webkit,webkit.console,benchmark', 60)
    tab.ExecuteJavaScript('console.time("' + TIMELINE_MARKER + '")')

  def Stop(self, page, tab):
    tab.ExecuteJavaScript('console.timeEnd("' + TIMELINE_MARKER + '")')
    timeline_model = tab.browser.StopTracing().AsTimelineModel()
    render_process_marker = timeline_model.FindTimelineMarkers(TIMELINE_MARKER)
    timeline_marker_labels = GetTimelineMarkerLabelsFromAction(
        self._compound_action)
    try:
      timeline_markers = timeline_model.FindTimelineMarkers(
          timeline_marker_labels)
    except MarkerMismatchError:
      # TODO(ernstm): re-raise exception as MeasurementFailure when the
      # reference build was updated.
      timeline_markers = render_process_marker
    except MarkerOverlapError as e:
      raise page_measurement.MeasurementFailure(str(e))

    self._stats = rendering_stats.RenderingStats(
        render_process_marker, timeline_markers)

    if not self._stats.frame_times:
      raise NotEnoughFramesError()


  def AddResults(self, tab, results):
    # List of raw frame times.
    results.Add('frame_times', 'ms', self._stats.frame_times)

    # Arithmetic mean of frame times.
    mean_frame_time = statistics.ArithmeticMean(self._stats.frame_times,
                                                len(self._stats.frame_times))
    results.Add('mean_frame_time', 'ms', round(mean_frame_time, 3))

    # Absolute discrepancy of frame time stamps.
    jank = statistics.FrameDiscrepancy(self._stats.frame_timestamps)
    results.Add('jank', '', round(jank, 4))

    # Are we hitting 60 fps for 95 percent of all frames?
    # We use 17ms as a slightly looser threshold, instead of 1000.0/60.0.
    percentile_95 = statistics.Percentile(self._stats.frame_times, 95.0)
    results.Add('mostly_smooth', '', 1.0 if percentile_95 < 17.0 else 0.0)
