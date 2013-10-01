# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import math

from telemetry.core import util
from metrics import discrepancy

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

def Total(data):
  if type(data) == float:
    total = data
  elif type(data) == int:
    total = float(data)
  elif type(data) == list:
    total = float(sum(data))
  else:
    raise TypeError
  return total

def Average(numerator, denominator, scale = None, precision = None):
  numerator_total = Total(numerator)
  denominator_total = Total(denominator)
  if denominator_total == 0:
    return 0
  avg = numerator_total / denominator_total
  if scale:
    avg *= scale
  if precision:
    avg = round(avg, precision)
  return avg

def DivideIfPossibleOrZero(numerator, denominator):
  if not denominator:
    return 0.0
  else:
    return numerator / denominator

def GeneralizedMean(values, exponent):
  ''' http://en.wikipedia.org/wiki/Generalized_mean '''
  if not values:
    return 0.0
  sum_of_powers = 0.0
  for v in values:
    sum_of_powers += v ** exponent
  return (sum_of_powers / len(values)) ** (1.0/exponent)

def Median(values):
  if not values:
    return 0.0
  sorted_values = sorted(values)
  n = len(values)
  if n % 2:
    median = sorted_values[n/2]
  else:
    median = 0.5 * (sorted_values[n/2] + sorted_values[n/2 - 1])
  return median

def Percentile(values, percentile):
  ''' Computed using linear interpolation between closest ranks. '''
  if not values:
    return 0.0
  sorted_values = sorted(values)
  n = len(values)
  percentile /= 100.0
  if percentile <= 0.5 / n:
    return sorted_values[0]
  elif percentile >= (n - 0.5) / n:
    return sorted_values[-1]
  else:
    floor_index = int(math.floor(n * percentile -  0.5))
    floor_value = sorted_values[floor_index]
    ceil_value = sorted_values[floor_index+1]
    alpha = n * percentile - 0.5 - floor_index
    return floor_value + alpha * (ceil_value - floor_value)

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

  # Scroll Results
  results.Add('frame_times', 'ms', frame_times)
  # Arithmetic mean of frame times.
  results.Add('mean_frame_time', 'ms',
              Average(s.total_time, s.screen_frame_count, 1000, 3))
  # Absolute discrepancy of frame time stamps.
  results.Add('jank', '',
              round(discrepancy.FrameDiscrepancy(s.screen_frame_timestamps,
                                                 True), 4))
  # Are we hitting 60 fps for 95 percent of all frames?
  # We use 17ms as a slightly looser threshold, instead of 1000.0/60.0.
  results.Add('mostly_smooth', '',
              Percentile(frame_times, 95.0) < 17.0)
