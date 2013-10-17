# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
from operator import attrgetter

from metrics import statistics
from telemetry.core import util
from telemetry.page import page_measurement

RENDER_PROCESS_MARKER = 'RenderProcessMarker'


class SmoothnessMetrics(object):
  def __init__(self, tab):
    self._tab = tab
    with open(
      os.path.join(os.path.dirname(__file__),
                   'smoothness.js')) as f:
      js = f.read()
      tab.ExecuteJavaScript(js)

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def Start(self):
    self._tab.ExecuteJavaScript(
        'window.__renderingStats = new __RenderingStats();'
        'window.__renderingStats.start()')

  def SetNeedsDisplayOnAllLayersAndStart(self):
    self._tab.ExecuteJavaScript(
        'chrome.gpuBenchmarking.setNeedsDisplayOnAllLayers();'
        'window.__renderingStats = new __RenderingStats();'
        'window.__renderingStats.start()')

  def Stop(self):
    self._tab.ExecuteJavaScript('window.__renderingStats.stop()')

  def BindToAction(self, action):
    # Make the scroll test start and stop measurement automatically.
    self._tab.ExecuteJavaScript(
        'window.__renderingStats = new __RenderingStats();')
    action.BindMeasurementJavaScript(
        self._tab,
        'window.__renderingStats.start(); ' +
        'console.time("' + RENDER_PROCESS_MARKER + '")',
        'window.__renderingStats.stop(); ' +
        'console.timeEnd("' + RENDER_PROCESS_MARKER + '")')

  @property
  def is_using_gpu_benchmarking(self):
    return self._tab.EvaluateJavaScript(
      'window.__renderingStats.isUsingGpuBenchmarking()')

  @property
  def start_values(self):
    return self._tab.EvaluateJavaScript(
      'window.__renderingStats.getStartValues()')

  @property
  def end_values(self):
    return self._tab.EvaluateJavaScript(
      'window.__renderingStats.getEndValues()')

  @property
  def deltas(self):
    return self._tab.EvaluateJavaScript(
      'window.__renderingStats.getDeltas()')


def CalcFirstPaintTimeResults(results, tab):
  if tab.browser.is_content_shell:
    results.Add('first_paint', 'ms', 'unsupported')
    return

  tab.ExecuteJavaScript("""
      window.__rafFired = false;
      window.webkitRequestAnimationFrame(function() {
          window.__rafFired  = true;
      });
  """)
  util.WaitFor(lambda: tab.EvaluateJavaScript('window.__rafFired'), 60)

  first_paint_secs = tab.EvaluateJavaScript(
      'window.chrome.loadTimes().firstPaintTime - ' +
      'window.chrome.loadTimes().startLoadTime')

  results.Add('first_paint', 'ms', round(first_paint_secs * 1000, 1))


def CalcResults(benchmark_stats, results):
  s = benchmark_stats

  # List of raw frame times.
  results.Add('frame_times', 'ms', s.frame_times)

  # Arithmetic mean of frame times.
  mean_frame_time = statistics.ArithmeticMean(s.frame_times,
                                              len(s.frame_times))
  results.Add('mean_frame_time', 'ms', round(mean_frame_time, 3))

  # Absolute discrepancy of frame time stamps.
  jank = statistics.FrameDiscrepancy(s.frame_timestamps)
  results.Add('jank', '', round(jank, 4))

  # Are we hitting 60 fps for 95 percent of all frames? (Boolean value)
  # We use 17ms as a slightly looser threshold, instead of 1000.0/60.0.
  results.Add('mostly_smooth', '',
      statistics.Percentile(s.frame_times, 95.0) < 17.0)


class TimelineMarkerMismatchException(page_measurement.MeasurementFailure):
  def __init__(self):
    super(TimelineMarkerMismatchException, self).__init__(
        'Number or order of timeline markers does not match provided labels')


class TimelineMarkerOverlapException(page_measurement.MeasurementFailure):
  def __init__(self):
    super(TimelineMarkerOverlapException, self).__init__(
        'Overlapping timeline markers found')


def FindTimelineMarkers(timeline, timeline_marker_labels):
  """Find the timeline events with the given names.

  If the number and order of events found does not match the labels,
  raise an error.
  """
  events = []
  if not type(timeline_marker_labels) is list:
    timeline_marker_labels = [timeline_marker_labels]
  for label in timeline_marker_labels:
    if not label:
      continue
    events = [s for s in timeline.GetAllEventsOfName(label)
              if s.parent_slice == None]
  events.sort(key=attrgetter('start'))

  if len(events) != len(timeline_marker_labels):
    raise TimelineMarkerMismatchException()

  for (i, event) in enumerate(events):
    if timeline_marker_labels[i] and event.name != timeline_marker_labels[i]:
      raise TimelineMarkerMismatchException()

  for i in xrange(0, len(events)):
    for j in xrange(i+1, len(events)):
      if (events[j].start < events[i].start + events[i].duration):
        raise TimelineMarkerOverlapException()

  return events
