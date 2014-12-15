# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import task_execution_time
from telemetry.core import wpr_modes
from telemetry.page import page as page_module
from telemetry.unittest_util import options_for_unittests
from telemetry.unittest_util import page_test_test_case


class TestTaskExecutionTimePage(page_module.Page):
  def __init__(self, page_set, base_dir):
    super(TestTaskExecutionTimePage, self).__init__(
        'file://blank.html', page_set, base_dir)

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class TaskExecutionTimeUnitTest(page_test_test_case.PageTestTestCase):
  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.browser_options.wpr_mode = wpr_modes.WPR_OFF

  def testCorrectNumberOfResultsReturned(self):
    ps = self.CreateEmptyPageSet()
    ps.AddUserStory(TestTaskExecutionTimePage(ps, ps.base_dir))
    measurement = task_execution_time.TaskExecutionTime()

    results = self.RunMeasurement(measurement, ps, options=self._options)

    self.assertEquals(
        task_execution_time.TaskExecutionTime.GetExpectedResultCount(),
        len(results.all_page_specific_values))

  def testResultsAreDecreasing(self):
    ps = self.CreateEmptyPageSet()
    ps.AddUserStory(TestTaskExecutionTimePage(ps, ps.base_dir))
    measurement = task_execution_time.TaskExecutionTime()

    results = self.RunMeasurement(measurement, ps, options=self._options)

    for first, second in zip(
        results.all_page_specific_values, results.all_page_specific_values[1:]):
      self.assertGreaterEqual(first.value, second.value)
