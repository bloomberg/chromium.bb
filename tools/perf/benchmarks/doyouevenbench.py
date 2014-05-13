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
    # Click Run button on http://rniwa.com/DoYouEvenBench to start test.
    tab.ExecuteJavaScript('document.getElementsByTagName("button")[1].click();')
    # <PRE> tag stores the results once the tests are completed.
    tab.WaitForJavaScriptExpression(
        'document.getElementsByTagName("pre").length > 0', 200)
    # Parse results
    results_log = tab.EvaluateJavaScript(
        'document.getElementsByTagName("pre")[0].innerHTML')
    res_log = results_log.splitlines()
    for res in res_log:
      name_and_score = res.split(': ', 2)
      assert len(name_and_score) == 2, 'Unexpected result format "%s"' % res
      # Replace '/' with '_' for quantity being measure otherwise it asserts
      # while printing results to stdout.
      name = name_and_score[0].replace('/', '_').strip()
      score = float(name_and_score[1].replace('ms', '').strip())
      # Since Total consists total time taken by all tests, add its values
      # to Summary.
      if 'Total' not in name:
        results.Add(name, 'ms', score, data_type='unimportant')
      else:
        results.AddSummaryValue(scalar.ScalarValue(None, 'Total', 'ms', score))


class DoYouEvenBench(test.Test):
  """DoYouEvenBench benchmark related to DOMs using JS frameworks."""
  test = DoYouEvenBenchMeasurement

  def CreatePageSet(self, options):
    return page_set.PageSet.FromDict({
        'archive_data_file': '../page_sets/data/doyouevenbench.json',
        'make_javascript_deterministic': False,
        'pages': [
            {'url': 'http://rniwa.com/DoYouEvenBench/'}
        ]
    }, os.path.abspath(__file__))

