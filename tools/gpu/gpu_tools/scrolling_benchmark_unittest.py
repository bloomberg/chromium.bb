# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from chrome_remote_control import multi_page_benchmark_unittest_base
from gpu_tools import scrolling_benchmark

class ScrollingBenchmarkUnitTest(
  multi_page_benchmark_unittest_base.MultiPageBenchmarkUnitTestBase):

  def testScrollingWithGpuBenchmarkingExtension(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')

    benchmark = scrolling_benchmark.ScrollingBenchmark()
    all_results = self.RunBenchmark(benchmark, ps)

    self.assertEqual(0, len(all_results.page_failures))
    self.assertEqual(1, len(all_results.page_results))
    results0 = all_results.page_results[0]['results']

    self.assertTrue('dropped_percent' in results0)
    self.assertTrue('mean_frame_time_ms' in results0)

  def testScrollingWithoutGpuBenchmarkingExtension(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')

    benchmark = scrolling_benchmark.ScrollingBenchmark()
    benchmark.use_gpu_benchmarking_extension = False

    all_results = self.RunBenchmark(benchmark, ps)

    self.assertEqual(0, len(all_results.page_failures))
    self.assertEqual(1, len(all_results.page_results))
    results0 = all_results.page_results[0]['results']

    self.assertTrue('dropped_percent' in results0)
    self.assertTrue('mean_frame_time_ms' in results0)

  def testCalcResultsFromRAFRenderStats(self):
    rendering_stats = {'droppedFrameCount': 5,
                       'totalTimeInSeconds': 1,
                       'numAnimationFrames': 10,
                       'numFramesSentToScreen': 10}
    res = scrolling_benchmark.CalcScrollResults(rendering_stats)
    self.assertEquals(50, res['dropped_percent'])
    self.assertAlmostEquals(100, res['mean_frame_time_ms'], 2)

  def testCalcResultsRealRenderStats(self):
    rendering_stats = {'numFramesSentToScreen': 60,
                       'globalTotalTextureUploadTimeInSeconds': 0,
                       'totalProcessingCommandsTimeInSeconds': 0,
                       'globalTextureUploadCount': 0,
                       'droppedFrameCount': 0,
                       'textureUploadCount': 0,
                       'numAnimationFrames': 10,
                       'totalPaintTimeInSeconds': 0.35374299999999986,
                       'globalTotalProcessingCommandsTimeInSeconds': 0,
                       'totalTextureUploadTimeInSeconds': 0,
                       'totalRasterizeTimeInSeconds': 0,
                       'totalTimeInSeconds': 1.0}
    res = scrolling_benchmark.CalcScrollResults(rendering_stats)
    self.assertEquals(0, res['dropped_percent'])
    self.assertAlmostEquals(1000/60.0, res['mean_frame_time_ms'], 2)
