# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""DoYouEvenBench performance benchmark .

DoYouEvenBench performance benchmark UI performance related to DOM using
popular JS frameworks. This benchmark uses http://todomvc.com and emulates user
actions: adding 100 todo items, completing them, and then deleting
them with Ember.js, Backbone.js, jQuery, and plain-old DOM.
"""

import os

from telemetry import test
from telemetry.page import page_measurement
from telemetry.page import page_set
from telemetry.value import scalar


class DoYouEvenBenchMeasurement(page_measurement.PageMeasurement):

  def MeasurePage(self, _, tab, results):
    tab.WaitForDocumentReadyStateToBeComplete()
    # Tests are ran 5 iteration, results are displayed in the form of HTML
    # table. Each row represents timetaken in that iteration and adds rows for
    # Arithmetic Mean and 95th Percentile. Total 7 rows in the results table.
    tab.WaitForJavaScriptExpression(
        'document.getElementsByTagName("tr").length >= 7', 200)
    # Parse results and result for each run including mean and 95th percentile.
    js_results = """ function JsResults() {
        var _result = {}
        _rows = document.getElementsByTagName("tr");
        for(var i=0; i< _rows.length; i++) {
          if (_rows[i].cells[0].innerText.indexOf("95th Percentile") == -1) {
            _result[_rows[i].cells[0].innerText] = _rows[i].cells[1].innerText;
          }
        }
        return _result;
    }; JsResults();"""
    results_log = tab.EvaluateJavaScript(js_results)
    for name, value in results_log.iteritems():
      score = float(value.replace('ms', '').strip())
      # Add time taken for each iteration to run tests.
      if name != 'Arithmetic Mean':
        results.Add(name, 'ms', score, data_type='unimportant')
      else:
        results.AddSummaryValue(scalar.ScalarValue(None, name, 'ms', score))

class DoYouEvenBench(test.Test):
  """DoYouEvenBench benchmark related to DOMs using JS frameworks."""
  test = DoYouEvenBenchMeasurement

  def CreatePageSet(self, options):
    ps = page_set.PageSet(
        file_path=os.path.abspath(__file__),
        archive_data_file='../page_sets/data/doyouevenbench.json',
        make_javascript_deterministic=False)
    ps.AddPageWithDefaultRunNavigate(
        'https://trac.webkit.org/export/164157/trunk/'
        'PerformanceTests/DoYouEvenBench/Full.html')
    return ps
