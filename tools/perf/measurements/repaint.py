# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import smoothness_controller
from telemetry.page import page_test


class Repaint(page_test.PageTest):
  def __init__(self, mode='viewport', width=None, height=None):
    super(Repaint, self).__init__()
    self._smoothness_controller = None
    self._micro_benchmark_id = None
    self._mode = mode
    self._width = width
    self._height = height

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-impl-side-painting',
        '--enable-threaded-compositing',
        '--enable-gpu-benchmarking'
    ])

  def WillRunActions(self, page, tab):
    tab.WaitForDocumentReadyStateToBeComplete()
    self._smoothness_controller = smoothness_controller.SmoothnessController()
    self._smoothness_controller.SetUp(page, tab)
    self._smoothness_controller.Start(tab)
    # Rasterize only what's visible.
    tab.ExecuteJavaScript(
        'chrome.gpuBenchmarking.setRasterizeOnlyVisibleContent();')

    args = {}
    args['mode'] = self._mode
    if self._width:
      args['width'] = self._width
    if self._height:
      args['height'] = self._height

    # Enque benchmark
    tab.ExecuteJavaScript("""
        window.benchmark_results = {};
        window.benchmark_results.id =
            chrome.gpuBenchmarking.runMicroBenchmark(
                "invalidation_benchmark",
                function(value) {},
                """ + str(args) + """
            );
    """)

    self._micro_benchmark_id = tab.EvaluateJavaScript(
        'window.benchmark_results.id')
    if (not self._micro_benchmark_id):
      raise page_test.MeasurementFailure(
          'Failed to schedule invalidation_benchmark.')

  def DidRunActions(self, page, tab):
    tab.ExecuteJavaScript("""
        window.benchmark_results.message_handled =
            chrome.gpuBenchmarking.sendMessageToMicroBenchmark(
                """ + str(self._micro_benchmark_id) + """, {
                  "notify_done": true
                });
    """)
    self._smoothness_controller.Stop(tab)

  def ValidateAndMeasurePage(self, page, tab, results):
    self._smoothness_controller.AddResults(tab, results)

  def CleanUpAfterPage(self, _, tab):
    self._smoothness_controller.CleanUp(tab)
