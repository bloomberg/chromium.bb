# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import unittest

from telemetry import test
from telemetry.core import discover
from telemetry.page import page_measurement
from telemetry.unittest import options_for_unittests


class MeasurementUnitTest(unittest.TestCase):

  def testMeasurementSmoke(self):
    # Run all Measurements against the first Page in the PageSet of the first
    # Benchmark that uses them.
    #
    # Ideally this test would be comprehensive, but the above serves as a
    # kind of smoke test.
    measurements_dir = os.path.dirname(__file__)
    top_level_dir = os.path.dirname(measurements_dir)
    benchmarks_dir = os.path.join(top_level_dir, 'benchmarks')

    all_measurements = discover.DiscoverClasses(
        measurements_dir, top_level_dir, page_measurement.PageMeasurement,
        pattern='*.py').values()
    all_benchmarks = discover.DiscoverClasses(
        benchmarks_dir, top_level_dir, test.Test, pattern='*.py').values()

    for benchmark in all_benchmarks:
      if benchmark.test not in all_measurements:
        # If the benchmark is not in measurements, then it is not composable.
        # Ideally we'd like to test these as well, but the non-composable
        # benchmarks are usually long-running benchmarks.
        continue

      if hasattr(benchmark, 'generated_profile_archive'):
        # We'd like to test these, but don't know how yet.
        continue

      # Only measure a single page so that this test cycles reasonably quickly.
      benchmark.options['pageset_repeat_iters'] = 1
      benchmark.options['page_repeat_iters'] = 1
      benchmark.options['page_repeat_secs'] = 1

      class SinglePageBenchmark(benchmark):  # pylint: disable=W0232
        def CreatePageSet(self, options):
          # pylint: disable=E1002
          ps = super(SinglePageBenchmark, self).CreatePageSet(options)
          ps.pages = ps.pages[:1]
          return ps

      logging.info('running: %s', benchmark)
      options = options_for_unittests.GetCopy()
      options.output_format = 'none'
      self.assertEqual(0, SinglePageBenchmark().Run(options))
