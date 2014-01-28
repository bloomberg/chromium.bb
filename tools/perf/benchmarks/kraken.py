# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Mozilla's Kraken JavaScript benchmark."""

import os

from metrics import power
from telemetry import test
from telemetry.page import page_measurement
from telemetry.page import page_set


def _Mean(l):
  return float(sum(l)) / len(l) if len(l) > 0 else 0.0


class _KrakenMeasurement(page_measurement.PageMeasurement):
  def __init__(self):
    super(_KrakenMeasurement, self).__init__()
    self._power_metric = power.PowerMetric()

  def CustomizeBrowserOptions(self, options):
    power.PowerMetric.CustomizeBrowserOptions(options)

  def DidNavigateToPage(self, page, tab):
    self._power_metric.Start(page, tab)

  def MeasurePage(self, page, tab, results):
    tab.WaitForDocumentReadyStateToBeComplete()
    tab.WaitForJavaScriptExpression(
        'document.title.indexOf("Results") != -1', 500)

    self._power_metric.Stop(page, tab)
    self._power_metric.AddResults(tab, results)

    js_get_results = """
var formElement = document.getElementsByTagName("input")[0];
decodeURIComponent(formElement.value.split("?")[1]);
"""
    result_dict = eval(tab.EvaluateJavaScript(js_get_results))
    total = 0
    for key in result_dict:
      if key == 'v':
        continue
      results.Add(key, 'ms', result_dict[key], data_type='unimportant')
      total += _Mean(result_dict[key])
    results.Add('Total', 'ms', total)


class Kraken(test.Test):
  """Mozilla's Kraken JavaScript benchmark."""
  test = _KrakenMeasurement

  def CreatePageSet(self, options):
    return page_set.PageSet.FromDict({
        'archive_data_file': '../page_sets/data/kraken.json',
        'pages': [
          { 'url': 'http://krakenbenchmark.mozilla.org/kraken-1.1/driver.html' }
          ]
        }, os.path.abspath(__file__))
