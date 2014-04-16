# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class MobileMemoryPage(page_module.PageWithDefaultRunNavigate):

  def __init__(self, url, page_set):
    super(MobileMemoryPage, self).__init__(url=url, page_set=page_set)
    self.credentials_path = 'data/credentials.json'
    self.user_agent_type = 'mobile'
    self.archive_data_file = 'data/mobile_memory.json'


class GmailPage(MobileMemoryPage):

  def __init__(self, page_set):
    super(GmailPage, self).__init__(
        url='https://mail.google.com/mail/m',
        page_set=page_set)

    self.reload_and_gc = [{'action': 'reload'},
                          {'action': 'wait',
                           'seconds': 15},
                          {'action': 'js_collect_garbage'}]
    self.credentials = 'google'

  def ReloadAndGc(self, action_runner):
    action_runner.RunAction(ReloadAction())
    action_runner.RunAction(WaitAction(
        {
            'seconds': 15
        }))
    action_runner.RunAction(JsCollectGarbageAction())

  def RunStressMemory(self, action_runner):
    for _ in xrange(3):
      self.ReloadAndGc(action_runner)


class GoogleSearchPage(MobileMemoryPage):

  """ Why: Tests usage of discardable memory """

  def __init__(self, page_set):
    super(GoogleSearchPage, self).__init__(
        url='https://www.google.com/search?site=&tbm=isch&q=google',
        page_set=page_set)

  def RunStressMemory(self, action_runner):
    action_runner.RunAction(ScrollAction())
    action_runner.RunAction(WaitAction(
        {
            'seconds': 3
        }))
    action_runner.RunAction(ScrollAction())
    action_runner.RunAction(WaitAction(
        {
            'seconds': 3
        }))
    action_runner.RunAction(ScrollAction())
    action_runner.RunAction(WaitAction(
        {
            'seconds': 3
        }))
    action_runner.RunAction(ScrollAction())
    action_runner.RunAction(WaitAction(
        {
            'javascript':
              'document.getElementById("rg_s").childElementCount > 300'
        }))


class ScrollPage(MobileMemoryPage):

  def __init__(self, url, page_set):
    super(ScrollPage, self).__init__(url=url, page_set=page_set)

  def RunStressMemory(self, action_runner):
    action_runner.RunAction(ScrollAction())


class MobileMemoryPageSet(page_set_module.PageSet):

  """ Mobile sites with interesting memory characteristics """

  def __init__(self):
    super(MobileMemoryPageSet, self).__init__(
        credentials_path='data/credentials.json',
        user_agent_type='mobile',
        archive_data_file='data/mobile_memory.json')

    self.AddPage(GmailPage(self))
    self.AddPage(GoogleSearchPage(self))

    urls_list = [
      # Why: Renderer process memory bloat
      'http://techcrunch.com',
      # pylint: disable=C0301
      'http://techcrunch.com/2014/02/17/pixel-brings-brings-old-school-video-game-art-to-life-in-your-home/'
      'http://techcrunch.com/2014/02/15/kickstarter-coins-2/'
      'http://techcrunch.com/2014/02/15/was-y-combinator-worth-it/'
    ]

    for url in urls_list:
      self.AddPage(ScrollPage(url, self))
