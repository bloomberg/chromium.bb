# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from metrics import test_page_measurement_results
from metrics import timeline
from telemetry.timeline import model as model_module
from telemetry.web_perf import timeline_interaction_record as tir_module

def _GetInteractionRecord(start, end):
  return tir_module.TimelineInteractionRecord("test-record", start, end)


class LoadTimesTimelineMetric(unittest.TestCase):
  def GetResults(self, metric, model, renderer_thread, interaction_records):
    results = test_page_measurement_results.TestPageMeasurementResults(self)
    metric.AddResults(model, renderer_thread, interaction_records, results)
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

    metric = timeline.LoadTimesTimelineMetric()
    results = self.GetResults(
      metric, model=model, renderer_thread=renderer_main,
      interaction_records=[_GetInteractionRecord(0, float('inf'))])
    results.AssertHasPageSpecificScalarValue(
      'CrRendererMain|x_y', 'ms', 10)
    results.AssertHasPageSpecificScalarValue(
      'CrRendererMain|x_y_max', 'ms', 10)
    results.AssertHasPageSpecificScalarValue(
      'CrRendererMain|x_y_avg', 'ms', 10)

  def testTimelineBetweenRange(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    #   [          X         ]    [       Z      ]
    #           [  Y  ]               [   T    ]
    #   [ interaction record ]
    renderer_main.BeginSlice('cat1', 'x.y', 10, 0)
    renderer_main.EndSlice(20, 20)
    renderer_main.BeginSlice('cat1', 'z.t', 30, 0)
    renderer_main.EndSlice(35, 35)
    model.FinalizeImport()

    metric = timeline.LoadTimesTimelineMetric()
    results = self.GetResults(
      metric, model=model, renderer_thread=renderer_main,
      interaction_records=[_GetInteractionRecord(10, 20)])
    results.AssertHasPageSpecificScalarValue(
      'CrRendererMain|x_y', 'ms', 10)
    results.AssertHasPageSpecificScalarValue(
      'CrRendererMain|x_y_max', 'ms', 10)
    results.AssertHasPageSpecificScalarValue(
      'CrRendererMain|x_y_avg', 'ms', 10)


  def testCounterSanitizing(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    x_counter = renderer_main.parent.GetOrCreateCounter('cat', 'x.y')
    x_counter.samples += [1, 2]
    x_counter.series_names += ['a']
    x_counter.timestamps += [0, 1]
    model.FinalizeImport()

    metric = timeline.LoadTimesTimelineMetric()
    results = self.GetResults(
      metric, model=model, renderer_thread=renderer_main,
      interaction_records=[_GetInteractionRecord(0, float('inf'))])
    results.AssertHasPageSpecificScalarValue(
      'cat_x_y', 'count', 3)
    results.AssertHasPageSpecificScalarValue(
      'cat_x_y_avg', 'count', 1.5)


class ThreadTimesTimelineMetricUnittest(unittest.TestCase):
  def GetResults(self, metric, model, renderer_thread, interaction_record):
    results = test_page_measurement_results.TestPageMeasurementResults(self)
    metric.AddResults(model, renderer_thread, interaction_record,
                                results)
    return results

  def testResults(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    metric = timeline.ThreadTimesTimelineMetric()
    metric.details_to_report = timeline.ReportMainThreadOnly
    results = self.GetResults(metric, model, renderer_main.parent,
                              [_GetInteractionRecord(1,2)])

    # Test that all result thread categories exist
    for name in timeline.TimelineThreadCategories.values():
      results.GetPageSpecificValueNamed(timeline.ThreadCpuTimeResultName(name))

  def testBasic(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # Create two frame swaps (Results times should be divided by two)
    cc_main = model.GetOrCreateProcess(1).GetOrCreateThread(3)
    cc_main.name = 'Compositor'
    cc_main.BeginSlice('cc_cat', timeline.FrameTraceName, 10, 10)
    cc_main.EndSlice(11, 11)
    cc_main.BeginSlice('cc_cat', timeline.FrameTraceName, 12, 12)
    cc_main.EndSlice(13, 13)

    # [      X       ]   [ Z ]
    #      [  Y  ]
    renderer_main.BeginSlice('cat1', 'X', 10, 0)
    renderer_main.BeginSlice('cat2', 'Y', 15, 5)
    renderer_main.EndSlice(16, 5.5)
    renderer_main.EndSlice(30, 19.5)
    renderer_main.BeginSlice('cat1', 'Z', 31, 20)
    renderer_main.BeginSlice('cat1', 'Z', 33, 21)
    model.FinalizeImport()

    # Exclude 'Z' using an action-range.
    metric = timeline.ThreadTimesTimelineMetric()
    metric.details_to_report = timeline.ReportMainThreadOnly
    results = self.GetResults(metric, model, renderer_main.parent,
                              [_GetInteractionRecord(10, 30)])


    # Test a couple specific results.
    assert_results = {
      timeline.ThreadCpuTimeResultName('renderer_main') : 9.75,
      timeline.ThreadDetailResultName('renderer_main','cat1') : 9.5,
      timeline.ThreadDetailResultName('renderer_main','cat2') : 0.5,
      timeline.ThreadDetailResultName('renderer_main','idle') : 0
    }
    for name, value in assert_results.iteritems():
      results.AssertHasPageSpecificScalarValue(name, 'ms', value)

  def testOverheadIsRemoved(self):
    model = model_module.TimelineModel()
    renderer_main = model.GetOrCreateProcess(1).GetOrCreateThread(2)
    renderer_main.name = 'CrRendererMain'

    # Create one frame swap.
    cc_main = model.GetOrCreateProcess(1).GetOrCreateThread(3)
    cc_main.name = 'Compositor'
    cc_main.BeginSlice('cc_cat', timeline.FrameTraceName, 10, 10)
    cc_main.EndSlice(11, 11)

    # [      X       ]
    #    [Overhead]
    overhead_category = timeline.OverheadTraceCategory
    overhead_name = timeline.OverheadTraceName
    renderer_main.BeginSlice('cat1', 'X', 10, 0)
    renderer_main.BeginSlice(overhead_category, overhead_name, 15, 5)
    renderer_main.EndSlice(16, 6)
    renderer_main.EndSlice(30, 10)
    model.FinalizeImport()

    # Include everything in an action-range.
    metric = timeline.ThreadTimesTimelineMetric()
    metric.details_to_report = timeline.ReportMainThreadOnly
    results = self.GetResults(metric, model, renderer_main.parent,
                              [_GetInteractionRecord(10, 30)])

    # Test a couple specific results.
    assert_results = {
      timeline.ThreadCpuTimeResultName('renderer_main') : 9.0,
    }
    for name, value in assert_results.iteritems():
      results.AssertHasPageSpecificScalarValue(name, 'ms', value)
