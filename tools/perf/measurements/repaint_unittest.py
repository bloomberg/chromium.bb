# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import repaint
from telemetry.core import wpr_modes
from telemetry.page import page_measurement_unittest_base
from telemetry.page import page as page_module
from telemetry.unittest import options_for_unittests
from telemetry.unittest import test


class TestRepaintPage(page_module.Page):
  def __init__(self, page_set, base_dir):
    super(TestRepaintPage, self).__init__('file://blank.html',
                                          page_set, base_dir)

  def RunRepaint(self, action_runner):
    action_runner.RepaintContinuously(seconds=2)


class RepaintUnitTest(
      page_measurement_unittest_base.PageMeasurementUnitTestBase):
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
    ps.AddPage(TestRepaintPage(ps, ps.base_dir))
    measurement = repaint.Repaint()
    results = self.RunMeasurement(measurement, ps, options=self._options)
    self.assertEquals(0, len(results.failures))

    frame_times = results.FindAllPageSpecificValuesNamed('frame_times')
    self.assertEquals(len(frame_times), 1)
    self.assertGreater(frame_times[0].GetRepresentativeNumber(), 0)

    mean_frame_time = results.FindAllPageSpecificValuesNamed('mean_frame_time')
    self.assertEquals(len(mean_frame_time), 1)
    self.assertGreater(mean_frame_time[0].GetRepresentativeNumber(), 0)

    jank = results.FindAllPageSpecificValuesNamed('jank')
    self.assertEquals(len(jank), 1)
    self.assertGreater(jank[0].GetRepresentativeNumber(), 0)

    mostly_smooth = results.FindAllPageSpecificValuesNamed('mostly_smooth')
    self.assertEquals(len(mostly_smooth), 1)
    self.assertGreaterEqual(mostly_smooth[0].GetRepresentativeNumber(), 0)

  @test.Disabled('android')
  def testCleanUpTrace(self):
    self.TestTracingCleanedUp(repaint.Repaint, self._options)
