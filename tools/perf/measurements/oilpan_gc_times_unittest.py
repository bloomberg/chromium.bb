# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import oilpan_gc_times

from telemetry.results import page_test_results
from telemetry.timeline import model
from telemetry.timeline import slice as slice_data
from telemetry.timeline.event import TimelineEvent
from telemetry.unittest_util import options_for_unittests
from telemetry.unittest_util import page_test_test_case
from telemetry.page import page as page_module


class TestOilpanGCTimesPage(page_module.Page):
  def __init__(self, page_set):
    super(TestOilpanGCTimesPage, self).__init__(
        'file://blank.html', page_set, page_set.base_dir)

  def RunPageInteractions(self, action_runner):
    interaction = action_runner.BeginGestureInteraction('ScrollAction')
    action_runner.ScrollPage()
    interaction.End()


class OilpanGCTimesTestData(object):
  def __init__(self, thread_name):
    self._model = model.TimelineModel()
    self._renderer_process = self._model.GetOrCreateProcess(1)
    self._renderer_thread = self._renderer_process.GetOrCreateThread(2)
    self._renderer_thread.name = thread_name
    self._results = page_test_results.PageTestResults()

  @property
  def results(self):
    return self._results

  def AddSlice(self, name, timestamp, duration, args):
    new_slice = slice_data.Slice(
        None,
        'category',
        name,
        timestamp,
        duration,
        timestamp,
        duration,
        args)
    self._renderer_thread.all_slices.append(new_slice)
    return new_slice

  def ClearResults(self):
    self._results = page_test_results.PageTestResults()


class OilpanGCTimesTest(page_test_test_case.PageTestTestCase):
  """Smoke test for Oilpan GC pause time measurements.

     Runs OilpanGCTimes measurement on some simple pages and verifies
     that all metrics were added to the results.  The test is purely functional,
     i.e. it only checks if the metrics are present and non-zero.
  """
  _KEY_MARK = 'Heap::collectGarbage'
  _KEY_LAZY_SWEEP = 'ThreadHeap::lazySweepPages'
  _KEY_COMPLETE_SWEEP = 'ThreadState::completeSweep'
  _KEY_COALESCE = 'ThreadHeap::coalesce'

  def setUp(self):
    self._options = options_for_unittests.GetCopy()

  def testForParsingOldFormat(self):
    def getMetric(results, name):
      metrics = results.FindAllPageSpecificValuesNamed(name)
      self.assertEquals(1, len(metrics))
      return metrics[0].GetBuildbotValue()

    data = self._GenerateDataForParsingOldFormat()

    measurement = oilpan_gc_times._OilpanGCTimesBase()
    measurement._renderer_process = data._renderer_process
    measurement._timeline_model = data._model
    measurement.ValidateAndMeasurePage(None, None, data.results)

    results = data.results
    self.assertEquals(7, len(getMetric(results, 'oilpan_coalesce')))
    self.assertEquals(3, len(getMetric(results, 'oilpan_precise_mark')))
    self.assertEquals(3, len(getMetric(results, 'oilpan_precise_lazy_sweep')))
    self.assertEquals(3, len(getMetric(results,
                                       'oilpan_precise_complete_sweep')))
    self.assertEquals(1, len(getMetric(results, 'oilpan_conservative_mark')))
    self.assertEquals(1, len(getMetric(results,
                                       'oilpan_conservative_lazy_sweep')))
    self.assertEquals(1, len(getMetric(results,
                                       'oilpan_conservative_complete_sweep')))
    self.assertEquals(2, len(getMetric(results, 'oilpan_forced_mark')))
    self.assertEquals(2, len(getMetric(results, 'oilpan_forced_lazy_sweep')))
    self.assertEquals(2, len(getMetric(results,
                                       'oilpan_forced_complete_sweep')))

  def testForParsing(self):
    def getMetric(results, name):
      metrics = results.FindAllPageSpecificValuesNamed(name)
      self.assertEquals(1, len(metrics))
      return metrics[0].GetBuildbotValue()

    data = self._GenerateDataForParsing()

    measurement = oilpan_gc_times._OilpanGCTimesBase()
    measurement._renderer_process = data._renderer_process
    measurement._timeline_model = data._model
    measurement.ValidateAndMeasurePage(None, None, data.results)

    results = data.results
    self.assertEquals(7, len(getMetric(results, 'oilpan_coalesce')))
    self.assertEquals(2, len(getMetric(results, 'oilpan_precise_mark')))
    self.assertEquals(2, len(getMetric(results, 'oilpan_precise_lazy_sweep')))
    self.assertEquals(2, len(getMetric(results,
                                       'oilpan_precise_complete_sweep')))
    self.assertEquals(2, len(getMetric(results, 'oilpan_conservative_mark')))
    self.assertEquals(2, len(getMetric(results,
                                       'oilpan_conservative_lazy_sweep')))
    self.assertEquals(2, len(getMetric(results,
                                       'oilpan_conservative_complete_sweep')))
    self.assertEquals(1, len(getMetric(results, 'oilpan_forced_mark')))
    self.assertEquals(1, len(getMetric(results, 'oilpan_forced_lazy_sweep')))
    self.assertEquals(1, len(getMetric(results,
                                       'oilpan_forced_complete_sweep')))
    self.assertEquals(1, len(getMetric(results, 'oilpan_idle_mark')))
    self.assertEquals(1, len(getMetric(results, 'oilpan_idle_lazy_sweep')))
    self.assertEquals(1, len(getMetric(results,
                                       'oilpan_idle_complete_sweep')))

  def testForSmoothness(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('create_many_objects.html')
    measurement = oilpan_gc_times.OilpanGCTimesForSmoothness()
    results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertEquals(0, len(results.failures))

    precise = results.FindAllPageSpecificValuesNamed('oilpan_precise_mark')
    conservative = results.FindAllPageSpecificValuesNamed(
        'oilpan_conservative_mark')
    self.assertLess(0, len(precise) + len(conservative))

  def testForBlinkPerf(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('create_many_objects.html')
    measurement = oilpan_gc_times.OilpanGCTimesForBlinkPerf()
    results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertEquals(0, len(results.failures))

    precise = results.FindAllPageSpecificValuesNamed('oilpan_precise_mark')
    conservative = results.FindAllPageSpecificValuesNamed(
        'oilpan_conservative_mark')
    self.assertLess(0, len(precise) + len(conservative))

  def _GenerateDataForEmptyPageSet(self):
    page_set = self.CreateEmptyPageSet()
    page = TestOilpanGCTimesPage(page_set)
    page_set.AddUserStory(page)

    data = OilpanGCTimesTestData('CrRendererMain')
    # Pretend we are about to run the tests to silence lower level asserts.
    data.results.WillRunPage(page)

    return data

  def _GenerateDataForParsingOldFormat(self):
    data = self._GenerateDataForEmptyPageSet()
    data.AddSlice(self._KEY_MARK, 1, 1, {'precise': True, 'forced': False})
    data.AddSlice(self._KEY_LAZY_SWEEP, 2, 2, {})
    data.AddSlice(self._KEY_COALESCE, 4, 3, {})
    data.AddSlice(self._KEY_LAZY_SWEEP, 7, 4, {})
    data.AddSlice(self._KEY_COALESCE, 11, 5, {})
    data.AddSlice(self._KEY_COMPLETE_SWEEP, 16, 6, {})
    data.AddSlice(self._KEY_MARK, 22, 7, {'precise': True, 'forced': False})
    data.AddSlice(self._KEY_LAZY_SWEEP, 29, 8, {})
    data.AddSlice(self._KEY_COALESCE, 37, 9, {})
    data.AddSlice(self._KEY_COMPLETE_SWEEP, 46, 10, {})
    data.AddSlice(self._KEY_MARK, 56, 11, {'precise': False, 'forced': False})
    data.AddSlice(self._KEY_LAZY_SWEEP, 67, 12, {})
    data.AddSlice(self._KEY_COALESCE, 79, 13, {})
    data.AddSlice(self._KEY_COMPLETE_SWEEP, 92, 14, {})
    data.AddSlice(self._KEY_MARK, 106, 15, {'precise': True, 'forced': False})
    data.AddSlice(self._KEY_LAZY_SWEEP, 121, 16, {})
    data.AddSlice(self._KEY_COALESCE, 137, 17, {})
    data.AddSlice(self._KEY_COMPLETE_SWEEP, 154, 18, {})
    data.AddSlice(self._KEY_MARK, 172, 19, {'precise': False, 'forced': True})
    data.AddSlice(self._KEY_COALESCE, 191, 20, {})
    data.AddSlice(self._KEY_LAZY_SWEEP, 211, 21, {})
    data.AddSlice(self._KEY_COMPLETE_SWEEP, 232, 22, {})
    data.AddSlice(self._KEY_MARK, 254, 23, {'precise': True, 'forced': True})
    data.AddSlice(self._KEY_COALESCE, 277, 24, {})
    data.AddSlice(self._KEY_LAZY_SWEEP, 301, 25, {})
    data.AddSlice(self._KEY_COMPLETE_SWEEP, 326, 26, {})
    return data

  def _GenerateDataForParsing(self):
    data = self._GenerateDataForEmptyPageSet()
    data.AddSlice(self._KEY_MARK, 1, 1,
                  {'lazySweeping': True, 'gcReason': 'ConservativeGC'})
    data.AddSlice(self._KEY_LAZY_SWEEP, 2, 2, {})
    data.AddSlice(self._KEY_COALESCE, 4, 3, {})
    data.AddSlice(self._KEY_LAZY_SWEEP, 7, 4, {})
    data.AddSlice(self._KEY_COALESCE, 11, 5, {})
    data.AddSlice(self._KEY_COMPLETE_SWEEP, 16, 6, {})
    data.AddSlice(self._KEY_MARK, 22, 7,
                  {'lazySweeping': True, 'gcReason': 'PreciseGC'})
    data.AddSlice(self._KEY_LAZY_SWEEP, 29, 8, {})
    data.AddSlice(self._KEY_COALESCE, 37, 9, {})
    data.AddSlice(self._KEY_COMPLETE_SWEEP, 46, 10, {})
    data.AddSlice(self._KEY_MARK, 56, 11,
                  {'lazySweeping': False, 'gcReason': 'ConservativeGC'})
    data.AddSlice(self._KEY_LAZY_SWEEP, 67, 12, {})
    data.AddSlice(self._KEY_COALESCE, 79, 13, {})
    data.AddSlice(self._KEY_COMPLETE_SWEEP, 92, 14, {})
    data.AddSlice(self._KEY_MARK, 106, 15,
                  {'lazySweeping': False, 'gcReason': 'PreciseGC'})
    data.AddSlice(self._KEY_LAZY_SWEEP, 121, 16, {})
    data.AddSlice(self._KEY_COALESCE, 137, 17, {})
    data.AddSlice(self._KEY_COMPLETE_SWEEP, 154, 18, {})
    data.AddSlice(self._KEY_MARK, 172, 19,
                  {'lazySweeping': False, 'gcReason': 'ForcedGCForTesting'})
    data.AddSlice(self._KEY_COALESCE, 191, 20, {})
    data.AddSlice(self._KEY_LAZY_SWEEP, 211, 21, {})
    data.AddSlice(self._KEY_COMPLETE_SWEEP, 232, 22, {})
    data.AddSlice(self._KEY_MARK, 254, 23,
                  {'lazySweeping': False, 'gcReason': 'IdleGC'})
    data.AddSlice(self._KEY_COALESCE, 277, 24, {})
    data.AddSlice(self._KEY_LAZY_SWEEP, 301, 25, {})
    data.AddSlice(self._KEY_COMPLETE_SWEEP, 326, 26, {})
    return data
