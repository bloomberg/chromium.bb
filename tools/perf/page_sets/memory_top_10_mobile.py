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

from page_sets import top_10_mobile


DUMP_WAIT_TIME = 3


class MemoryMeasurementPage(page_module.Page):
  """Abstract class for measuring memory on a story unit."""

  _PHASE = NotImplemented

  def __init__(self, story_set, name, url):
    super(MemoryMeasurementPage, self).__init__(
        page_set=story_set, name=name, url=url,
        shared_page_state_class=shared_page_state.SharedMobilePageState,
        grouping_keys={'phase': self._PHASE})

  def _TakeMemoryMeasurement(self, action_runner):
    platform = action_runner.tab.browser.platform
    action_runner.Wait(1)  # See crbug.com/540022#c17.
    if not platform.tracing_controller.is_tracing_running:
      logging.warning('Tracing is off. No memory dumps are being recorded.')
      return  # Tracing is not running, e.g., when recording a WPR archive.
    with action_runner.CreateInteraction(self._PHASE):
      action_runner.Wait(DUMP_WAIT_TIME)
      action_runner.ForceGarbageCollection()
      platform.FlushEntireSystemCache()
      action_runner.Wait(DUMP_WAIT_TIME)
      if not action_runner.tab.browser.DumpMemory():
        logging.error('Unable to get a memory dump for %s.', self.name)


class ForegroundPage(MemoryMeasurementPage):
  """Take a measurement after loading a regular webpage."""

  _PHASE = 'foreground'

  def __init__(self, story_set, name, url):
    super(ForegroundPage, self).__init__(story_set, name, url)

  def RunPageInteractions(self, action_runner):
    action_runner.tab.WaitForDocumentReadyStateToBeComplete()
    self._TakeMemoryMeasurement(action_runner)


class BackgroundPage(MemoryMeasurementPage):
  """Take a measurement while Chrome is in the background."""

  _PHASE = 'background'

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
    self._TakeMemoryMeasurement(action_runner)

    # Go back to Chrome.
    android_platform.android_action_runner.InputKeyEvent(keyevent.KEYCODE_BACK)


class MemoryTop10Mobile(story.StorySet):
  """User story to measure foreground/background memory in top 10 mobile."""

  def __init__(self):
    super(MemoryTop10Mobile, self).__init__(
        archive_data_file='data/memory_top_10_mobile.json',
        cloud_storage_bucket=story.PARTNER_BUCKET)

    for url in top_10_mobile.URL_LIST:
      # We name pages so their foreground/background counterparts are easy
      # to identify. For example 'http://google.com' becomes
      # 'http_google_com' and 'after_http_google_com' respectively.
      name = re.sub('\W+', '_', url)
      self.AddStory(ForegroundPage(self, name, url))
      self.AddStory(BackgroundPage(self, 'after_' + name))
