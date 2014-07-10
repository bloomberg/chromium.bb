# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class GmailRefreshPage(page_module.Page):

  """ Why: Continually reload the gmail page. """

  def __init__(self, page_set):
    super(GmailRefreshPage, self).__init__(
      url='https://mail.google.com/mail/',
      page_set=page_set)

    self.credentials_path = 'data/credentials.json'
    self.credentials = 'google'
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/gmail_refresh.json'

  def RunEndure(self, action_runner):
    action_runner.ReloadPage()


class GmailRefreshPageSet(page_set_module.PageSet):

  """
  Description: Chrome Endure control test to test gmail page reload
  """

  def __init__(self):
    super(GmailRefreshPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='desktop',
      archive_data_file='data/gmail_refresh.json',
      bucket=page_set_module.PUBLIC_BUCKET)

    self.AddPage(GmailRefreshPage(self))
