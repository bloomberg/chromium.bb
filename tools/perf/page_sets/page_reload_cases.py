# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module

from page_sets import top_pages


def _Reload(action_runner):
  # Numbers below are chosen arbitrarily. For the V8DetachedContextAgeInGC
  # the number of reloads should be high enough so that V8 could do few
  # incremeantal GCs.
  NUMBER_OF_RELOADS = 7
  WAIT_TIME = 2
  for _ in xrange(NUMBER_OF_RELOADS):
    action_runner.ReloadPage()
    action_runner.Wait(WAIT_TIME)


def _CreatePageClassWithReload(page_cls):
  class DerivedSmoothPage(page_cls):  # pylint: disable=W0232

    def RunPageInteractions(self, action_runner):
      _Reload(action_runner)
  return DerivedSmoothPage


class PageReloadCasesPageSet(page_set_module.PageSet):

  """ Pages for testing GC efficiency on page reload. """

  def __init__(self):
    super(PageReloadCasesPageSet, self).__init__(
        user_agent_type='desktop',
        archive_data_file='data/top_25.json',
        bucket=page_set_module.PARTNER_BUCKET)

    self.AddUserStory(_CreatePageClassWithReload(
        top_pages.GoogleWebSearchPage)(self))
    self.AddUserStory(_CreatePageClassWithReload(
        top_pages.GmailPage)(self))
    self.AddUserStory(_CreatePageClassWithReload(
        top_pages.GoogleCalendarPage)(self))
    self.AddUserStory(_CreatePageClassWithReload(
        top_pages.GoogleDocPage)(self))
    self.AddUserStory(_CreatePageClassWithReload(
        top_pages.GooglePlusPage)(self))
    self.AddUserStory(_CreatePageClassWithReload(
        top_pages.YoutubePage)(self))
    self.AddUserStory(_CreatePageClassWithReload(
        top_pages.WordpressPage)(self))
    self.AddUserStory(_CreatePageClassWithReload(
        top_pages.FacebookPage)(self))
