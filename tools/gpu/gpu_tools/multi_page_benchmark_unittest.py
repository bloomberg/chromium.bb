# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from gpu_tools import multi_page_benchmark

class BenchThatFails(multi_page_benchmark.MultiPageBenchmark):
  def MeasurePage(self, page, tab):
    raise multi_page_benchmark.MeasurementFailure('Whoops')

class BenchThatHasDefaults(multi_page_benchmark.MultiPageBenchmark):
  def AddOptions(self, parser):
    parser.add_option('-x', dest='x', default=3)

  def MeasurePage(self, page, tab):
    assert self.options.x == 3
    return {}

class MultiPageBenchmarkTest(multi_page_benchmark.MultiPageBenchmarkUnitTest):
  def testFailure(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('non_scrollable_page.html')
    benchmark = BenchThatFails()
    self.RunBenchmark(benchmark, ps)
    self.assertEquals(1, len(benchmark.page_failures))

  def testDefaults(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('non_scrollable_page.html')
    benchmark = BenchThatHasDefaults()
    self.RunBenchmark(benchmark, ps)

