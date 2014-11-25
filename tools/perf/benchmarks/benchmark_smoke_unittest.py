# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run the first page of one benchmark for every module.

Only benchmarks that have a composable measurement are included.
Ideally this test would be comprehensive, however, running one page
of every benchmark would run impractically long.
"""

import os
import unittest

from telemetry import benchmark as benchmark_module
from telemetry.core import discover
from telemetry.page import page_test
from telemetry.unittest_util import options_for_unittests
from telemetry.unittest_util import progress_reporter


def SmokeTestGenerator(benchmark):
  # NOTE TO SHERIFFS: DO NOT DISABLE THIS TEST.
  #
  # This smoke test dynamically tests all benchmarks. So disabling it for one
  # failing or flaky benchmark would disable a much wider swath of coverage
  # than is usally intended. Instead, if a particular benchmark is failing,
  # disable it in tools/perf/benchmarks/*.
  @benchmark_module.Disabled('chromeos')  # crbug.com/351114
  def BenchmarkSmokeTest(self):
    # Only measure a single page so that this test cycles reasonably quickly.
    benchmark.options['pageset_repeat'] = 1
    benchmark.options['page_repeat'] = 1

    class SinglePageBenchmark(benchmark):  # pylint: disable=W0232
      def CreatePageSet(self, options):
        # pylint: disable=E1002
        ps = super(SinglePageBenchmark, self).CreatePageSet(options)
        for p in ps.pages:
          p.skip_waits = True
          ps.user_stories = [p]
          break
        return ps

    # Set the benchmark's default arguments.
    options = options_for_unittests.GetCopy()
    options.output_format = 'none'
    options.suppress_gtest_report = True
    parser = options.CreateParser()

    benchmark.AddCommandLineArgs(parser)
    benchmark_module.AddCommandLineArgs(parser)
    benchmark.SetArgumentDefaults(parser)
    options.MergeDefaultValues(parser.get_default_values())

    benchmark.ProcessCommandLineArgs(None, options)
    benchmark_module.ProcessCommandLineArgs(None, options)

    self.assertEqual(0, SinglePageBenchmark().Run(options),
                     msg='Failed: %s' % benchmark)

  return BenchmarkSmokeTest


def load_tests(_, _2, _3):
  suite = progress_reporter.TestSuite()

  benchmarks_dir = os.path.dirname(__file__)
  top_level_dir = os.path.dirname(benchmarks_dir)
  measurements_dir = os.path.join(top_level_dir, 'measurements')

  all_measurements = discover.DiscoverClasses(
      measurements_dir, top_level_dir, page_test.PageTest).values()
  # Using the default of |index_by_class_name=False| means that if a module
  # has multiple benchmarks, only the last one is returned.
  all_benchmarks = discover.DiscoverClasses(
      benchmarks_dir, top_level_dir, benchmark_module.Benchmark,
      index_by_class_name=False).values()
  for benchmark in all_benchmarks:
    if hasattr(benchmark, 'test') and benchmark.test not in all_measurements:
      # If the benchmark does not have a measurement, then it is not composable.
      # Ideally we'd like to test these as well, but the non-composable
      # benchmarks are usually long-running benchmarks.
      continue

    # TODO(tonyg): Smoke doesn't work with session_restore yet.
    if (benchmark.Name().startswith('session_restore') or
        benchmark.Name().startswith('skpicture_printer')):
      continue

    if hasattr(benchmark, 'generated_profile_archive'):
      # We'd like to test these, but don't know how yet.
      continue

    class BenchmarkSmokeTest(unittest.TestCase):
      pass

    method = SmokeTestGenerator(benchmark)

    # Make sure any decorators are propagated from the original declaration.
    # (access to protected members) pylint: disable=W0212
    # TODO(dpranke): Since we only pick the first test from every class
    # (above), if that test is disabled, we'll end up not running *any*
    # test from the class. We should probably discover all of the tests
    # in a class, and then throw the ones we don't need away instead.
    if hasattr(benchmark, '_enabled_strings'):
      method._enabled_strings = benchmark._enabled_strings
    if hasattr(benchmark, '_disabled_strings'):
      method._disabled_strings = benchmark._disabled_strings
    setattr(BenchmarkSmokeTest, benchmark.Name(), method)

    suite.addTest(BenchmarkSmokeTest(benchmark.Name()))

  return suite
