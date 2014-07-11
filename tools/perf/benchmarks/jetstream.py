# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Apple's JetStream benchmark.

JetStream combines a variety of JavaScript benchmarks, covering a variety of
advanced workloads and programming techniques, and reports a single score that
balances them using geometric mean.

Each benchmark measures a distinct workload, and no single optimization
technique is sufficient to speed up all benchmarks. Latency tests measure that
a web application can start up quickly, ramp up to peak performance quickly,
and run smoothly without interruptions. Throughput tests measure the sustained
peak performance of a web application, ignoring ramp-up time and spikes in
smoothness. Some benchmarks demonstrate tradeoffs, and aggressive or
specialized optimization for one benchmark might make another benchmark slower.
"""

import json
import os

from telemetry import benchmark
from telemetry.page import page_measurement
from telemetry.page import page_set
from telemetry.util import statistics
from telemetry.value import list_of_scalar_values
from telemetry.value import scalar


class _JetstreamMeasurement(page_measurement.PageMeasurement):
  def __init__(self):
    super(_JetstreamMeasurement, self).__init__()

  def WillNavigateToPage(self, page, tab):
    page.script_to_evaluate_on_commit = """
        var __results = [];
        var __real_log = window.console.log;
        window.console.log = function() {
          __results.push(Array.prototype.join.call(arguments, ' '));
          __real_log.apply(this, arguments);
        }
        """

  def MeasurePage(self, page, tab, results):
    get_results_js = """
        (function() {
          for (var i = 0; i < __results.length; i++) {
            if (!__results[i].indexOf('Raw results: ')) return __results[i];
          }
          return null;
        })();
        """

    tab.WaitForDocumentReadyStateToBeComplete()
    tab.EvaluateJavaScript('JetStream.start()')
    tab.WaitForJavaScriptExpression(get_results_js, 600)

    result = tab.EvaluateJavaScript(get_results_js)
    result = json.loads(result.partition(': ')[2])

    all_scores = []
    for k, v in result.iteritems():
      results.AddValue(list_of_scalar_values.ListOfScalarValues(
          results.current_page, k.replace('.', '_'), 'score', v['result'],
          important=False))
      # Collect all test scores to compute geometric mean.
      all_scores.extend(v['result'])
    total = statistics.GeometricMean(all_scores)
    results.AddSummaryValue(
        scalar.ScalarValue(None, 'Score', 'score', total))


@benchmark.Disabled('android', 'xp')  # crbug.com/381742
class Jetstream(benchmark.Benchmark):
  test = _JetstreamMeasurement

  def CreatePageSet(self, options):
    ps = page_set.PageSet(
        archive_data_file='../page_sets/data/jetstream.json',
        make_javascript_deterministic=False,
        file_path=os.path.abspath(__file__))
    ps.AddPageWithDefaultRunNavigate('http://browserbench.org/JetStream/')
    return ps
