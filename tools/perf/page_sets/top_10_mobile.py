# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story


URL_LIST = [
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


class Top10MobilePage(page_module.Page):

  def __init__(self, url, page_set, run_no_page_interactions,
               collect_memory_dumps):
    super(Top10MobilePage, self).__init__(
        url=url, page_set=page_set, credentials_path = 'data/credentials.json',
        shared_page_state_class=shared_page_state.SharedMobilePageState)
    self.archive_data_file = 'data/top_10_mobile.json'
    self._run_no_page_interactions = run_no_page_interactions
    self._collect_memory_dumps = collect_memory_dumps

  def RunPageInteractions(self, action_runner):
    if self._collect_memory_dumps:
      action_runner.tab.browser.DumpMemory()
    if self._run_no_page_interactions:
      return
    with action_runner.CreateGestureInteraction('ScrollAction'):
      action_runner.ScrollPage()
    if self._collect_memory_dumps:
      action_runner.tab.browser.DumpMemory()


class _Top10MobilePageSet(story.StorySet):
  """ Base class for Top 10 mobile sites """

  def __init__(self, run_no_page_interactions=False,
               collect_memory_dumps=False):
    super(_Top10MobilePageSet, self).__init__(
      archive_data_file='data/top_10_mobile.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)

    for url in URL_LIST:
      self.AddStory(Top10MobilePage(url, self, run_no_page_interactions,
        collect_memory_dumps))


class Top10MobilePageSet(_Top10MobilePageSet):
  """ Top 10 mobile sites """

  def __init__(self, run_no_page_interactions=False):
    super(Top10MobilePageSet, self).__init__(run_no_page_interactions)


class Top10MobileMemoryPageSet(_Top10MobilePageSet):
  """ Top 10 mobile sites for measuring memory"""

  def __init__(self):
    super(Top10MobileMemoryPageSet, self).__init__(
        run_no_page_interactions=False, collect_memory_dumps=True)

