# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Apple's SunSpider JavaScript benchmark."""

import collections
import json
import os

from telemetry.core import util
from telemetry.page import page_measurement
from telemetry.page import page_set

class SunSpiderMeasurement(page_measurement.PageMeasurement):
  def CreatePageSet(self, _, options):
    return page_set.PageSet.FromDict({
        'serving_dirs': ['../../../chrome/test/data/sunspider/'],
        'pages': [
          { 'url': 'file:///../../../chrome/test/data/sunspider/'
                   'sunspider-1.0/driver.html' }
          ]
        }, os.path.abspath(__file__))

  def MeasurePage(self, _, tab, results):
    js_is_done = """
window.location.pathname.indexOf('results.html') >= 0"""
    def _IsDone():
      return tab.EvaluateJavaScript(js_is_done)
    util.WaitFor(_IsDone, 300, poll_interval=5)

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
