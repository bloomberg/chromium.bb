# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import page_sets
from telemetry import benchmark
from telemetry.page import page_measurement
from telemetry.value import scalar


class _ServiceWorkerMeasurement(page_measurement.PageMeasurement):
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-experimental-web-platform-features'
      ])

  def MeasurePage(self, _, tab, results):
    tab.WaitForJavaScriptExpression('window.done', 40)
    json = tab.EvaluateJavaScript('window.results')
    for key, value in json.iteritems():
      results.AddValue(scalar.ScalarValue(
          results.current_page, key, value['units'], value['value']))


@benchmark.Disabled
class ServiceWorkerPerfTest(benchmark.Benchmark):
  test = _ServiceWorkerMeasurement
  page_set = page_sets.ServiceWorkerPageSet
