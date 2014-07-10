# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class GmailExpandCollapseConversationPage(
  page_module.Page):

  """ Why: Expand and Collapse a long conversation. """
  # TODO(edmundyan): Find a long conversation rather than hardcode url

  def __init__(self, page_set):
    super(GmailExpandCollapseConversationPage, self).__init__(
      url='https://mail.google.com/mail/u/0/#inbox/13c6a141fa95ffe0',
      page_set=page_set,
      name='gmail_expand_collapse_conversation')
    self.credentials_path = 'data/credentials.json'
    self.credentials = 'google'
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/gmail_expand_collapse_conversation.json'

  def RunNavigateSteps(self, action_runner):
    action_runner.NavigateToPage(self)
    action_runner.WaitForElement('img[alt="Expand all"]')
    action_runner.ClickElement('img[alt="Expand all"]')
    action_runner.Wait(5)
    action_runner.WaitForElement('img[alt="Collapse all"]')
    action_runner.ClickElement('img[alt="Collapse all"]')
    action_runner.Wait(1)

  def RunEndure(self, action_runner):
    action_runner.WaitForElement('img[alt="Expand all"]')
    action_runner.ClickElement('img[alt="Expand all"]')
    action_runner.Wait(1)
    action_runner.WaitForElement('img[alt="Collapse all"]')
    action_runner.ClickElement('img[alt="Collapse all"]')
    action_runner.Wait(1)


class GmailExpandCollapseConversationPageSet(page_set_module.PageSet):

  """
  Description: Chrome Endure test for GMail.
  """

  def __init__(self):
    super(GmailExpandCollapseConversationPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='desktop',
      archive_data_file='data/gmail_expand_collapse_conversation.json',
      bucket=page_set_module.PUBLIC_BUCKET)

    self.AddPage(GmailExpandCollapseConversationPage(self))
