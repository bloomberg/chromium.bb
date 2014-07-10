# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


def _CreateXpathFunction(xpath):
  return ('document.evaluate("%s",'
                             'document,'
                             'null,'
                             'XPathResult.FIRST_ORDERED_NODE_TYPE,'
                             'null)'
          '.singleNodeValue' % re.escape(xpath))


class IndexeddbOfflinePage(page_module.Page):

  """ Why: Simulates user input while offline and sync while online. """

  def __init__(self, page_set):
    super(IndexeddbOfflinePage, self).__init__(
      url='file://endure/indexeddb_app.html',
      page_set=page_set,
      name='indexeddb_offline')
    self.user_agent_type = 'desktop'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForElement(text='initialized')

  def RunEndure(self, action_runner):
    action_runner.WaitForElement('button[id="online"]:not(disabled)')
    action_runner.ClickElement('button[id="online"]:not(disabled)')
    action_runner.WaitForElement(
        element_function=_CreateXpathFunction('id("state")[text()="online"]'))
    action_runner.Wait(1)
    action_runner.WaitForElement('button[id="offline"]:not(disabled)')
    action_runner.ClickElement('button[id="offline"]:not(disabled)')
    action_runner.WaitForElement(
        element_function=_CreateXpathFunction('id("state")[text()="offline"]'))


class IndexeddbOfflinePageSet(page_set_module.PageSet):

  """ Chrome Endure test for IndexedDB. """

  def __init__(self):
    super(IndexeddbOfflinePageSet, self).__init__(
        user_agent_type='desktop')

    self.AddPage(IndexeddbOfflinePage(self))
