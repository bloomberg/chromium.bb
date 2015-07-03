# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core import util
from telemetry.page import page as page_module
from telemetry.page import shared_page_state
from telemetry import story

util.AddDirToPythonPath(util.GetChromiumSrcDir(), 'build', 'android')
from pylib.constants import keyevent # pylint: disable=import-error
from pylib.device import intent # pylint: disable=import-error


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

  def __init__(self, story_set, url):
    super(ForegroundPage, self).__init__(
        url=url, page_set=story_set, name=url,
        shared_page_state_class=shared_page_state.SharedMobilePageState)
    self.archive_data_file = story_set.archive_data_file

  def RunPageInteractions(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    with action_runner.CreateInteraction('measurement'):
      # TODO(perezju): This should catch a few memory dumps. When available,
      # use the dump API to request dumps on demand crbug.com/505826
      action_runner.Wait(7)


class BackgroundPage(page_module.Page):
  """Take a measurement while Chrome is in the background."""

  def __init__(self, story_set, number):
    super(BackgroundPage, self).__init__(
        url='about:blank', page_set=story_set,
        name='chrome_background_%d' % number,
        shared_page_state_class=shared_page_state.SharedMobilePageState)
    self.archive_data_file = story_set.archive_data_file

  def RunPageInteractions(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()

    # launch clock app, pushing Chrome to the background
    android_platform = action_runner.tab.browser.platform
    android_platform.LaunchAndroidApplication(
        intent.Intent(package='com.google.android.deskclock',
                      activity='com.android.deskclock.DeskClock'),
        app_has_webviews=False)

    # take measurement
    with action_runner.CreateInteraction('measurement'):
      # TODO(perezju): This should catch a few memory dumps. When available,
      # use the dump API to request dumps on demand crbug.com/505826
      action_runner.Wait(7)

    # go back to Chrome
    android_platform.android_action_runner.InputKeyEvent(keyevent.KEYCODE_BACK)


class MemoryHealthStory(story.StorySet):
  """User story for the Memory Health Plan."""

  def __init__(self):
    super(MemoryHealthStory, self).__init__(
        archive_data_file='data/memory_health_plan.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    for number, url in enumerate(URL_LIST, 1):
      self.AddStory(ForegroundPage(self, url))
      self.AddStory(BackgroundPage(self, number))
