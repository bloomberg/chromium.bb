# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from gpu_tools import first_paint_time_benchmark
from gpu_tools import multi_page_benchmark

class FirstPaintTimeBenchmarkUnitTest(
  multi_page_benchmark.MultiPageBenchmarkUnitTest):

  def testFirstPaintTimeMeasurement(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')

    benchmark = first_paint_time_benchmark.FirstPaintTimeBenchmark()
    rows = self.RunBenchmark(benchmark, ps)

    self.assertEqual(0, len(benchmark.page_failures))

    header = rows[0]
    results = rows[1:]
    self.assertEqual(['url', 'first_paint_secs'],
                     header)
    self.assertEqual(1, len(results))
    self.assertTrue('scrollable_page.html' in results[0][0])
    if results[0][1] == 'unsupported':
      # This test can't run on content_shell.
      return
    self.assertTrue(results[0][1] > 0)
