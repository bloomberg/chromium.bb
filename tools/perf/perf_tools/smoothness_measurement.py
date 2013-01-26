# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

class SmoothnessMeasurement(object):
  def __init__(self, tab):
    self._tab = tab
    # Bring in the smoothness benchmark
    with open(
      os.path.join(os.path.dirname(__file__),
                   'smoothness_measurement.js')) as f:
      js = f.read()
      tab.ExecuteJavaScript(js)

  def Start(self):
    self._tab.ExecuteJavaScript(
        'window.__renderingStats = new __RenderingStats();'
        'window.__renderingStats.start()')

  def Stop(self):
    self._tab.ExecuteJavaScript('window.__renderingStats.stop()')

  def BindToAction(self, action):
    # Make the scroll test start and stop measurement automatically.
    self._tab.ExecuteJavaScript(
        'window.__renderingStats = new __RenderingStats();')
    action.BindMeasurementJavaScript(self._tab,
                                     'window.__renderingStats.start();',
                                     'window.__renderingStats.stop();')

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
