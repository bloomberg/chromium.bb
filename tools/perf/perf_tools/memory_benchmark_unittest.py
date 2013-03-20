# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from perf_tools import memory_benchmark
from telemetry.page import page_benchmark_unittest_base

class MemoryBenchmarkUnitTest(
  page_benchmark_unittest_base.PageBenchmarkUnitTestBase):

  def testMemoryBenchmark(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('page_with_link.html')
    ps.pages[0].stress_memory = {'action': 'click_element', 'text': 'Click me'}

    benchmark = memory_benchmark.MemoryBenchmark()
    all_results = self.RunBenchmark(benchmark, ps)

    self.assertEqual(0, len(all_results.page_failures))
    self.assertEqual(1, len(all_results.page_results))

    results0 = all_results.page_results[0]
    expected_measurements = ['V8_MemoryExternalFragmentationTotal',
                             'V8_MemoryHeapSampleTotalCommitted',
                             'V8_MemoryHeapSampleTotalUsed']

    self.assertTrue(all(
        [m in results0.measurement_names for m in expected_measurements]))
