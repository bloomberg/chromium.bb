# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import math
import os

from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set


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


class DomPerf(page_measurement.PageMeasurement):
  def CreatePageSet(self, _, options):
    BASE_PAGE = 'file:///../../../data/dom_perf/run.html?reportInJS=1&run='
    return page_set.PageSet.FromDict({
        'pages': [
          { 'url': BASE_PAGE + 'Accessors' },
          { 'url': BASE_PAGE + 'CloneNodes' },
          { 'url': BASE_PAGE + 'CreateNodes' },
          { 'url': BASE_PAGE + 'DOMDivWalk' },
          { 'url': BASE_PAGE + 'DOMTable' },
          { 'url': BASE_PAGE + 'DOMWalk' },
          { 'url': BASE_PAGE + 'Events' },
          { 'url': BASE_PAGE + 'Get+Elements' },
          { 'url': BASE_PAGE + 'GridSort' },
          { 'url': BASE_PAGE + 'Template' }
          ]
        }, os.path.abspath(__file__))

  @property
  def results_are_the_same_on_every_page(self):
    return False

  def MeasurePage(self, page, tab, results):
    try:
      def _IsDone():
        return tab.GetCookieByName('__domperf_finished') == '1'
      util.WaitFor(_IsDone, 600, poll_interval=5)

      data = json.loads(tab.EvaluateJavaScript('__domperf_result'))
      for suite in data['BenchmarkSuites']:
        # Skip benchmarks that we didn't actually run this time around.
        if len(suite['Benchmarks']) or suite['score']:
          results.Add(SCORE_TRACE_NAME, SCORE_UNIT,
                      suite['score'], suite['name'], 'unimportant')
    finally:
      tab.EvaluateJavaScript('document.cookie = "__domperf_finished=0"')

  def DidRunPageSet(self, tab, results):
    # Now give the geometric mean as the total for the combined runs.
    scores = []
    for result in results.page_results:
      scores.append(result[SCORE_TRACE_NAME].output_value)
    total = _GeometricMean(scores)
    results.AddSummary(SCORE_TRACE_NAME, SCORE_UNIT, total, 'Total')
