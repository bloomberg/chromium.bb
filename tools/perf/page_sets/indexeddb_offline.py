# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class IndexeddbOfflinePage(page_module.PageWithDefaultRunNavigate):

  """ Why: Simulates user input while offline and sync while online. """

  def __init__(self, page_set):
    super(IndexeddbOfflinePage, self).__init__(
      url='file://endure/indexeddb_app.html',
      page_set=page_set)
    self.user_agent_type = 'desktop'
    self.name = 'indexeddb_offline'

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
        {
            'text': 'initialized',
            'condition': 'element'
        }))

  def RunEndure(self, action_runner):
    action_runner.RunAction(WaitAction(
        {
            'condition': 'element',
            'selector': 'button[id="online"]:not(disabled)'
        }))
    action_runner.RunAction(ClickElementAction(
        {
            'selector': 'button[id="online"]:not(disabled)'
        }))
    action_runner.RunAction(WaitAction(
        {
            'xpath': 'id("state")[text()="online"]',
            'condition': 'element'
        }))
    action_runner.RunAction(WaitAction(
        {
            "seconds": 1
        }))
    action_runner.RunAction(WaitAction(
        {
            'condition': 'element',
            'selector': 'button[id="offline"]:not(disabled)'
        }))
    action_runner.RunAction(ClickElementAction(
        {
            'selector': 'button[id="offline"]:not(disabled)'
        }))
    action_runner.RunAction(WaitAction(
        {
            'xpath': 'id("state")[text()="offline"]',
            'condition': 'element'
        }))


class IndexeddbOfflinePageSet(page_set_module.PageSet):

  """ Chrome Endure test for IndexedDB. """

  def __init__(self):
    super(IndexeddbOfflinePageSet, self).__init__(
        user_agent_type='desktop')

    self.AddPage(IndexeddbOfflinePage(self))
