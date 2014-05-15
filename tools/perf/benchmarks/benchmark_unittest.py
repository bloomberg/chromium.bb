# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run the first page of every benchmark that has a composable measurement.

Ideally this test would be comprehensive, but the above serves as a
kind of smoke test.
"""

import os
import unittest

from telemetry import test
from telemetry.core import discover
from telemetry.page import page_measurement
from telemetry.unittest import options_for_unittests


# In general you should @test.Disabled individual benchmarks that fail,
# instead of this entire smoke test suite.
# TODO(achuith): Multiple tests failing on CrOS. crbug.com/351114
@test.Disabled('android', 'chromeos', 'win')
class BenchmarkSmokeTest(unittest.TestCase):
  """Placeholder class for the tests.

  The members of this class are generated in load_tests, below.
  """


def SmokeTestGenerator(benchmark):
  def SmokeTest(self):
    # Only measure a single page so that this test cycles reasonably quickly.
    benchmark.options['pageset_repeat'] = 1
    benchmark.options['page_repeat'] = 1

    class SinglePageBenchmark(benchmark):  # pylint: disable=W0232
      def CreatePageSet(self, options):
        # pylint: disable=E1002
        ps = super(SinglePageBenchmark, self).CreatePageSet(options)
        ps.pages = ps.pages[:1]
        return ps

    # Set the benchmark's default arguments.
    options = options_for_unittests.GetCopy()
    options.output_format = 'none'
    parser = options.CreateParser()

    benchmark.AddCommandLineArgs(parser)
    test.AddCommandLineArgs(parser)
    benchmark.SetArgumentDefaults(parser)
    options.MergeDefaultValues(parser.get_default_values())

    benchmark.ProcessCommandLineArgs(None, options)
    test.ProcessCommandLineArgs(None, options)

    self.assertEqual(0, SinglePageBenchmark().Run(options),
                     msg='Failed: %s' % benchmark)

  return SmokeTest


def load_tests(_, _2, _3):
  suite = unittest.TestSuite()

  benchmarks_dir = os.path.dirname(__file__)
  top_level_dir = os.path.dirname(benchmarks_dir)
  measurements_dir = os.path.join(top_level_dir, 'measurements')

  all_measurements = discover.DiscoverClasses(
      measurements_dir, top_level_dir, page_measurement.PageMeasurement,
      pattern='*.py').values()
  all_benchmarks = discover.DiscoverClasses(
      benchmarks_dir, top_level_dir, test.Test, pattern='*.py').values()

  for benchmark in all_benchmarks:
    if benchmark.PageTestClass() not in all_measurements:
      # If the benchmark is not in measurements, then it is not composable.
      # Ideally we'd like to test these as well, but the non-composable
      # benchmarks are usually long-running benchmarks.
      continue

    if hasattr(benchmark, 'generated_profile_archive'):
      # We'd like to test these, but don't know how yet.
      continue

    setattr(BenchmarkSmokeTest, benchmark.Name(), SmokeTestGenerator(benchmark))
    suite.addTest(BenchmarkSmokeTest(benchmark.Name()))

  return suite
