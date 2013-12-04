# Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

from metrics import statistics
from telemetry import test
from telemetry.page import page_measurement
from telemetry.page import page_set


class _OctaneMeasurement(page_measurement.PageMeasurement):

  def WillNavigateToPage(self, page, tab):
    page.script_to_evaluate_on_commit = """
        var __results = [];
        var __real_log = window.console.log;
        window.console.log = function(msg) {
          __results.push(msg);
          __real_log.apply(this, [msg]);
        }
        """

  def MeasurePage(self, _, tab, results):
    tab.WaitForJavaScriptExpression(
        'completed && !document.getElementById("progress-bar-container")', 1200)

    results_log = tab.EvaluateJavaScript('__results')
    all_scores = []
    for output in results_log:
      # Split the results into score and test name.
      # results log e.g., "Richards: 18343"
      score_and_name = output.split(': ', 2)
      assert len(score_and_name) == 2, \
        'Unexpected result format "%s"' % score_and_name
      name = score_and_name[0]
      score = int(score_and_name[1])
      results.Add(name, 'score', score, data_type='unimportant')
      # Collect all test scores to compute geometric mean.
      all_scores.append(score)
    total = statistics.GeometricMean(all_scores)
    results.AddSummary('Score', 'score', total, chart_name='Total')


class Octane(test.Test):
  """Google's Octane JavaScript benchmark."""
  test = _OctaneMeasurement

  def CreatePageSet(self, options):
    return page_set.PageSet.FromDict({
      'archive_data_file': '../page_sets/data/octane.json',
      'make_javascript_deterministic': False,
      'pages': [{
          'url':
          'http://octane-benchmark.googlecode.com/svn/latest/index.html?auto=1'
          }
        ]
      }, os.path.abspath(__file__))

