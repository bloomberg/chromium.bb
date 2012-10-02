# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from chrome_remote_control import multi_page_benchmark
from chrome_remote_control import multi_page_benchmark_unittest_base
from chrome_remote_control import page_set

class BenchThatFails(multi_page_benchmark.MultiPageBenchmark):
  def MeasurePage(self, page, tab):
    raise multi_page_benchmark.MeasurementFailure('Intentional failure.')

class BenchThatHasDefaults(multi_page_benchmark.MultiPageBenchmark):
  def AddOptions(self, parser):
    parser.add_option('-x', dest='x', default=3)

  def MeasurePage(self, page, tab):
    assert self.options.x == 3
    return {'x': 7}

class BenchForBlank(multi_page_benchmark.MultiPageBenchmark):
  def MeasurePage(self, page, tab):
    contents = tab.runtime.Evaluate('document.body.textContent')
    assert contents.strip() == 'Hello world'

class BenchForReplay(multi_page_benchmark.MultiPageBenchmark):
  def MeasurePage(self, page, tab):
    # Web Page Replay returns '404 Not found' if a page is not in the archive.
    contents = tab.runtime.Evaluate('document.body.textContent')
    if '404 Not Found' in contents.strip():
      raise multi_page_benchmark.MeasurementFailure('Page not in archive.')

class MultiPageBenchmarkUnitTest(
  multi_page_benchmark_unittest_base.MultiPageBenchmarkUnitTestBase):

  _record = False

  def CustomizeOptionsForTest(self, options):
    options.record = self._record

  def testGotToBlank(self):
    ps = self.CreatePageSetFromFileInUnittestDataDir('blank.html')
    benchmark = BenchForBlank()
    all_results = self.RunBenchmark(benchmark, ps)
    self.assertEquals(0, len(all_results.page_failures))

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
    self.assertEquals(all_results.page_results[0]['results']['x'], 7)

  def testRecordAndReplay(self):
    test_archive = '/tmp/google.wpr'
    try:
      ps = page_set.PageSet(description='',
                            archive_path=test_archive,
                            file_path='/tmp/test_pageset.json')
      benchmark = BenchForReplay()

      # First record an archive with only www.google.com.
      self._record = True

      ps.pages = [page_set.Page('http://www.google.com/')]
      all_results = self.RunBenchmark(benchmark, ps)
      self.assertEquals(0, len(all_results.page_failures))

      # Now replay it and verify that google.com is found but foo.com is not.
      self._record = False

      ps.pages = [page_set.Page('http://www.foo.com/')]
      all_results = self.RunBenchmark(benchmark, ps)
      self.assertEquals(1, len(all_results.page_failures))

      ps.pages = [page_set.Page('http://www.google.com/')]
      all_results = self.RunBenchmark(benchmark, ps)
      self.assertEquals(0, len(all_results.page_failures))

    finally:
      assert os.path.isfile(test_archive)
      os.remove(test_archive)
