# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry.core import util
from telemetry.page import page_set
from telemetry.page import page_runner
from telemetry.page import profile_creator

class SmallProfileCreator(profile_creator.ProfileCreator):
  """
  Runs a browser through a series of operations to fill in a small test profile.
  """

  def __init__(self):
    super(SmallProfileCreator, self).__init__()
    top_25 = os.path.join(util.GetBaseDir(), 'page_sets', 'top_25.json')
    self._page_set = page_set.PageSet.FromFile(top_25)

    # Open all links in the same tab save for the last N_NUM_TABS links which
    # are each opened in a new tab.
    self._NUM_TABS = 5

  def CanRunForPage(self, page):
    idx = page.page_set.pages.index(page)
    return idx <= (len(page.page_set.pages) - self._NUM_TABS)

  def DidNavigateToPage(self, page, tab):
    num_pages_in_pageset = len(page.page_set.pages)
    last_tab_loaded_automatically = num_pages_in_pageset - self._NUM_TABS
    page_index = page.page_set.pages.index(page)
    if (page_index == last_tab_loaded_automatically):
      for i in xrange(last_tab_loaded_automatically + 1, num_pages_in_pageset):
        # Load the last _NUM_TABS pages, each in a new tab.
        t = tab.browser.tabs.New()

        page_state = page_runner.PageState()
        page_state.PreparePage(page.page_set.pages[i], t)
        page_state.ImplicitPageNavigation(page.page_set.pages[i], t)

        t.WaitForDocumentReadyStateToBeInteractiveOrBetter()


  def MeasurePage(self, _, tab, results):
    # Can't use WaitForDocumentReadyStateToBeComplete() here due to
    # crbug.com/280750 .
    tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()
