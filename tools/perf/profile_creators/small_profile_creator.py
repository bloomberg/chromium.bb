# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry.core import util
from telemetry.page import page_runner
from telemetry.page import page_set
from telemetry.page import profile_creator

class SmallProfileCreator(profile_creator.ProfileCreator):
  """
  Runs a browser through a series of operations to fill in a small test profile.
  """

  def __init__(self):
    super(SmallProfileCreator, self).__init__()
    top_25 = os.path.join(util.GetBaseDir(), 'page_sets', 'top_25.json')
    self._page_set = page_set.PageSet.FromFile(top_25)

  def CanRunForPage(self, page):
    # Return true only for the first page, the other pages are opened in new
    # tabs by DidNavigateToPage().
    return not page.page_set.pages.index(page)

  def DidNavigateToPage(self, page, tab):
    for i in xrange(1, len(page.page_set.pages)):
      t = tab.browser.tabs.New()

      page_state = page_runner.PageState()
      page_state.PreparePage(page.page_set.pages[i], t)
      page_state.ImplicitPageNavigation(page.page_set.pages[i], t)

      t.WaitForDocumentReadyStateToBeComplete()
      # Work around crbug.com/258113.
      util.CloseConnections(t)

  def MeasurePage(self, _, tab, results):
    pass
