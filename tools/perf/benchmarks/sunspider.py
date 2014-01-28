# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import collections
import json
import os

from metrics import power
from telemetry import test
from telemetry.page import page_measurement
from telemetry.page import page_set


_URL = 'http://www.webkit.org/perf/sunspider-1.0.2/sunspider-1.0.2/driver.html'


class _SunspiderMeasurement(page_measurement.PageMeasurement):
  def __init__(self):
    super(_SunspiderMeasurement, self).__init__()
    self._power_metric = power.PowerMetric()

  def CustomizeBrowserOptions(self, options):
    power.PowerMetric.CustomizeBrowserOptions(options)

  def DidNavigateToPage(self, page, tab):
    self._power_metric.Start(page, tab)

  def MeasurePage(self, page, tab, results):
    tab.WaitForJavaScriptExpression(
        'window.location.pathname.indexOf("results.html") >= 0'
        '&& typeof(output) != "undefined"', 300)

    self._power_metric.Stop(page, tab)
    self._power_metric.AddResults(tab, results)

    js_get_results = 'JSON.stringify(output);'
    js_results = json.loads(tab.EvaluateJavaScript(js_get_results))
    r = collections.defaultdict(list)
    totals = []
    # js_results is: [{'foo': v1, 'bar': v2},
    #                 {'foo': v3, 'bar': v4},
    #                 ...]
    for result in js_results:
      total = 0
      for key, value in result.iteritems():
        r[key].append(value)
        total += value
      totals.append(total)
    for key, values in r.iteritems():
      results.Add(key, 'ms', values, data_type='unimportant')
    results.Add('Total', 'ms', totals)


class Sunspider(test.Test):
  """Apple's SunSpider JavaScript benchmark."""
  test = _SunspiderMeasurement

  def CreatePageSet(self, options):
    return page_set.PageSet.FromDict(
        {
          'archive_data_file': '../page_sets/data/sunspider.json',
          'make_javascript_deterministic': False,
          'pages': [{ 'url': _URL }],
        }, os.path.abspath(__file__))
