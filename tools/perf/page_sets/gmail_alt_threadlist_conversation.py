# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
# pylint: disable=W0401,W0614
from telemetry.page.actions.all_page_actions import *
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class GmailAltThreadlistConversationPage(
  page_module.PageWithDefaultRunNavigate):

  """ Why: Alternate between Inbox and the first email conversation. """

  def __init__(self, page_set):
    super(GmailAltThreadlistConversationPage, self).__init__(
      url='https://mail.google.com/mail/',
      page_set=page_set)
    self.credentials_path = 'data/credentials.json'
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/gmail_alt_threadlist_conversation.json'
    self.name = 'gmail_alt_threadlist_conversation'
    self.credentials = 'google'

  def RunNavigateSteps(self, action_runner):
    action_runner.RunAction(NavigateAction())
    action_runner.RunAction(WaitAction(
      {
        'javascript':
        'window.gmonkey !== undefined && document.getElementById("gb") !== null'
      }))

  def RunEndure(self, action_runner):
    action_runner.RunAction(ClickElementAction(
      {
        'xpath': '//span[@email]',
        'wait_until': {'condition': 'href_change'}
      }))
    action_runner.RunAction(WaitAction({'seconds': 1}))
    action_runner.RunAction(ClickElementAction(
      {
        'wait_until': {'condition': 'href_change'},
        'selector': 'a[href="https://mail.google.com/mail/u/0/?shva=1#inbox"]'
      }))
    action_runner.RunAction(WaitAction({'seconds': 1}))


class GmailAltThreadlistConversationPageSet(page_set_module.PageSet):

  """ Chrome Endure test for GMail. """

  def __init__(self):
    super(GmailAltThreadlistConversationPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='desktop',
      archive_data_file='data/gmail_alt_threadlist_conversation.json')

    self.AddPage(GmailAltThreadlistConversationPage(self))
