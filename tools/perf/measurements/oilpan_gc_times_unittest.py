# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from measurements import oilpan_gc_times
from telemetry.unittest_util import options_for_unittests
from telemetry.unittest_util import page_test_test_case

class OilpanGCTimesTest(page_test_test_case.PageTestTestCase):
  """Smoke test for Oilpan GC pause time measurements.

     Runs OilpanGCTimes measurement on some simple pages and verifies
     that all metrics were added to the results.  The test is purely functional,
     i.e. it only checks if the metrics are present and non-zero.
  """
  def setUp(self):
    self._options = options_for_unittests.GetCopy()

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
