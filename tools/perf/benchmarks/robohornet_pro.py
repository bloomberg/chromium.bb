# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Microsoft's RoboHornet Pro benchmark."""

import os

from metrics import power
from telemetry import benchmark
from telemetry.page import page_set
from telemetry.page import page_test
from telemetry.value import scalar


class _RobohornetProMeasurement(page_test.PageTest):
  def __init__(self):
    super(_RobohornetProMeasurement, self).__init__()
    self._power_metric = None

  def CustomizeBrowserOptions(self, options):
    power.PowerMetric.CustomizeBrowserOptions(options)

  def WillStartBrowser(self, browser):
    self._power_metric = power.PowerMetric(browser)

  def DidNavigateToPage(self, page, tab):
    self._power_metric.Start(page, tab)

  def ValidateAndMeasurePage(self, page, tab, results):
    tab.ExecuteJavaScript('ToggleRoboHornet()')
    tab.WaitForJavaScriptExpression(
        'document.getElementById("results").innerHTML.indexOf("Total") != -1',
        600)

    self._power_metric.Stop(page, tab)
    self._power_metric.AddResults(tab, results)

    result = int(tab.EvaluateJavaScript('stopTime - startTime'))
    results.AddValue(
        scalar.ScalarValue(results.current_page, 'Total', 'ms', result))



class RobohornetPro(benchmark.Benchmark):
  test = _RobohornetProMeasurement

  def CreatePageSet(self, options):
    ps = page_set.PageSet(
      archive_data_file='../page_sets/data/robohornet_pro.json',
      # Measurement require use of real Date.now() for measurement.
      make_javascript_deterministic=False,
      file_path=os.path.abspath(__file__))
    ps.AddPageWithDefaultRunNavigate(
      'http://ie.microsoft.com/testdrive/performance/robohornetpro/')
    return ps
