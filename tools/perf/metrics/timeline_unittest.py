# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from metrics import timeline
from telemetry.core.timeline import model as model_module
from telemetry.page import page as page_module
from telemetry.page import page_measurement_results
from telemetry.value import scalar

class TestPageMeasurementResults(
    page_measurement_results.PageMeasurementResults):
  def __init__(self, test):
    super(TestPageMeasurementResults, self).__init__()
    self.test = test
    page = page_module.Page("http://www.google.com", {})
    self.WillMeasurePage(page)

  def GetPageSpecificValueNamed(self, name):
    values = [value for value in self.all_page_specific_values
         if value.name == name]
    assert len(values) == 1, 'Could not find value named %s' % name
    return values[0]

  def AssertHasPageSpecificScalarValue(self, name, units, expected_value):
    value = self.GetPageSpecificValueNamed(name)
    self.test.assertEquals(units, value.units)
    self.test.assertTrue(isinstance(value, scalar.ScalarValue))
    self.test.assertEquals(expected_value, value.value)

  def __str__(self):
    return '\n'.join([repr(x) for x in self.all_page_specific_values])

class ThreadTimesTimelineMetricUnittest(unittest.TestCase):
  def GetResultsForModel(self, metric, model):
    metric.model = model
    results = TestPageMeasurementResults(self)
    tab = None
    metric.AddResults(tab, results)
    return results

  def testBasic(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # [      X       ]
    #      [  Y  ]
    renderer_main.BeginSlice('cat1', 'X', 10, 0)
    renderer_main.BeginSlice('cat2', 'Y', 15, 5)
    renderer_main.EndSlice(16, 0.5)
    renderer_main.EndSlice(30, 19.5)
    model.FinalizeImport()

    metric = timeline.ThreadTimesTimelineMetric()
    metric.report_renderer_main_details = True
    results = self.GetResultsForModel(metric, model)
    results.AssertHasPageSpecificScalarValue(
      'thread_renderer_main_clock_time_percentage', '%', 100)
    results.AssertHasPageSpecificScalarValue(
      'renderer_main|cat1', 'ms', 19)
    results.AssertHasPageSpecificScalarValue(
      'renderer_main|cat2', 'ms', 1)
    results.AssertHasPageSpecificScalarValue(
      'renderer_main|idle', 'ms', 0)

  def testPercentage(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # [      X       ]   [  Z  ]
    # X is 20ms, then 10ms delay, then Z is 10ms
    renderer_main.BeginSlice('cat', 'X', 10, 0)
    renderer_main.EndSlice(30, 19.5)
    renderer_main.BeginSlice('cat', 'Z', 40, 20)
    renderer_main.EndSlice(50, 30)
    model.FinalizeImport()

    metric = timeline.ThreadTimesTimelineMetric()
    metric.report_renderer_main_details = True
    results = self.GetResultsForModel(metric, model)

    results.AssertHasPageSpecificScalarValue(
      'thread_renderer_main_clock_time_percentage', '%', 75)
    results.AssertHasPageSpecificScalarValue(
      'renderer_main|cat', 'ms', 30)
    results.AssertHasPageSpecificScalarValue(
      'renderer_main|idle', 'ms', 10)
