# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import page_set as page_set_module


class Top10MobilePage(page_module.Page):

  def __init__(self, url, page_set):
    super(Top10MobilePage, self).__init__(url=url, page_set=page_set)
    self.credentials_path = 'data/credentials.json'
    self.user_agent_type = 'mobile'
    self.archive_data_file = 'data/top_10_mobile.json'

  def RunSmoothness(self, action_runner):
    interaction = action_runner.BeginGestureInteraction(
        'ScrollAction', is_smooth=True)
    action_runner.ScrollPage()
    interaction.End()


class Top10MobilePageSet(page_set_module.PageSet):

  """ Top 10 mobile sites """

  def __init__(self):
    super(Top10MobilePageSet, self).__init__(
      credentials_path='data/credentials.json',
      user_agent_type='mobile',
      archive_data_file='data/top_10_mobile.json',
      bucket=page_set_module.PARTNER_BUCKET)

    urls_list = [
      # Why: #1 (Alexa) most visited page worldwide, picked a reasonable
      # search term
      'https://www.google.com/#hl=en&q=science',
      # Why: #2 (Alexa) most visited page worldwide, picked the most liked
      # page
      'https://m.facebook.com/rihanna',
      # Why: #3 (Alexa) most visited page worldwide, picked a reasonable
      # search term
      'http://m.youtube.com/results?q=science',
      # Why: #4 (Alexa) most visited page worldwide, picked a reasonable search
      # term
      'http://search.yahoo.com/search;_ylt=?p=google',
      # Why: #5 (Alexa) most visited page worldwide, picked a reasonable search
      # term
      'http://www.baidu.com/s?word=google',
      # Why: #6 (Alexa) most visited page worldwide, picked a reasonable page
      'http://en.m.wikipedia.org/wiki/Science',
      # Why: #10 (Alexa) most visited page worldwide, picked the most followed
      # user
      'https://mobile.twitter.com/justinbieber?skip_interstitial=true',
      # Why: #11 (Alexa) most visited page worldwide, picked a reasonable
      # page
      'http://www.amazon.com/gp/aw/s/?k=nexus',
      # Why: #13 (Alexa) most visited page worldwide, picked the first real
      # page
      'http://m.taobao.com/channel/act/mobile/20131111-women.html',
      # Why: #18 (Alexa) most visited page worldwide, picked a reasonable
      # search term
      'http://yandex.ru/touchsearch?text=science',
    ]

    for url in urls_list:
      self.AddPage(Top10MobilePage(url, self))
