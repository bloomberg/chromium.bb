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

from telemetry import test
from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set


class _IndexedDbMeasurement(page_measurement.PageMeasurement):
  def MeasurePage(self, _, tab, results):
    tab.WaitForDocumentReadyStateToBeComplete()
    tab.WaitForJavaScriptExpression(
        'window.document.cookie.indexOf("__done=1") >= 0', 600)

    js_get_results = "JSON.stringify(automation.getResults());"
    result_dict = json.loads(tab.EvaluateJavaScript(js_get_results))
    total = 0.0
    for key in result_dict:
      if key == 'OverallTestDuration':
        continue
      msec = float(result_dict[key])
      results.Add(key, 'ms', msec, data_type='unimportant')
      total += msec
    results.Add('Total', 'ms', total)

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
