# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import test
from telemetry.page import page_measurement


class _ServiceWorkerMeasurement(page_measurement.PageMeasurement):
  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs([
        '--enable-experimental-web-platform-features',
        '--enable-service-worker'
      ])

  def MeasurePage(self, _, tab, results):
    tab.WaitForJavaScriptExpression('window.done', 40)
    json = tab.EvaluateJavaScript('window.results')
    for key, value in json.iteritems():
      results.Add(key, value['units'], value['value'])


class ServiceWorkerPerfTest(test.Test):
  test = _ServiceWorkerMeasurement
  page_set = 'page_sets/service_worker.py'
