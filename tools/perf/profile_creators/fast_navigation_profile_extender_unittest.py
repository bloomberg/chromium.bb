# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from profile_creators.fast_navigation_profile_extender import (
    FastNavigationProfileExtender)
from telemetry.core import util

util.AddDirToPythonPath(util.GetTelemetryDir(), 'third_party', 'mock')
import mock


class FakeTab(object):
  pass


class FakeBrowser(object):
  def __init__(self, tab_count):
    self.tabs = []
    for _ in range(tab_count):
      self.tabs.append(FakeTab())


# Testing private method.
# pylint: disable=protected-access
class FastNavigationProfileExtenderTest(unittest.TestCase):
  def testPerformNavigations(self):
    extender = FastNavigationProfileExtender()
    num_urls = extender._NUM_PARALLEL_PAGES * 3 + 4
    num_tabs = extender._NUM_TABS

    navigation_urls = []
    for i in range(num_urls):
      navigation_urls.append('http://test%s.com' % i)

    extender._navigation_urls = navigation_urls
    extender._browser = FakeBrowser(num_tabs)
    extender._WaitForQueuedTabsToLoad = mock.MagicMock()
    extender._BatchNavigateTabs = mock.MagicMock()

    # Set up a callback to record the tabs and urls in each navigation.
    batch_callback_tabs = []
    batch_callback_urls = []
    def SideEffect(*args, **_):
      batch = args[0]
      for tab, url in batch:
        batch_callback_tabs.append(tab)
        batch_callback_urls.append(url)
    extender._BatchNavigateTabs.side_effect = SideEffect

    # Perform the navigations.
    extender._PerformNavigations()

    # Each url should have been navigated to exactly once.
    self.assertEqual(set(batch_callback_urls), set(navigation_urls))

    # The first 4 tabs should have been navigated 4 times. The remaining tabs
    # should have been navigated 3 times.
    num_navigations_per_tab = 3
    num_tabs_with_one_extra_navigation = 4
    for i in range(len(extender._browser.tabs)):
      tab = extender._browser.tabs[i]

      expected_tab_navigation_count = num_navigations_per_tab
      if i < num_tabs_with_one_extra_navigation:
        expected_tab_navigation_count += 1

      count = batch_callback_tabs.count(tab)
      self.assertEqual(count, expected_tab_navigation_count)
