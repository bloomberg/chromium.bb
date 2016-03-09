# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re

from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story

from devil.android.sdk import intent # pylint: disable=import-error
from devil.android.sdk import keyevent # pylint: disable=import-error


DUMP_WAIT_TIME = 3

URL_LIST = [
    'http://google.com',
    'http://vimeo.com',
    'http://yahoo.com',
    'http://baidu.com',
    'http://cnn.com',
    'http://yandex.ru',
    'http://yahoo.co.jp',
    'http://amazon.com',
    'http://ebay.com',
    'http://bing.com',
]


class ForegroundPage(page_module.Page):
  """Take a measurement after loading a regular webpage."""

  def __init__(self, story_set, name, url):
    super(ForegroundPage, self).__init__(
        url=url, page_set=story_set, name=name,
        shared_page_state_class=shared_page_state.SharedMobilePageState)

  def _TakeMemoryMeasurement(self, action_runner, phase):
    action_runner.Wait(1)  # See crbug.com/540022#c17.
    with action_runner.CreateInteraction(phase):
      action_runner.Wait(DUMP_WAIT_TIME)
      action_runner.ForceGarbageCollection()
      action_runner.tab.browser.platform.FlushEntireSystemCache()
      action_runner.Wait(DUMP_WAIT_TIME)
      if not action_runner.tab.browser.DumpMemory():
        logging.error('Unable to get a memory dump for %s.', self.name)

  def RunPageInteractions(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    self._TakeMemoryMeasurement(action_runner, 'foreground')


class BackgroundPage(ForegroundPage):
  """Take a measurement while Chrome is in the background."""

  def __init__(self, story_set, name):
    super(BackgroundPage, self).__init__(story_set, name, 'about:blank')

  def RunPageInteractions(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()

    # Launch clock app, pushing Chrome to the background.
    android_platform = action_runner.tab.browser.platform
    android_platform.LaunchAndroidApplication(
        intent.Intent(package='com.google.android.deskclock',
                      activity='com.android.deskclock.DeskClock'),
        app_has_webviews=False)

    # Take measurement.
    self._TakeMemoryMeasurement(action_runner, 'background')

    # Go back to Chrome.
    android_platform.android_action_runner.InputKeyEvent(keyevent.KEYCODE_BACK)


class MemoryHealthStory(story.StorySet):
  """User story for the Memory Health Plan."""

  def __init__(self):
    super(MemoryHealthStory, self).__init__(
        archive_data_file='data/memory_health_plan.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    for url in URL_LIST:
      # We name pages so their foreground/background counterparts are easy
      # to identify. For example 'http://google.com' becomes
      # 'http_google_com' and 'after_http_google_com' respectively.
      name = re.sub('\W+', '_', url)
      self.AddStory(ForegroundPage(self, name, url))
      self.AddStory(BackgroundPage(self, 'after_' + name))
