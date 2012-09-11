# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import scrolling_benchmark
import multi_page_benchmark

class ScrollingBenchmarkUnitTest(
  multi_page_benchmark.MultiPageBenchmarkUnitTest):

  def testScrollingWithGpuBenchmarkingExtension(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')

    benchmark = scrolling_benchmark.ScrollingBenchmark()
    rows = self.RunBenchmark(benchmark, ps)

    self.assertEqual(0, len(benchmark.page_failures))

    header = rows[0]
    results = rows[1:]
    self.assertEqual(['url', 'dropped_percent', 'mean_frame_time_ms'],
                     header)
    self.assertEqual(1, len(results))
    self.assertTrue('scrollable_page.html' in results[0][0])

  def testScrollingWithoutGpuBenchmarkingExtension(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('scrollable_page.html')

    benchmark = scrolling_benchmark.ScrollingBenchmark()
    benchmark.use_gpu_bencharking_extension = False

    rows = self.RunBenchmark(benchmark, ps)

    self.assertEqual(0, len(benchmark.page_failures))

    header = rows[0]
    results = rows[1:]
    self.assertEqual(['url', 'dropped_percent', 'mean_frame_time_ms'],
                     header)
    self.assertEqual(1, len(results))
    self.assertTrue('scrollable_page.html' in results[0][0])

  def testCalcResultsFromRAFRenderStats(self):
    rendering_stats = {'droppedFrameCount': 5,
               'totalTimeInSeconds': 1,
               'numAnimationFrames': 10,
               'numFramesSentToScreen': 10}
    res = scrolling_benchmark._CalcScrollResults(rendering_stats)
    self.assertEquals(50, res["dropped_percent"])
    self.assertAlmostEquals(100, res["mean_frame_time_ms"], 2)


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
    res = scrolling_benchmark._CalcScrollResults(rendering_stats)
    self.assertEquals(0, res["dropped_percent"])
    self.assertAlmostEquals(1000/60.0, res["mean_frame_time_ms"], 2)
