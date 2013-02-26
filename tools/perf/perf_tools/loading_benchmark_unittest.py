# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from perf_tools import loading_benchmark
from telemetry.page import page_benchmark_unittest_base

class SmoothnessBenchmarkUnitTest(
  page_benchmark_unittest_base.PageBenchmarkUnitTestBase):

  def testBasicFunctionality(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('blank.html')

    benchmark = loading_benchmark.LoadingBenchmark()
    all_results = self.RunBenchmark(benchmark, ps)

    self.assertEqual(0, len(all_results.page_failures))
    self.assertEqual(1, len(all_results.page_results))

    results0 = all_results.page_results[0]
    self.assertTrue(results0['Layout'].value > 0)
    self.assertTrue(results0['Paint'].value > 0)
