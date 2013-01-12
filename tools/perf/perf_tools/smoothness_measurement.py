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
      tab.runtime.Execute(js)

  def bindToScrollInteraction(self):
    tab = self._tab
    if tab.runtime.Evaluate('window.__scrollingInteraction === undefined'):
      raise Exception('bindToScrollInteraction requires ' +
                      'window.__scrollingInteraction to be set.')

    # Make the scroll test start and stop measurement automatically.
    tab.runtime.Execute("""
        window.__renderingStats = new __RenderingStats();
        window.__scrollingInteraction.beginMeasuringHook = function() {
            window.__renderingStats.start();
        };
        window.__scrollingInteraction.endMeasuringHook = function() {
            window.__renderingStats.stop();
        };
    """)

  @property
  def start_values(self):
    return self._tab.runtime.Evaluate(
      'window.__renderingStats.getStartValues()')

  @property
  def end_values(self):
    return self._tab.runtime.Evaluate(
      'window.__renderingStats.getEndValues()')

  @property
  def deltas(self):
    return self._tab.runtime.Evaluate(
      'window.__renderingStats.getDeltas()')
