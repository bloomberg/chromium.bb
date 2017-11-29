# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets import page_cycler_story
from telemetry import story
from telemetry.page import cache_temperature as cache_temperature_module

cache_temperatures = [
    cache_temperature_module.COLD,
    cache_temperature_module.WARM_BROWSER,
    cache_temperature_module.HOT_BROWSER,
]

temperature_warmup_run_count = {
  cache_temperature_module.COLD: 0,
  cache_temperature_module.WARM_BROWSER: 1,
  cache_temperature_module.HOT_BROWSER: 2,
}

# This URL list is a migration of a non-telemetry existing V8 benchmark.
urls_list = [
    'https://www.google.de/search?q=v8',
    'https://www.youtube.com',
    'https://www.youtube.com/watch?v=_kZsOISarzg',
    'https://www.facebook.com/shakira',
    'http://www.baidu.com/s?wd=v8',
    'http://www.yahoo.co.jp',
    'http://www.amazon.com/s/?field-keywords=v8',
    # pylint: disable=line-too-long
    'http://hi.wikipedia.org/wiki/%E0%A4%AE%E0%A5%81%E0%A4%96%E0%A4%AA%E0%A5%83%E0%A4%B7%E0%A5%8D%E0%A4%A0',
    'https://en.wikipedia.org/w/index.php?title=Barack_Obama&veaction=edit',
    'http://www.qq.com',
    'http://www.twitter.com/taylorswift13',
    'http://www.reddit.com',
    'http://www.ebay.fr/sch/i.html?_nkw=v8',
    'http://edition.cnn.com',
    'http://world.taobao.com',
    'http://www.instagram.com/archdigest',
    'https://www.linkedin.com/m/',
    'http://www.msn.com/ar-ae',
    'http://www.bing.com/search?q=v8+engine',
    'http://www.pinterest.com/categories/popular',
    # 'http://www.sina.com.cn', http://crbug.com/699579
    'http://weibo.com',
    'http://yandex.ru/search/?text=v8',
    'http://www.wikiwand.com/en/hill',
    'http://meta.discourse.org',
    'http://reddit.musicplayer.io',
    'http://inbox.google.com',
    'http://maps.google.co.jp/maps/search/restaurant+tokyo',
    'https://adwords.google.com',
    'http://pollouer.muc/Speedometer/CustomRunner.html?angular',
    'http://pollouer.muc/Speedometer/CustomRunner.html?jquery',
    'http://pollouer.muc/Speedometer/CustomRunner.html?backbone',
    'http://pollouer.muc/Speedometer/CustomRunner.html?ember',
    'http://pollouer.muc/Speedometer/CustomRunner.html?vanilla',
    'https://cdn.ampproject.org/c/www.bbc.co.uk/news/amp/37344292#log=3',
]

class V8Top25Story(page_cycler_story.PageCyclerStory):

  def __init__(self, url, page_set, name='',
               cache_temperature=cache_temperature_module.ANY):
    super(V8Top25Story, self).__init__(
        url=url, page_set=page_set, name=name,
        cache_temperature=cache_temperature)
    self.remaining_warmups = temperature_warmup_run_count[cache_temperature]

  def RunPageInteractions(self, action_runner):
    if self.remaining_warmups == 0:
        # We wait for 20 seconds to make sure we capture enough information
        # to calculate the interactive time correctly.
        action_runner.Wait(20)
    else:
        self.remaining_warmups -= 1

class V8Top25StorySet(story.StorySet):
  """~25 of top pages, used for v8 testing. They represent popular websites as
  well as other pages the V8 team wants to track due to their unique
  characteristics."""

  def __init__(self):
    super(V8Top25StorySet, self).__init__(
        archive_data_file='data/v8_top_25.json',
        cloud_storage_bucket=story.INTERNAL_BUCKET)

    for url in urls_list:
      for temp in cache_temperatures:
        self.AddStory(V8Top25Story(url, self, url, cache_temperature=temp))
