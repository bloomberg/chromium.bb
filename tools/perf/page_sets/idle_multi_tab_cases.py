# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from telemetry.page import shared_page_state
from telemetry.page import page_set as page_set_module

from page_sets import top_pages
from page_sets import top_7_stress


def _SpawnTab(action_runner):
  return action_runner.EvaluateJavaScript('__telemetry_spawn_tab()')


def _CloseTab(action_runner, tab):
  action_runner.EvaluateJavaScript('__telemetry_close_tab(' + str(tab) + ')')


def _CreateIdleMultiTabPageClass(base_page_cls, base_js):
  class DerivedIdleMultiTabPage(base_page_cls):  # pylint: disable=W0232

    def RunPageInteractions(self, action_runner):
      MAX_TABS = 10
      action_runner.ExecuteJavaScript(base_js)
      tabs = {}
      # Slowly open tabs.
      for tab in xrange(MAX_TABS):
        tabs[tab] = _SpawnTab(action_runner)
        action_runner.Wait(1)
      action_runner.Wait(20)
      # Slowly close tabs.
      for tab in xrange(MAX_TABS):
        _CloseTab(action_runner, tabs[tab])
        action_runner.Wait(1)
      action_runner.Wait(30)
      # Quickly open tabs.
      for tab in xrange(MAX_TABS):
        tabs[tab] = _SpawnTab(action_runner)
      action_runner.Wait(20)
      # Quickly close tabs.
      for tab in xrange(MAX_TABS):
        _CloseTab(action_runner, tabs[tab])
      action_runner.Wait(30)
  return DerivedIdleMultiTabPage


class IdleMultiTabCasesPageSet(page_set_module.PageSet):

  """ Pages for testing GC efficiency on idle pages. """

  def __init__(self):
    super(IdleMultiTabCasesPageSet, self).__init__(
        archive_data_file='data/top_25.json',
        bucket=page_set_module.PARTNER_BUCKET)
    with open(os.path.join(os.path.dirname(__file__),
              'idle_multi_tab_cases.js')) as f:
      base_js = f.read()
    self.AddUserStory(
        _CreateIdleMultiTabPageClass(top_pages.GoogleDocPage, base_js)
        (self, shared_page_state.SharedDesktopPageState))
    self.AddUserStory(
        _CreateIdleMultiTabPageClass(top_7_stress.GooglePlusPage, base_js)
        (self))
