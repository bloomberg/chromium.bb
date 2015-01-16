# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""For all the benchmarks that set options, test that the options are valid."""

import logging
import os
import unittest
from collections import defaultdict

from telemetry import benchmark as benchmark_module
from telemetry.core import browser_options
from telemetry.core import discover
from telemetry.unittest_util import progress_reporter


def _GetPerfDir(*subdirs):
  perf_dir = os.path.dirname(os.path.dirname(__file__))
  return os.path.join(perf_dir, *subdirs)


def _GetAllPerfBenchmarks():
  return discover.DiscoverClasses(
      _GetPerfDir('benchmarks'), _GetPerfDir(), benchmark_module.Benchmark,
      index_by_class_name=True).values()

def _BenchmarkOptionsTestGenerator(benchmark):
  def testBenchmarkOptions(self):  # pylint: disable=W0613
    """Invalid options will raise benchmark.InvalidOptionsError."""
    options = browser_options.BrowserFinderOptions()
    parser = options.CreateParser()
    benchmark.AddCommandLineArgs(parser)
    benchmark_module.AddCommandLineArgs(parser)
    benchmark.SetArgumentDefaults(parser)
  return testBenchmarkOptions


class TestNoBenchmarkNamesDuplication(unittest.TestCase):
  def runTest(self):
    all_benchmarks = _GetAllPerfBenchmarks()
    names_to_benchmarks = defaultdict(list)
    for b in all_benchmarks:
      names_to_benchmarks[b.Name()].append(b)
    for n in names_to_benchmarks:
      self.assertEquals(1, len(names_to_benchmarks[n]),
                        'Multiple benchmarks with the same name %s are '
                        'found: %s' % (n, str(names_to_benchmarks[n])))

# Test all perf benchmarks explicitly define the Name() method and the name
# values are the same as the default one.
# TODO(nednguyen): remove this test after all perf benchmarks define Name() and
# checked in.
class TestPerfBenchmarkNames(unittest.TestCase):
  def runTest(self):
    all_benchmarks = _GetAllPerfBenchmarks()

    def BenchmarkName(cls):  # Copy from Benchmark.Name()'s implementation
      name = cls.__module__.split('.')[-1]
      if hasattr(cls, 'tag'):
        name += '.' + cls.tag
      if hasattr(cls, 'page_set'):
        name += '.' + cls.page_set.Name()
      return name

    for b in all_benchmarks:
      self.assertNotEquals(b.Name, benchmark_module.Benchmark.Name)
      self.assertEquals(b.Name(), BenchmarkName(b))


def _AddBenchmarkOptionsTests(suite):
  # Using |index_by_class_name=True| allows returning multiple benchmarks
  # from a module.
  all_benchmarks = _GetAllPerfBenchmarks()
  for benchmark in all_benchmarks:
    if not benchmark.options:
      # No need to test benchmarks that have not defined options.
      continue
    class BenchmarkOptionsTest(unittest.TestCase):
      pass
    setattr(BenchmarkOptionsTest, benchmark.Name(),
            _BenchmarkOptionsTestGenerator(benchmark))
    suite.addTest(BenchmarkOptionsTest(benchmark.Name()))
  suite.addTest(TestNoBenchmarkNamesDuplication())
  suite.addTest(TestPerfBenchmarkNames())


def load_tests(_, _2, _3):
  suite = progress_reporter.TestSuite()
  _AddBenchmarkOptionsTests(suite)
  return suite
