# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Apple's Speedometer performance benchmark.

Speedometer measures simulated user interactions in web applications.

The current benchmark uses TodoMVC to simulate user actions for adding,
completing, and removing to-do items. Speedometer repeats the same actions using
DOM APIs - a core set of web platform APIs used extensively in web applications-
as well as six popular JavaScript frameworks: Ember.js, Backbone.js, jQuery,
AngularJS, React, and Flight. Many of these frameworks are used on the most
popular websites in the world, such as Facebook and Twitter. The performance of
these types of operations depends on the speed of the DOM APIs, the JavaScript
engine, CSS style resolution, layout, and other technologies.
"""

import os

from telemetry import benchmark
from telemetry.page import page_set
from telemetry.page import page_test
from telemetry.value import list_of_scalar_values


class SpeedometerMeasurement(page_test.PageTest):
  enabled_suites = [
    'VanillaJS-TodoMVC',
    'EmberJS-TodoMVC',
    'BackboneJS-TodoMVC',
    'jQuery-TodoMVC',
    'AngularJS-TodoMVC',
    'React-TodoMVC',
    'FlightJS-TodoMVC'
  ]

  def ValidateAndMeasurePage(self, page, tab, results):
    tab.WaitForDocumentReadyStateToBeComplete()
    iterationCount = 10
    # A single iteration on android takes ~75 seconds, the benchmark times out
    # when running for 10 iterations.
    if tab.browser.platform.GetOSName() == 'android':
      iterationCount = 3

    tab.ExecuteJavaScript("""
        // Store all the results in the benchmarkClient
        benchmarkClient._measuredValues = []
        benchmarkClient.didRunSuites = function(measuredValues) {
          benchmarkClient._measuredValues.push(measuredValues);
          benchmarkClient._timeValues.push(measuredValues.total);
        };
        benchmarkClient.iterationCount = %d;
        startTest();
        """ % iterationCount)
    tab.WaitForJavaScriptExpression(
        'benchmarkClient._finishedTestCount == benchmarkClient.testsCount', 600)
    results.AddValue(list_of_scalar_values.ListOfScalarValues(
        page, 'Total', 'ms',
        tab.EvaluateJavaScript('benchmarkClient._timeValues'), important=True))

    # Extract the timings for each suite
    for suite_name in self.enabled_suites:
      results.AddValue(list_of_scalar_values.ListOfScalarValues(
          page, suite_name, 'ms',
          tab.EvaluateJavaScript("""
              var suite_times = [];
              for(var i = 0; i < benchmarkClient.iterationCount; i++) {
                suite_times.push(
                    benchmarkClient._measuredValues[i].tests['%s'].total);
              };
              suite_times;
              """ % suite_name), important=False))

class Speedometer(benchmark.Benchmark):
  test = SpeedometerMeasurement

  def CreatePageSet(self, options):
    ps = page_set.PageSet(
        file_path=os.path.abspath(__file__),
        archive_data_file='../page_sets/data/speedometer.json',
        make_javascript_deterministic=False)
    ps.AddPageWithDefaultRunNavigate('http://browserbench.org/Speedometer/')
    return ps
