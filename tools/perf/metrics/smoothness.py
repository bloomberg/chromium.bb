# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from metrics import statistics
from telemetry.core import util
from telemetry.page import page_measurement

TIMELINE_MARKER = 'smoothness_scroll'
SYNTHETIC_GESTURE_MARKER = 'SyntheticGestureController::running'


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
        'console.time("' + TIMELINE_MARKER + '")',
        'window.__renderingStats.stop(); ' +
        'console.timeEnd("' + TIMELINE_MARKER + '")')

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

  frame_times = []
  for i in xrange(1, len(s.screen_frame_timestamps)):
    frame_times.append(
        round(s.screen_frame_timestamps[i] - s.screen_frame_timestamps[i-1], 2))

  # List of raw frame times.
  results.Add('frame_times', 'ms', frame_times)

  mean_frame_time_ms = 1000 * statistics.ArithmeticMean(
      s.total_time, s.screen_frame_count)
  # Arithmetic mean of frame times. Not the generalized mean.
  results.Add('mean_frame_time', 'ms', round(mean_frame_time_ms, 3))

  # Absolute discrepancy of frame time stamps.
  jank = statistics.FrameDiscrepancy(s.screen_frame_timestamps)
  results.Add('jank', '', round(jank, 4))

  # Are we hitting 60 fps for 95 percent of all frames? (Boolean value)
  # We use 17ms as a slightly looser threshold, instead of 1000.0/60.0.
  results.Add('mostly_smooth', '',
      statistics.Percentile(frame_times, 95.0) < 17.0)


class MissingTimelineMarker(page_measurement.MeasurementFailure):
  def __init__(self, name):
    super(MissingTimelineMarker, self).__init__(
        'Timeline marker not found: ' + name)


def FindTimelineMarker(timeline, name):
  """Find the timeline event with the given name.

  If there is not exactly one such timeline event, raise an error.
  """
  events = [s for s in timeline.GetAllEventsOfName(name)
            if s.parent_slice == None]
  if len(events) != 1:
    raise MissingTimelineMarker(name)
  return events[0]

