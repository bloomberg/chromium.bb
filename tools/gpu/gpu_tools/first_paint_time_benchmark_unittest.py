# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from chrome_remote_control import multi_page_benchmark_unittest_base
from gpu_tools import first_paint_time_benchmark

class FirstPaintTimeBenchmarkUnitTest(
  multi_page_benchmark_unittest_base.MultiPageBenchmarkUnitTestBase):

  def testFirstPaintTimeMeasurement(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')

    benchmark = first_paint_time_benchmark.FirstPaintTimeBenchmark()
    all_results = self.RunBenchmark(benchmark, ps)

    self.assertEqual(0, len(all_results.page_failures))
    self.assertEqual(1, len(all_results.page_results))

    results0 = all_results.page_results[0]['results']
    if results0['first_paint_secs'] == 'unsupported':
      # This test can't run on content_shell.
      return
    self.assertTrue(results0['first_paint_secs'] > 0)
