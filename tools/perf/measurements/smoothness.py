# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page_test

from measurements import smoothness_controller


class Smoothness(page_test.PageTest):
  def __init__(self, enable_auto_issuing_marker=False):
    super(Smoothness, self).__init__()
    self._smoothness_controller = None
    self._enable_auto_issuing_marker = enable_auto_issuing_marker

  @classmethod
  def CustomizeBrowserOptions(cls, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')
    options.AppendExtraBrowserArgs('--touch-events=enabled')
    options.AppendExtraBrowserArgs('--running-performance-benchmark')

  def WillNavigateToPage(self, page, tab):
    self._smoothness_controller = smoothness_controller.SmoothnessController(
        auto_issuing_marker=self._enable_auto_issuing_marker)
    self._smoothness_controller.SetUp(page, tab)

  def WillRunActions(self, page, tab):
    self._smoothness_controller.Start(tab)

  def DidRunActions(self, page, tab):
    self._smoothness_controller.Stop(tab)

  def ValidateAndMeasurePage(self, page, tab, results):
    self._smoothness_controller.AddResults(tab, results)

  def CleanUpAfterPage(self, page, tab):
    if self._smoothness_controller:
      self._smoothness_controller.CleanUp(tab)
