# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import profile_creator

import page_sets


class SmallProfileCreator(profile_creator.ProfileCreator):
  """
  Runs a browser through a series of operations to fill in a small test profile.
  """

  def __init__(self):
    super(SmallProfileCreator, self).__init__()
    self._page_set = page_sets.Typical25PageSet()

    # Open all links in the same tab save for the last _NUM_TABS links which
    # are each opened in a new tab.
    self._NUM_TABS = 5

  def TabForPage(self, page, browser):
    idx = page.page_set.pages.index(page)
    # The last _NUM_TABS pages open a new tab.
    if idx <= (len(page.page_set.pages) - self._NUM_TABS):
      return browser.tabs[0]
    else:
      return browser.tabs.New()

  def ValidateAndMeasurePage(self, _, tab, results):
    tab.WaitForDocumentReadyStateToBeComplete()
