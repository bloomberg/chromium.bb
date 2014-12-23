# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import repaint
from telemetry import decorators
from telemetry.core import wpr_modes
from telemetry.page import page as page_module
from telemetry.unittest_util import options_for_unittests
from telemetry.unittest_util import page_test_test_case


class TestRepaintPage(page_module.Page):
  def __init__(self, page_set, base_dir):
    super(TestRepaintPage, self).__init__('file://blank.html',
                                          page_set, base_dir)

  def RunPageInteractions(self, action_runner):
    action_runner.RepaintContinuously(seconds=2)


class RepaintUnitTest(page_test_test_case.PageTestTestCase):
  """Smoke test for repaint measurement

     Runs repaint measurement on a simple page and verifies
     that all metrics were added to the results. The test is purely functional,
     i.e. it only checks if the metrics are present and non-zero.
  """

  def setUp(self):
    self._options = options_for_unittests.GetCopy()
    self._options.browser_options.wpr_mode = wpr_modes.WPR_OFF

  def testRepaint(self):
    ps = self.CreateEmptyPageSet()
    ps.AddUserStory(TestRepaintPage(ps, ps.base_dir))
    measurement = repaint.Repaint()
    results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertEquals(0, len(results.failures))

    frame_times = results.FindAllPageSpecificValuesNamed('frame_times')
    self.assertEquals(len(frame_times), 1)
    self.assertGreater(frame_times[0].GetRepresentativeNumber(), 0)

    mean_frame_time = results.FindAllPageSpecificValuesNamed('mean_frame_time')
    self.assertEquals(len(mean_frame_time), 1)
    self.assertGreater(mean_frame_time[0].GetRepresentativeNumber(), 0)

    frame_time_discrepancy = results.FindAllPageSpecificValuesNamed(
        'frame_time_discrepancy')
    self.assertEquals(len(frame_time_discrepancy), 1)
    self.assertGreater(frame_time_discrepancy[0].GetRepresentativeNumber(), 0)

    percentage_smooth = results.FindAllPageSpecificValuesNamed(
        'percentage_smooth')
    self.assertEquals(len(percentage_smooth), 1)
    self.assertGreaterEqual(percentage_smooth[0].GetRepresentativeNumber(), 0)

  @decorators.Disabled('android')
  def testCleanUpTrace(self):
    self.TestTracingCleanedUp(repaint.Repaint, self._options)
