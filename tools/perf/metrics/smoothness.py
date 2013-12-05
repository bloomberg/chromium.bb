# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import Metric
from metrics import rendering_stats
from metrics import statistics
from telemetry.core.timeline.model import MarkerMismatchError
from telemetry.core.timeline.model import MarkerOverlapError
from telemetry.page import page_measurement

TIMELINE_MARKER = 'Smoothness'


class NotEnoughFramesError(page_measurement.MeasurementFailure):
  def __init__(self):
    super(NotEnoughFramesError, self).__init__(
        'Page output less than two frames')


class NoSupportedActionError(page_measurement.MeasurementFailure):
  def __init__(self):
    super(NoSupportedActionError, self).__init__(
        'None of the actions is supported by smoothness measurement')


class SmoothnessMetric(Metric):
  def __init__(self):
    super(SmoothnessMetric, self).__init__()
    self._stats = None
    self._timeline_marker_names = []

  def AddTimelineMarkerNameToIncludeInMetric(self, timeline_marker_name):
    self._timeline_marker_names.append(timeline_marker_name)

  def Start(self, page, tab):
    tab.browser.StartTracing('webkit.console,benchmark', 60)
    tab.ExecuteJavaScript('console.time("' + TIMELINE_MARKER + '")')

  def Stop(self, page, tab):
    tab.ExecuteJavaScript('console.timeEnd("' + TIMELINE_MARKER + '")')
    timeline_model = tab.browser.StopTracing().AsTimelineModel()
    try:
      timeline_markers = timeline_model.FindTimelineMarkers(
          self._timeline_marker_names)
    except MarkerMismatchError as e:
      raise page_measurement.MeasurementFailure(str(e))
    except MarkerOverlapError as e:
      raise page_measurement.MeasurementFailure(str(e))

    renderer_process = timeline_model.GetRendererProcessFromTab(tab)
    self._stats = rendering_stats.RenderingStats(
        renderer_process, timeline_markers)

    if not self._stats.frame_times:
      raise NotEnoughFramesError()

  def SetStats(self, stats):
    """ Pass in a RenderingStats object directly. For unittests that don't call
        Start/Stop.
    """
    self._stats = stats

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
    # We use 19ms as a somewhat looser threshold, instead of 1000.0/60.0.
    percentile_95 = statistics.Percentile(self._stats.frame_times, 95.0)
    results.Add('mostly_smooth', '', 1.0 if percentile_95 < 19.0 else 0.0)
