# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class Typical10MobilePage(page_module.Page):

  def __init__(self, url, page_set, name=''):
    super(Typical10MobilePage, self).__init__(
        url=url, page_set=page_set, name=name,
        credentials_path = 'data/credentials.json')
    self.user_agent_type = 'mobile'
    self.archive_data_file = 'data/typical_10_mobile.json'

  def RunPageInteractions(self, action_runner):
    action_runner.Wait(20)
    action_runner.ScrollPage()
    action_runner.Wait(20)


class Typical10MobilePageSet(page_set_module.PageSet):
  """10 typical mobile pages, used for power testing."""

  def __init__(self):
    super(Typical10MobilePageSet, self).__init__(
        user_agent_type='mobile',
        archive_data_file='data/typical_10_mobile.json',
        bucket=page_set_module.PARTNER_BUCKET)

    urls_list = [
        # Why: Top site
        'http://m.facebook.com/barackobama',
        # Why: Wikipedia article with lots of pictures, German language
        'http://de.m.wikipedia.org/wiki/K%C3%B6lner_Dom',
        # Why: current top Q&A on popular Japanese site
        'http://m.chiebukuro.yahoo.co.jp/detail/q10136829180',
        # Why: news article on popular site
        'http://m.huffpost.com/us/entry/6004486',
        # Why: news article on popular site
        'http://www.cnn.com/2014/03/31/showbiz/tv/himym-finale/index.html',
        # Why: Popular RTL language site
        'http://m.ynet.co.il',
        # Why: Popular Russian language site
        'http://www.rg.ru/2014/10/21/cska-site.html',
        # Why: Popular shopping site
        'http://m.ebay.com/itm/351157205404',
        # Why: Popular viral site, lots of images
        'http://siriuslymeg.tumblr.com/',
        # Why: Popular Chinese language site.
        'http://wapbaike.baidu.com/',
    ]

    for url in urls_list:
      self.AddUserStory(Typical10MobilePage(url, self))
