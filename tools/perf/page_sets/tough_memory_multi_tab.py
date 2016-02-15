# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story


class ToughMemoryMultiTabPage(page_module.Page):

  def __init__(self, url, page_set):
    super(ToughMemoryMultiTabPage, self).__init__(
        url=url, page_set=page_set, credentials_path = 'data/credentials.json',
        shared_page_state_class=shared_page_state.SharedMobilePageState)
    self.archive_data_file = 'data/key_mobile_sites.json'


class ToughMemoryMultiTabPageSet(story.StorySet):

  """ Mobile sites for exercising multi-tab memory issues """

  def __init__(self):
    super(ToughMemoryMultiTabPageSet, self).__init__(
      archive_data_file='data/key_mobile_sites.json')

    urls_list = [
      'https://www.google.co.uk/search?hl=en&q=barack+obama&cad=h',
      'http://theverge.com',
      'http://techcrunch.com'
    ]

    for url in urls_list:
      self.AddStory(ToughMemoryMultiTabPage(url, self))
