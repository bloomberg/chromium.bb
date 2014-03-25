# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Facebook's JSGameBench benchmark."""

import os

from metrics import power
from telemetry import test
from telemetry.page import page_measurement
from telemetry.page import page_set


class _JsgamebenchMeasurement(page_measurement.PageMeasurement):
  def __init__(self):
    super(_JsgamebenchMeasurement, self).__init__()
    self._power_metric = power.PowerMetric()

  def CustomizeBrowserOptions(self, options):
    power.PowerMetric.CustomizeBrowserOptions(options)

  def DidNavigateToPage(self, page, tab):
    self._power_metric.Start(page, tab)

  def MeasurePage(self, page, tab, results):
    tab.ExecuteJavaScript('UI.call({}, "perftest")')
    tab.WaitForJavaScriptExpression(
        'document.getElementById("perfscore0") != null', 1800)

    self._power_metric.Stop(page, tab)
    self._power_metric.AddResults(tab, results)

    js_get_results = 'document.getElementById("perfscore0").innerHTML'
    result = int(tab.EvaluateJavaScript(js_get_results))
    results.Add('Score', 'score (bigger is better)', result)


class Jsgamebench(test.Test):
  """Counts how many animating sprites can move around on the screen at once."""
  test = _JsgamebenchMeasurement

  def CreatePageSet(self, options):
    return page_set.PageSet.FromDict({
        'archive_data_file': '../page_sets/data/jsgamebench.json',
        'pages': [
          { 'url': 'http://localhost/' }
          ]
        }, os.path.dirname(__file__))
