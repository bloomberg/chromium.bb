# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

from chrome_remote_control import browser_finder
from chrome_remote_control import browser_options
from chrome_remote_control import multi_page_benchmark
from chrome_remote_control import page_runner
from chrome_remote_control import page_set

class MultiPageBenchmarkUnitTestBase(unittest.TestCase):
  """unittest.TestCase-derived class to help in the construction of unit tests
  for a benchmark."""

  unittest_data_dir = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                      '..', 'unittest_data'))

  def CreatePageSetFromFileInUnittestDataDir(self, test_filename):
    path = os.path.join(self.unittest_data_dir, test_filename)
    self.assertTrue(os.path.exists(path))
    page = page_set.Page('file://%s' % path)

    ps = page_set.PageSet()
    ps.pages.append(page)
    return ps

  def CustomizeOptionsForTest(self, options):
    """Override to customize default options."""
    pass

  def RunBenchmark(self, benchmark, ps):
    """Runs a benchmark against a pageset, returning the rows its outputs."""
    assert browser_options.options_for_unittests
    options = (
      browser_options.options_for_unittests.Copy())
    temp_parser = options.CreateParser()
    benchmark.AddOptions(temp_parser)
    defaults = temp_parser.get_default_values()
    for k, v in defaults.__dict__.items():
      if hasattr(options, k):
        continue
      setattr(options, k, v)

    benchmark.CustomizeBrowserOptions(options)
    self.CustomizeOptionsForTest(options)
    possible_browser = browser_finder.FindBrowser(options)

    results = multi_page_benchmark.BenchmarkResults()
    with page_runner.PageRunner(ps) as runner:
      runner.Run(options, possible_browser, benchmark, results)
    return results
