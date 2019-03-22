# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry import story

from telemetry.value import list_of_scalar_values

from page_sets import press_story

class SpeedometerStory(press_story.PressStory):
  URL='http://browserbench.org/Speedometer/'

  enabled_suites = [
      'VanillaJS-TodoMVC',
      'EmberJS-TodoMVC',
      'BackboneJS-TodoMVC',
      'jQuery-TodoMVC',
      'AngularJS-TodoMVC',
      'React-TodoMVC',
      'FlightJS-TodoMVC'
  ]

  def ExecuteTest(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    iterationCount = 10
    # A single iteration on android takes ~75 seconds, the benchmark times out
    # when running for 10 iterations.
    if action_runner.tab.browser.platform.GetOSName() == 'android':
      iterationCount = 3

    action_runner.ExecuteJavaScript("""
        // Store all the results in the benchmarkClient
        benchmarkClient._measuredValues = []
        benchmarkClient.didRunSuites = function(measuredValues) {
          benchmarkClient._measuredValues.push(measuredValues);
          benchmarkClient._timeValues.push(measuredValues.total);
        };
        benchmarkClient.iterationCount = {{ count }};
        startTest();
        """,
        count=iterationCount)
    action_runner.WaitForJavaScriptCondition(
        'benchmarkClient._finishedTestCount == benchmarkClient.testsCount',
        timeout=600)

  def ParseTestResults(self, action_runner):
    self.AddJavascriptMetricValue(list_of_scalar_values.ListOfScalarValues(
        self, 'Total', 'ms',
        action_runner.EvaluateJavaScript('benchmarkClient._timeValues'),
        important=True))
    self.AddJavascriptMetricValue(list_of_scalar_values.ListOfScalarValues(
        self, 'RunsPerMinute', 'score',
        action_runner.EvaluateJavaScript(
            '[parseFloat(document.getElementById("result-number").innerText)];'
        ),
        important=True))

    # Extract the timings for each suite
    for suite_name in self.enabled_suites:
      self.AddJavascriptMetricValue(list_of_scalar_values.ListOfScalarValues(
          self, suite_name, 'ms',
          action_runner.EvaluateJavaScript("""
              var suite_times = [];
              for(var i = 0; i < benchmarkClient.iterationCount; i++) {
                suite_times.push(
                    benchmarkClient._measuredValues[i].tests[{{ key }}].total);
              };
              suite_times;
              """,
              key=suite_name), important=False))


class SpeedometerStorySet(story.StorySet):
  def __init__(self):
    super(SpeedometerStorySet, self).__init__(
        archive_data_file='data/speedometer.json',
        cloud_storage_bucket=story.PUBLIC_BUCKET)

    self.AddStory(SpeedometerStory(self))
