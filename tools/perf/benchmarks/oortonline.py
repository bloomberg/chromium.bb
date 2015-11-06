# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import page_sets

from core import perf_benchmark
from telemetry import benchmark
from telemetry.page import page_test
from telemetry.value import scalar
from telemetry.value import improvement_direction

class _OortOnlineMeasurement(page_test.PageTest):
  def __init__(self):
    super(_OortOnlineMeasurement, self).__init__()

  def ValidateAndMeasurePage(self, page, tab, results):
    tab.WaitForJavaScriptExpression('window.benchmarkFinished', 1000)
    scores = tab.EvaluateJavaScript('window.benchmarkScore')
    for score in scores:
      valid = score['valid']
      if valid:
        results.AddValue(scalar.ScalarValue(
            results.current_page, score['name'], 'score', score['score'],
            important=True, improvement_direction=improvement_direction.UP))

@benchmark.Disabled('android')
class OortOnline(perf_benchmark.PerfBenchmark):
  """OortOnline benchmark that measures WebGL and V8 performance.
  URL: http://oortonline.gl/#run
  Info: http://v8project.blogspot.de/2015/10/jank-busters-part-one.html
  """
  test = _OortOnlineMeasurement

  @classmethod
  def Name(cls):
    return 'oortonline'

  def CreateStorySet(self, options):
    return page_sets.OortOnlinePageSet()
