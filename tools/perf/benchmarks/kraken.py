# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Mozilla's Kraken JavaScript benchmark."""

import os

from metrics import power
from telemetry import benchmark
from telemetry.page import page_measurement
from telemetry.page import page_set
from telemetry.value import list_of_scalar_values
from telemetry.value import scalar


def _Mean(l):
  return float(sum(l)) / len(l) if len(l) > 0 else 0.0


class _KrakenMeasurement(page_measurement.PageMeasurement):
  def __init__(self):
    super(_KrakenMeasurement, self).__init__()
    self._power_metric = None

  def CustomizeBrowserOptions(self, options):
    power.PowerMetric.CustomizeBrowserOptions(options)

  def WillStartBrowser(self, browser):
    self._power_metric = power.PowerMetric(browser)

  def DidNavigateToPage(self, page, tab):
    self._power_metric.Start(page, tab)

  def MeasurePage(self, page, tab, results):
    tab.WaitForJavaScriptExpression(
        'document.title.indexOf("Results") != -1', 700)
    tab.WaitForDocumentReadyStateToBeComplete()

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
      results.AddValue(list_of_scalar_values.ListOfScalarValues(
          results.current_page, key, 'ms', result_dict[key], important=False))
      total += _Mean(result_dict[key])

    # TODO(tonyg/nednguyen): This measurement shouldn't calculate Total. The
    # results system should do that for us.
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'Total', 'ms', total))


class Kraken(benchmark.Benchmark):
  """Mozilla's Kraken JavaScript benchmark."""
  test = _KrakenMeasurement

  def CreatePageSet(self, options):
    ps = page_set.PageSet(
      archive_data_file='../page_sets/data/kraken.json',
      file_path=os.path.abspath(__file__))
    ps.AddPageWithDefaultRunNavigate(
      'http://krakenbenchmark.mozilla.org/kraken-1.1/driver.html')
    return ps
