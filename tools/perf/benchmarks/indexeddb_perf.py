# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Chromium's IndexedDB performance test. These test:

Databases:
  create/delete
Keys:
  create/delete
Indexes:
  create/delete
Data access:
  Random read/write
  Sporadic writes
  Read cache
Cursors:
  Read & random writes
  Walking multiple
  Seeking.
"""

import json
import os

from metrics import memory
from metrics import power
from metrics import v8_object_stats
from telemetry import test
from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set

_V8_COUNTER_NAMES = [
    'V8.OsMemoryAllocated',
  ]

class _IndexedDbMeasurement(page_measurement.PageMeasurement):
  def __init__(self, *args, **kwargs):
    super(_IndexedDbMeasurement, self).__init__(*args, **kwargs)
    self._memory_metric = None
    self._power_metric = power.PowerMetric()
    self._v8_object_stats_metric = None

  def DidStartBrowser(self, browser):
    """Initialize metrics once right after the browser has been launched."""
    self._memory_metric = memory.MemoryMetric(browser)
    self._v8_object_stats_metric = (
      v8_object_stats.V8ObjectStatsMetric(_V8_COUNTER_NAMES))

  def DidNavigateToPage(self, page, tab):
    self._memory_metric.Start(page, tab)
    self._power_metric.Start(page, tab)
    self._v8_object_stats_metric.Start(page, tab)

  def MeasurePage(self, page, tab, results):
    tab.WaitForDocumentReadyStateToBeComplete()
    tab.WaitForJavaScriptExpression(
        'window.document.cookie.indexOf("__done=1") >= 0', 600)

    self._power_metric.Stop(page, tab)
    self._memory_metric.Stop(page, tab)
    self._v8_object_stats_metric.Stop(page, tab)

    self._memory_metric.AddResults(tab, results)
    self._power_metric.AddResults(tab, results)
    self._v8_object_stats_metric.AddResults(tab, results)

    js_get_results = "JSON.stringify(automation.getResults());"
    result_dict = json.loads(tab.EvaluateJavaScript(js_get_results))
    total = 0.0
    for key in result_dict:
      if key == 'OverallTestDuration':
        continue
      msec = float(result_dict[key])
      results.Add(key, 'ms', msec, data_type='unimportant')
      total += msec
    results.Add('Total Perf', 'ms', total)

  def CustomizeBrowserOptions(self, options):
    memory.MemoryMetric.CustomizeBrowserOptions(options)
    power.PowerMetric.CustomizeBrowserOptions(options)
    v8_object_stats.V8ObjectStatsMetric.CustomizeBrowserOptions(options)

class IndexedDb(test.Test):
  """Chromium's IndexedDB Performance tests."""
  test = _IndexedDbMeasurement

  def CreatePageSet(self, options):
    indexeddb_dir = os.path.join(util.GetChromiumSrcDir(), 'chrome', 'test',
                                 'data', 'indexeddb')
    return page_set.PageSet.FromDict({
        'pages': [
            { 'url': 'file://perf_test.html' }
          ]
        }, indexeddb_dir)
