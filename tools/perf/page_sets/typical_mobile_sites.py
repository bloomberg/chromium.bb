# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class TypicalMobileSitesPage(page_module.Page):

  def __init__(self, url, page_set):
    super(TypicalMobileSitesPage, self).__init__(
        url=url, page_set=page_set, credentials_path = 'data/credentials.json')
    self.user_agent_type = 'mobile'
    self.archive_data_file = 'data/typical_mobile_sites.json'

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(5)
    action_runner.ScrollPage()


class TypicalMobileSitesPageSet(page_set_module.PageSet):

  def __init__(self):
    super(TypicalMobileSitesPageSet, self).__init__(
        user_agent_type='mobile',
        archive_data_file='data/typical_mobile_sites.json',
        bucket=page_set_module.PARTNER_BUCKET)

    urls_list = [
       'http://google.com',
       'http://yahoo.com',
       'http://baidu.com',
       'http://cnn.com',
       'http://yandex.ru',
       'http://yahoo.co.jp',
       'http://amazon.com',
       'http://ebay.com',
       'http://bing.com',
       'http://nytimes.com',
       'http://latimes.com',
       'http://chicagotribune.com',
       'http://wikipedia.com',
       'http://sfbay.craigslist.org',
       'http://bbc.co.uk',
       'http://apple.com',
       'http://amazon.co.jp',
       'http://sina.com.cn',
       'http://weibo.com',
    ]

    for url in urls_list:
      self.AddUserStory(TypicalMobileSitesPage(url, self))
