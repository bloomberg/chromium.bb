# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import task_execution_time
from telemetry import decorators
from telemetry.core import wpr_modes
from telemetry.page import page as page_module
from telemetry.results import page_test_results
from telemetry.timeline import model as model_module
from telemetry.timeline import slice as slice_data
from telemetry.unittest_util import options_for_unittests
from telemetry.unittest_util import page_test_test_case


class TestTaskExecutionTimePage(page_module.Page):

  def __init__(self, page_set, base_dir):
    super(TestTaskExecutionTimePage, self).__init__(
        'file://blank.html', page_set, base_dir)

  def RunPageInteractions(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class TaskExecutionTimeUnitTest(page_test_test_case.PageTestTestCase):

  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.browser_options.wpr_mode = wpr_modes.WPR_OFF

  @decorators.Disabled('win')
  def testSomeResultsReturnedFromDummyPage(self):
    ps = self.CreateEmptyPageSet()
    ps.AddUserStory(TestTaskExecutionTimePage(ps, ps.base_dir))
    measurement = task_execution_time.TaskExecutionTime()

    results = self.RunMeasurement(measurement, ps, options=self._options)

    self.assertGreater(len(results.all_page_specific_values), 0)

  @decorators.Disabled('win')
  def testSlicesConformToRequiredNamingConventionsUsingDummyPage(self):
    """This test ensures the presence of required keywords.

       Some arbitrary keywords are required to generate the names of the top 10
       tasks. The code has a weak dependancy on 'src_func', 'class' and 'line'
       existing; if they exist in a slice's args they are used to generate a
       name, if they don't exists the code falls back to using the name of the
       slice, which is less clear.

       If the code has been refactored and these keywords no longer exist
       the code that relies on them in task_execution_time.py should be
       updated to use the appropriate technique for assertaining this data
       (and this test changed in the same way).
    """
    ps = self.CreateEmptyPageSet()
    ps.AddUserStory(TestTaskExecutionTimePage(ps, ps.base_dir))
    measurement = task_execution_time.TaskExecutionTime()

    self.RunMeasurement(measurement, ps, options=self._options)

    required_keywords = {'src_func': 0, 'class': 0, 'line': 0}

    # Check all slices and count the uses of the required keywords.
    for thread in measurement._renderer_process.threads.values():
      for slice_info in thread.IterAllSlices():
        _CheckSliceForKeywords(slice_info, required_keywords)

    # Confirm that all required keywords have at least one instance.
    for use_counts in required_keywords.values():
      self.assertGreater(use_counts, 0)

  @decorators.Disabled('win')
  def testMockedResults(self):
    task_execution_time_metric = task_execution_time.TaskExecutionTime()
    ps = self.CreateEmptyPageSet()

    # Get the name of a thread used by task_execution_time metric and set up
    # some dummy execution data pretending to be from that thread & process.
    first_thread_name = task_execution_time_metric._RENDERER_THREADS[0]
    data = TaskExecutionTestData(first_thread_name)
    task_execution_time_metric._renderer_process = data._renderer_process

    # Pretend we're about to run the tests to silence lower level asserts.
    data.results.WillRunPage(TestTaskExecutionTimePage(ps, ps.base_dir))
    data.AddSlice('fast', 0, 1)
    data.AddSlice('medium', 0, 500)
    data.AddSlice('slow', 0, 1000)

    # Run the code we are testing.
    task_execution_time_metric.ValidateAndMeasurePage(None, None, data.results)

    # Confirm we get back 3 results that are correctly sorted and named
    self.assertEqual(len(data.results.all_page_specific_values), 3)
    self.assertEqual(
        data.results.all_page_specific_values[0].name,
        'process 1:' + first_thread_name + ':slow')
    self.assertEqual(
        data.results.all_page_specific_values[1].name,
        'process 1:' + first_thread_name + ':medium')
    self.assertEqual(
        data.results.all_page_specific_values[2].name,
        'process 1:' + first_thread_name + ':fast')
    self.assertEqual(data.results.all_page_specific_values[0].value, 1000)
    self.assertEqual(data.results.all_page_specific_values[1].value, 500)
    self.assertEqual(data.results.all_page_specific_values[2].value, 1)


def _CheckSliceForKeywords(slice_info, required_keywords):
  for argument in slice_info.args:
    if argument in required_keywords:
      required_keywords[argument] += 1
  # recurse into our sub-slices
  for sub_slice in slice_info.sub_slices:
    _CheckSliceForKeywords(sub_slice, required_keywords)


class TaskExecutionTestData(object):

  def __init__(self, thread_name):
    self._model = model_module.TimelineModel()
    self._renderer_process = self._model.GetOrCreateProcess(1)
    self._renderer_thread = self._renderer_process.GetOrCreateThread(2)
    self._renderer_thread.name = thread_name
    self._results = page_test_results.PageTestResults()
    self._metric = None
    self._ps = None

  @property
  def results(self):
    return self._results

  @property
  def metric(self):
    return self._metric

  def AddSlice(self, name, timestamp, duration):
    self._renderer_thread.all_slices.append(
        slice_data.Slice(
            None,
            'category',
            name,
            timestamp,
            duration,
            timestamp,
            duration,
            []))
