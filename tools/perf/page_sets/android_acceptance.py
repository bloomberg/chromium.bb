# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class AndroidAcceptancePage(page_module.Page):

  def __init__(self, url, page_set, name=''):
    super(AndroidAcceptancePage, self).__init__(
        url=url, page_set=page_set, name=name,
        credentials_path = 'data/credentials.json')
    self.user_agent_type = 'desktop'
    self.archive_data_file = 'data/android_acceptance.json'

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(40)


class AndroidAcceptancePageSet(page_set_module.PageSet):

  """ Pages used in android acceptance testing. """

  def __init__(self):
    super(AndroidAcceptancePageSet, self).__init__(
      user_agent_type='desktop', # Android acceptance uses desktop.
      archive_data_file='data/android_acceptance.json',
      bucket=page_set_module.PARTNER_BUCKET)

    urls_list = [
       'http://www.amazon.com',
       'http://www.cnn.com',
       'http://www.msn.com',
    ]

    for url in urls_list:
      self.AddUserStory(AndroidAcceptancePage(url, self))
