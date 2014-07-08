# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Facebook's JSGameBench benchmark.

As of May 14, 2014, JSGameBench is no longer maintained. See README.md:
https://github.com/facebookarchive/jsgamebench

The benchmark is kept here for historical purposes but is disabled on the bots.
"""

import os

from telemetry import benchmark
from telemetry.page import page_measurement
from telemetry.page import page_set
from telemetry.value import scalar


class _JsgamebenchMeasurement(page_measurement.PageMeasurement):
  def __init__(self):
    super(_JsgamebenchMeasurement, self).__init__()

  def MeasurePage(self, page, tab, results):
    tab.ExecuteJavaScript('UI.call({}, "perftest")')
    tab.WaitForJavaScriptExpression(
        'document.getElementById("perfscore0") != null', 1800)

    js_get_results = 'document.getElementById("perfscore0").innerHTML'
    result = int(tab.EvaluateJavaScript(js_get_results))
    results.AddValue(scalar.ScalarValue(
        results.current_page, 'Score', 'score (bigger is better)', result))


@benchmark.Disabled
class Jsgamebench(benchmark.Benchmark):
  """Counts how many animating sprites can move around on the screen at once."""
  test = _JsgamebenchMeasurement

  def CreatePageSet(self, options):
    ps = page_set.PageSet(
      archive_data_file='../page_sets/data/jsgamebench.json',
      file_path=os.path.dirname(__file__))
    ps.AddPageWithDefaultRunNavigate('http://localhost/')
    return ps
