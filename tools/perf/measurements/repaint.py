# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from metrics import smoothness
from telemetry.page import page_measurement


class Repaint(page_measurement.PageMeasurement):
  def __init__(self):
    super(Repaint, self).__init__('RunRepaint', False)
    self._smoothness_metric = None

  def WillRunActions(self, page, tab):
    tab.WaitForDocumentReadyStateToBeComplete()
    self._smoothness_metric = smoothness.SmoothnessMetric()
    self._smoothness_metric.Start(page, tab)
    # Rasterize only what's visible.
    tab.ExecuteJavaScript(
        'chrome.gpuBenchmarking.setRasterizeOnlyVisibleContent();')

  def DidRunAction(self, page, tab, action):
    self._smoothness_metric.AddActionToIncludeInMetric(action)

  def DidRunActions(self, page, tab):
    self._smoothness_metric.Stop(page, tab)

  def MeasurePage(self, page, tab, results):
    self._smoothness_metric.AddResults(tab, results)
