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


def _GetCurrentLocation(action_runner):
  return action_runner.EvaluateJavaScript('document.location.href')


def _WaitForLocationChange(action_runner, old_href):
  action_runner.WaitForJavaScriptCondition(
      'document.location.href != "%s"' % old_href)


class GmailAltThreadlistConversationPage(
  page_module.Page):

  """ Why: Alternate between Inbox and the first email conversation. """

  def __init__(self, page_set):
    super(GmailAltThreadlistConversationPage, self).__init__(
      url='https://mail.google.com/mail/',
      page_set=page_set,
      name='gmail_alt_threadlist_conversation')
    self.credentials_path = 'data/credentials.json'
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/gmail_alt_threadlist_conversation.json'
    self.credentials = 'google'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForJavaScriptCondition(
        'window.gmonkey !== undefined && '
        'document.getElementById("gb") !== null')

  def RunEndure(self, action_runner):
    old_href = _GetCurrentLocation(action_runner)
    action_runner.ClickElement(
        element_function=_CreateXpathFunction('//span[@email]'))
    _WaitForLocationChange(action_runner, old_href)
    action_runner.Wait(1)
    old_href = _GetCurrentLocation(action_runner)
    action_runner.ClickElement(
        'a[href="https://mail.google.com/mail/u/0/?shva=1#inbox"]')
    _WaitForLocationChange(action_runner, old_href)
    action_runner.Wait(1)


class GmailAltThreadlistConversationPageSet(page_set_module.PageSet):

  """ Chrome Endure test for GMail. """

  def __init__(self):
    super(GmailAltThreadlistConversationPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='desktop',
      archive_data_file='data/gmail_alt_threadlist_conversation.json',
      bucket=page_set_module.PUBLIC_BUCKET)

    self.AddPage(GmailAltThreadlistConversationPage(self))
