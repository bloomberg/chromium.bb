# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""For all the benchmarks that set options, test that the options are valid."""

import os
import unittest

from telemetry import benchmark as benchmark_module
from telemetry.core import browser_options
from telemetry.core import discover
from telemetry.unittest_util import progress_reporter


def _GetPerfDir(*subdirs):
  perf_dir = os.path.dirname(os.path.dirname(__file__))
  return os.path.join(perf_dir, *subdirs)


def _BenchmarkOptionsTestGenerator(benchmark):
  def testBenchmarkOptions(self):  # pylint: disable=W0613
    """Invalid options will raise benchmark.InvalidOptionsError."""
    options = browser_options.BrowserFinderOptions()
    parser = options.CreateParser()
    benchmark.AddCommandLineArgs(parser)
    benchmark_module.AddCommandLineArgs(parser)
    benchmark.SetArgumentDefaults(parser)
  return testBenchmarkOptions


def _AddBenchmarkOptionsTests(suite):
  # Using |index_by_class_name=True| allows returning multiple benchmarks
  # from a module.
  all_benchmarks = discover.DiscoverClasses(
      _GetPerfDir('benchmarks'), _GetPerfDir(), benchmark_module.Benchmark,
      index_by_class_name=True).values()
  for benchmark in all_benchmarks:
    if not benchmark.options:
      # No need to test benchmarks that have not defined options.
      continue
    class BenchmarkOptionsTest(unittest.TestCase):
      pass
    setattr(BenchmarkOptionsTest, benchmark.Name(),
            _BenchmarkOptionsTestGenerator(benchmark))
    suite.addTest(BenchmarkOptionsTest(benchmark.Name()))


def load_tests(_, _2, _3):
  suite = progress_reporter.TestSuite()
  _AddBenchmarkOptionsTests(suite)
  return suite
