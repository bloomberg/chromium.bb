# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page_measurement

class NavigateMeasurement(page_measurement.PageMeasurement):
  """ Measures time between navigation start and the end of the navigate_steps

  For pages where the 'load' event isn't a good measurement point, this
  measurement allows a page_set to have its loading sequence listed by
  way of navigate_steps, with a navigation_time result measured
  by performance.now().
  """
  def __init__(self):
    super(NavigateMeasurement, self).__init__()
    self._navigate_time = None

  def DidNavigateToPage(self, page, tab):
    self._navigate_time = int(tab.EvaluateJavaScript('performance.now()'))

  def MeasurePage(self, page, tab, results):
    results.Add('navigate_time', 'ms', self._navigate_time)
