# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import math
import os

from telemetry import benchmark
from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set
from telemetry.value import merge_values
from telemetry.value import scalar


def _GeometricMean(values):
  """Compute a rounded geometric mean from an array of values."""
  if not values:
    return None
  # To avoid infinite value errors, make sure no value is less than 0.001.
  new_values = []
  for value in values:
    if value > 0.001:
      new_values.append(value)
    else:
      new_values.append(0.001)
  # Compute the sum of the log of the values.
  log_sum = sum(map(math.log, new_values))
  # Raise e to that sum over the number of values.
  mean = math.pow(math.e, (log_sum / len(new_values)))
  # Return the rounded mean.
  return int(round(mean))


SCORE_UNIT = 'score (bigger is better)'
SCORE_TRACE_NAME = 'score'


class _DomPerfMeasurement(page_measurement.PageMeasurement):
  def MeasurePage(self, page, tab, results):
    try:
      def _IsDone():
        return tab.GetCookieByName('__domperf_finished') == '1'
      util.WaitFor(_IsDone, 600)

      data = json.loads(tab.EvaluateJavaScript('__domperf_result'))
      for suite in data['BenchmarkSuites']:
        # Skip benchmarks that we didn't actually run this time around.
        if len(suite['Benchmarks']) or suite['score']:
          results.AddValue(scalar.ScalarValue(
              results.current_page, '%s.%s' % (suite['name'], SCORE_TRACE_NAME),
              SCORE_UNIT, suite['score'], important=False))
    finally:
      tab.EvaluateJavaScript('document.cookie = "__domperf_finished=0"')

  def DidRunTest(self, browser, results):
    # Now give the geometric mean as the total for the combined runs.
    combined = merge_values.MergeLikeValuesFromDifferentPages(
        results.all_page_specific_values,
        group_by_name_suffix=True)
    combined_score = [x for x in combined if x.name == SCORE_TRACE_NAME][0]
    total = _GeometricMean(combined_score.values)
    results.AddSummaryValue(
        scalar.ScalarValue(None, 'Total.' + SCORE_TRACE_NAME, SCORE_UNIT,
                           total))


@benchmark.Disabled('android', 'linux')
class DomPerf(benchmark.Benchmark):
  """A suite of JavaScript benchmarks for exercising the browser's DOM.

  The final score is computed as the geometric mean of the individual results.
  Scores are not comparable across benchmark suite versions and higher scores
  means better performance: Bigger is better!"""
  test = _DomPerfMeasurement

  def CreatePageSet(self, options):
    dom_perf_dir = os.path.join(util.GetChromiumSrcDir(), 'data', 'dom_perf')
    run_params = [
      'Accessors',
      'CloneNodes',
      'CreateNodes',
      'DOMDivWalk',
      'DOMTable',
      'DOMWalk',
      'Events',
      'Get+Elements',
      'GridSort',
      'Template'
    ]
    ps = page_set.PageSet(file_path=dom_perf_dir)
    for param in run_params:
      ps.AddPageWithDefaultRunNavigate(
        'file://run.html?reportInJS=1&run=%s' % param)
    return ps
