# Copyright 2013 The Chromium Authors. All rights reserved.
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

class LoadTimesTimelineMetric(unittest.TestCase):
  def GetResultsForModel(self, metric, model):
    metric.model = model
    results = TestPageMeasurementResults(self)
    tab = None
    metric.AddResults(tab, results)
    return results

  def testSanitizing(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # [      X       ]
    #      [  Y  ]
    renderer_main.BeginSlice('cat1', 'x.y', 10, 0)
    renderer_main.EndSlice(20, 20)
    model.FinalizeImport()

    metric = timeline.LoadTimesTimelineMetric(timeline.TRACING_MODE)
    metric.renderer_process = renderer_main.parent
    results = self.GetResultsForModel(metric, model)
    results.AssertHasPageSpecificScalarValue(
      'CrRendererMain|x_y', 'ms', 10)
    results.AssertHasPageSpecificScalarValue(
      'CrRendererMain|x_y_max', 'ms', 10)
    results.AssertHasPageSpecificScalarValue(
      'CrRendererMain|x_y_avg', 'ms', 10)


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

    # Create two frame swaps (Results times should be divided by two)
    cc_main = model.GetOrCreateProcess(1).GetOrCreateThread(3)
    cc_main.name = 'Compositor'
    cc_main.BeginSlice('cc_cat', timeline.CompositorFrameTraceName, 10, 10)
    cc_main.EndSlice(11, 11)
    cc_main.BeginSlice('cc_cat', timeline.CompositorFrameTraceName, 12, 12)
    cc_main.EndSlice(13, 13)

    # [      X       ]
    #      [  Y  ]
    renderer_main.BeginSlice('cat1', 'X', 10, 0)
    renderer_main.BeginSlice('cat2', 'Y', 15, 5)
    renderer_main.EndSlice(16, 0.5)
    renderer_main.EndSlice(30, 19.5)
    model.FinalizeImport()

    metric = timeline.ThreadTimesTimelineMetric()
    metric.details_to_report = timeline.MainThread
    results = self.GetResultsForModel(metric, model)

    # Test that all categories exist
    for name in timeline.TimelineThreadCategories.values():
      results.GetPageSpecificValueNamed(timeline.ThreadTimeResultName(name))
      results.GetPageSpecificValueNamed(timeline.ThreadCpuTimeResultName(name))

    # Test a couple specific results.
    assert_results = {
      timeline.ThreadTimeResultName('renderer_main') : 10,
      timeline.ThreadDetailResultName('renderer_main','cat1') : 9.5,
      timeline.ThreadDetailResultName('renderer_main','cat2') : 0.5,
      timeline.ThreadDetailResultName('renderer_main','idle') : 0
    }
    for name, value in assert_results.iteritems():
      results.AssertHasPageSpecificScalarValue(name, 'ms', value)
