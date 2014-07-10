# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class ToughMemoryMultiTabPage(page_module.Page):

  def __init__(self, url, page_set):
    super(ToughMemoryMultiTabPage, self).__init__(url=url, page_set=page_set)
    self.credentials_path = 'data/credentials.json'
    self.user_agent_type = 'mobile'
    self.archive_data_file = 'data/key_mobile_sites.json'


class ToughMemoryMultiTabPageSet(page_set_module.PageSet):

  """ Mobile sites for exercising multi-tab memory issues """

  def __init__(self):
    super(ToughMemoryMultiTabPageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='mobile',
      archive_data_file='data/key_mobile_sites.json')

    urls_list = [
      'https://www.google.com/#hl=en&q=barack+obama',
      'http://theverge.com',
      'http://techcrunch.com'
    ]

    for url in urls_list:
      self.AddPage(ToughMemoryMultiTabPage(url, self))
