# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page_benchmark_unittest_base
# from perf_tools import image_decoding_benchmark


class ImageDecodingBenchmarkUnitTest(
  page_benchmark_unittest_base.PageBenchmarkUnitTestBase):

  def testImageDecodingMeasurement(self):
    # TODO(qinmin): uncomment this after we fix the image decoding benchmark
    # for lazily decoded images
    return
    # ps = self.CreatePageSetFromFileInUnittestDataDir('image_decoding.html')

    # benchmark = image_decoding_benchmark.ImageDecoding()
    # all_results = self.RunBenchmark(benchmark, ps)

    # self.assertEqual(0, len(all_results.page_failures))
    # self.assertEqual(1, len(all_results.page_results))

    # results0 = all_results.page_results[0]
    # self.assertTrue('ImageDecoding_avg' in results0)
    # self.assertTrue(results0['ImageDecoding_avg'] > 0)
