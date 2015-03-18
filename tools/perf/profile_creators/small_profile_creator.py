# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from profile_creators import profile_creator

import logging
import page_sets

from telemetry import benchmark
from telemetry.page import page_test
from telemetry.page import test_expectations
from telemetry.results import results_options
from telemetry.user_story import user_story_runner


class SmallProfileCreator(profile_creator.ProfileCreator):
  """
  Runs a browser through a series of operations to fill in a small test profile.

  This consists of performing a single navigation to each of the top 25 pages.
  """
  class PageTest(page_test.PageTest):
    def __init__(self):
      super(SmallProfileCreator.PageTest, self).__init__()
      self._page_set = page_sets.Typical25PageSet()
      self._ValidatePageSet(self._page_set)

      # Open all links in the same tab save for the last _NUM_TABS links which
      # are each opened in a new tab.
      self._NUM_TABS = 5

    @staticmethod
    def _ValidatePageSet(page_set):
      """Raise an exception if |page_set| uses more than one WPR archive."""
      wpr_paths = set(page_set.WprFilePathForUserStory(p)
                      for p in page_set if not p.is_local)
      if len(wpr_paths) > 1:
        raise Exception("Invalid page set: has multiple WPR archives: %s" %
                        ','.join(sorted(wpr_paths)))

    def TabForPage(self, page, browser):
      """Superclass override."""
      idx = page.page_set.pages.index(page)
      # The last _NUM_TABS pages open a new tab.
      if idx <= (len(page.page_set.pages) - self._NUM_TABS):
        return browser.tabs[0]
      else:
        return browser.tabs.New()

    def ValidateAndMeasurePage(self, page, tab, results):
      """Superclass override."""
      tab.WaitForDocumentReadyStateToBeComplete()

  def __init__(self):
    super(SmallProfileCreator, self).__init__()
    self._page_test = SmallProfileCreator.PageTest()

  def Run(self, options):
    expectations = test_expectations.TestExpectations()
    results = results_options.CreateResults(
        benchmark.BenchmarkMetadata(profile_creator.__class__.__name__),
        options)
    user_story_runner.Run(self._page_test, self._page_test._page_set,
        expectations, options, results)

    if results.failures:
      logging.warning('Some pages failed to load.')
      logging.warning('Failed pages:\n%s',
                      '\n'.join(map(str, results.pages_that_failed)))
      raise Exception('SmallProfileCreator failed.')
