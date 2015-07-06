# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story


class GmailRefreshPage(page_module.Page):

  """ Why: Continually reload the gmail page. """

  def __init__(self, page_set):
    super(GmailRefreshPage, self).__init__(
      url='https://mail.google.com/mail/',
      shared_page_state_class=shared_page_state.SharedDesktopPageState,
      page_set=page_set, credentials_path = 'data/credentials.json')
    self.credentials = 'google'
    self.archive_data_file = 'data/gmail_refresh.json'

  def RunEndure(self, action_runner):
    action_runner.ReloadPage()


class GmailRefreshPageSet(story.StorySet):

  """
  Description: Chrome Endure control test to test gmail page reload
  """

  def __init__(self):
    super(GmailRefreshPageSet, self).__init__(
      archive_data_file='data/gmail_refresh.json',
      cloud_storage_bucket=story.PUBLIC_BUCKET)

    self.AddStory(GmailRefreshPage(self))
