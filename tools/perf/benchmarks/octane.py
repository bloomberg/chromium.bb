# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Octane 2.0 javascript benchmark.

Octane 2.0 is a modern benchmark that measures a JavaScript engine's performance
by running a suite of tests representative of today's complex and demanding web
applications. Octane's goal is to measure the performance of JavaScript code
found in large, real-world web applications.
Octane 2.0 consists of 17 tests, four more than Octane v1.
"""

import os

from metrics import power
from telemetry import benchmark
from telemetry.page import page_measurement
from telemetry.page import page_set
from telemetry.util import statistics
from telemetry.value import scalar


_GB = 1024 * 1024 * 1024


class _OctaneMeasurement(page_measurement.PageMeasurement):
  def __init__(self):
    super(_OctaneMeasurement, self).__init__()
    self._power_metric = None

  def CustomizeBrowserOptions(self, options):
    power.PowerMetric.CustomizeBrowserOptions(options)

  def WillStartBrowser(self, browser):
    self._power_metric = power.PowerMetric(browser)

  def WillNavigateToPage(self, page, tab):
    if tab.browser.memory_stats['SystemTotalPhysicalMemory'] < 1 * _GB:
      skipBenchmarks = '"zlib"'
    else:
      skipBenchmarks = ''
    page.script_to_evaluate_on_commit = """
        var __results = [];
        var __real_log = window.console.log;
        window.console.log = function(msg) {
          __results.push(msg);
          __real_log.apply(this, [msg]);
        }
        skipBenchmarks = [%s]
        """ % (skipBenchmarks)

  def DidNavigateToPage(self, page, tab):
    self._power_metric.Start(page, tab)

  def MeasurePage(self, page, tab, results):
    tab.WaitForJavaScriptExpression(
        'completed && !document.getElementById("progress-bar-container")', 1200)

    self._power_metric.Stop(page, tab)
    self._power_metric.AddResults(tab, results)

    results_log = tab.EvaluateJavaScript('__results')
    all_scores = []
    for output in results_log:
      # Split the results into score and test name.
      # results log e.g., "Richards: 18343"
      score_and_name = output.split(': ', 2)
      assert len(score_and_name) == 2, \
        'Unexpected result format "%s"' % score_and_name
      if 'Skipped' not in score_and_name[1]:
        name = score_and_name[0]
        score = int(score_and_name[1])
        results.AddValue(scalar.ScalarValue(
            results.current_page, name, 'score', score, important=False))

        # Collect all test scores to compute geometric mean.
        all_scores.append(score)
    total = statistics.GeometricMean(all_scores)
    results.AddSummaryValue(
        scalar.ScalarValue(None, 'Total.Score', 'score', total))


class Octane(benchmark.Benchmark):
  """Google's Octane JavaScript benchmark."""
  test = _OctaneMeasurement

  def CreatePageSet(self, options):
    ps = page_set.PageSet(
      archive_data_file='../page_sets/data/octane.json',
      make_javascript_deterministic=False,
      file_path=os.path.abspath(__file__))
    ps.AddPageWithDefaultRunNavigate(
      'http://octane-benchmark.googlecode.com/svn/latest/index.html?auto=1')
    return ps
