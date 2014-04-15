# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import smoothness_controller
from telemetry.page import page_measurement


class Repaint(page_measurement.PageMeasurement):
  def __init__(self):
    super(Repaint, self).__init__('RunRepaint', False)
    self._smoothness_controller = None

  def WillRunActions(self, page, tab):
    tab.WaitForDocumentReadyStateToBeComplete()
    self._smoothness_controller = smoothness_controller.SmoothnessController()
    self._smoothness_controller.Start(page, tab)
    # Rasterize only what's visible.
    tab.ExecuteJavaScript(
        'chrome.gpuBenchmarking.setRasterizeOnlyVisibleContent();')

  def DidRunActions(self, page, tab):
    self._smoothness_controller.Stop(tab)

  def MeasurePage(self, page, tab, results):
    self._smoothness_controller.AddResults(tab, results)

  def CleanUpAfterPage(self, _, tab):
    self._smoothness_controller.CleanUp(tab)
