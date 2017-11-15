# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import py_utils
from telemetry import story
from telemetry.page import page as page_module

class LeakDetectionPage(page_module.Page):
  def __init__(self, url, page_set, name=''):
    super(LeakDetectionPage, self).__init__(
      url=url, page_set=page_set, name=name)

  def RunNavigateSteps(self, action_runner):
    action_runner.Navigate('about:blank')
    action_runner.PrepareForLeakDetection()
    action_runner.MeasureMemory()
    action_runner.Navigate(self.url)
    py_utils.WaitFor(action_runner.tab.HasReachedQuiescence, timeout=30)
    action_runner.Navigate('about:blank')
    action_runner.PrepareForLeakDetection()
    action_runner.MeasureMemory()

class LeakDetectionStorySet(story.StorySet):
  def __init__(self):
    super(LeakDetectionStorySet, self).__init__(
      archive_data_file='data/leak_detection.json',
      cloud_storage_bucket=story.PARTNER_BUCKET)
    urls_list = [
      # Alexa top websites
      'https://www.google.com',
      'https://www.youtube.com',
      'https://www.facebook.com',
      'https://www.baidu.com',
      'https://www.wikipedia.org',
      'https://www.yahoo.com',
      'https://www.reddit.com',
      'http://www.qq.com',
      # websites which found to be leaking in the past
      'https://www.macys.com',
      'https://www.citi.com',
      'http://www.time.com',
      'http://infomoney.com.br',
      'http://www.cheapoair.com',
      'http://www.quora.com',
      'http://www.onlinedown.net',
      'http://www.dailypost.ng',
      'http://www.listindiario.com',
      'http://www.aljazeera.net',
    ]
    for url in urls_list:
      self.AddStory(LeakDetectionPage(url, self, url))
