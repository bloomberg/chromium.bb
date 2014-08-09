# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Canvasmark HTML5, Canvas 2D rendering and javascript benchmark.

CanvasMark tests the HTML5 <canvas> rendering performance for commonly used
operations in HTML5 games: bitmaps, canvas drawing, alpha blending, polygon
fills, shadows and text functions.
"""

import os

from telemetry import benchmark
from telemetry.page import page_set
from telemetry.page import page_test
from telemetry.value import scalar


class _CanvasMarkMeasurement(page_test.PageTest):

  def WillNavigateToPage(self, page, tab):
    page.script_to_evaluate_on_commit = """
        var __results = [];
        var __real_log = window.console.log;
        window.console.log = function(msg) {
          __results.push(msg);
          __real_log.apply(this, [msg]);
        }
        """

  def ValidateAndMeasurePage(self, _, tab, results):
    tab.WaitForJavaScriptExpression('__results.length == 8', 300)
    results_log = tab.EvaluateJavaScript('__results')
    total = 0
    for output in results_log:
      # Split the results into score and test name.
      # results log e.g., "489 [Test 1 - Asteroids - Bitmaps]"
      score_and_name = output.split(' [', 2)
      assert len(score_and_name) == 2, \
        'Unexpected result format "%s"' % score_and_name
      score = int(score_and_name[0])
      name = score_and_name[1][:-1]
      results.AddValue(scalar.ScalarValue(
          results.current_page, name, 'score', score, important=False))
      # Aggregate total score for all tests.
      total += score
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'Score', 'score', total))


@benchmark.Disabled
class CanvasMark(benchmark.Benchmark):
  test = _CanvasMarkMeasurement

  def CreatePageSet(self, options):
    ps = page_set.PageSet(
      file_path=os.path.abspath(__file__),
      archive_data_file='../page_sets/data/canvasmark.json',
      make_javascript_deterministic=False)
    ps.AddPageWithDefaultRunNavigate(
      'http://www.kevs3d.co.uk/dev/canvasmark/?auto=true')
    return ps
