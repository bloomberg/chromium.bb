# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import smoothness_controller
from telemetry.page import page_test


class Repaint(page_test.PageTest):
  def __init__(self):
    super(Repaint, self).__init__('RunRepaint', False)
    self._smoothness_controller = None
    self._micro_benchmark_id = None

  @classmethod
  def AddCommandLineArgs(cls, parser):
    parser.add_option('--mode', type='string',
                      default='viewport',
                      help='Invalidation mode. '
                      'Supported values: fixed_size, layer, random, viewport.')
    parser.add_option('--width', type='int',
                      default=None,
                      help='Width of invalidations for fixed_size mode.')
    parser.add_option('--height', type='int',
                      default=None,
                      help='Height of invalidations for fixed_size mode.')

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
    args['mode'] = self.options.mode
    if self.options.width:
      args['width'] = self.options.width
    if self.options.height:
      args['height'] = self.options.height

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

  def MeasurePage(self, page, tab, results):
    self._smoothness_controller.AddResults(tab, results)

  def CleanUpAfterPage(self, _, tab):
    self._smoothness_controller.CleanUp(tab)
