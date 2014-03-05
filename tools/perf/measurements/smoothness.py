# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import power
from metrics import smoothness
from telemetry.page import page_measurement


class Smoothness(page_measurement.PageMeasurement):
  def __init__(self):
    super(Smoothness, self).__init__('smoothness')
    self._smoothness_metric = None
    self._power_metric = None

  def CustomizeBrowserOptions(self, options):
    power.PowerMetric.CustomizeBrowserOptions(options)

  def CanRunForPage(self, page):
    return hasattr(page, 'smoothness')

  def WillRunActions(self, page, tab):
    self._power_metric = power.PowerMetric()
    self._power_metric.Start(page, tab)
    self._smoothness_metric = smoothness.SmoothnessMetric()
    self._smoothness_metric.Start(page, tab)

  def DidRunAction(self, page, tab, action):
    self._smoothness_metric.AddActionToIncludeInMetric(action)

  def DidRunActions(self, page, tab):
    self._power_metric.Stop(page, tab)
    self._smoothness_metric.Stop(page, tab)

  def MeasurePage(self, page, tab, results):
    self._power_metric.AddResults(tab, results)
    self._smoothness_metric.AddResults(tab, results)
