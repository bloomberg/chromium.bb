# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page_test

from measurements import smoothness_controller


class Repaint(page_test.PageTest):
  def __init__(self):
    super(Repaint, self).__init__()
    self._smoothness_controller = None

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-impl-side-painting',
        '--enable-threaded-compositing',
        '--enable-gpu-benchmarking'
    ])

  def WillNavigateToPage(self, page, tab):
    self._smoothness_controller = smoothness_controller.SmoothnessController()
    self._smoothness_controller.Start(page, tab)

  def DidRunActions(self, page, tab):
    self._smoothness_controller.Stop(tab)

  def ValidateAndMeasurePage(self, page, tab, results):
    self._smoothness_controller.AddResults(tab, results)

  def CleanUpAfterPage(self, _, tab):
    self._smoothness_controller.CleanUp(tab)
