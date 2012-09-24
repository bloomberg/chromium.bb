# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from chrome_remote_control import multi_page_benchmark
from chrome_remote_control import multi_page_benchmark_unittest_base

class BenchThatFails(multi_page_benchmark.MultiPageBenchmark):
  def MeasurePage(self, page, tab):
    raise multi_page_benchmark.MeasurementFailure('Intentional failure.')

class BenchThatHasDefaults(multi_page_benchmark.MultiPageBenchmark):
  def AddOptions(self, parser):
    parser.add_option('-x', dest='x', default=3)

  def MeasurePage(self, page, tab):
    assert self.options.x == 3
    return {'x': 7}

class MultiPageBenchmarkUnitTest(
  multi_page_benchmark_unittest_base.MultiPageBenchmarkUnitTestBase):

  def testFailure(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('blank.html')
    benchmark = BenchThatFails()
    all_results = self.RunBenchmark(benchmark, ps)
    self.assertEquals(1, len(all_results.page_failures))

  def testDefaults(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('blank.html')
    benchmark = BenchThatHasDefaults()
    all_results = self.RunBenchmark(benchmark, ps)
    self.assertEquals(len(all_results.page_results), 1)
    self.assertEquals(all_results.page_results[0]['results']["x"], 7)

