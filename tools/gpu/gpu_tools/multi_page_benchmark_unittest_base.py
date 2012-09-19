# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest

import chrome_remote_control.browser_options
from gpu_tools import page_set

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
    rows = []
    class LocalWriter(object):
      @staticmethod
      def writerow(row):
        rows.append(row)

    assert chrome_remote_control.browser_options.options_for_unittests
    options = (
      chrome_remote_control.browser_options.options_for_unittests.Copy())
    temp_parser = options.CreateParser()
    benchmark.AddOptions(temp_parser)
    defaults = temp_parser.get_default_values()
    for k, v in defaults.__dict__.items():
      if hasattr(options, k):
        continue
      setattr(options, k, v)

    benchmark.CustomizeBrowserOptions(options)
    self.CustomizeOptionsForTest(options)
    possible_browser = chrome_remote_control.FindBrowser(options)
    with possible_browser.Create() as browser:
      benchmark.Run(LocalWriter(), browser, options, ps)
    return rows
