# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

from telemetry import page
from telemetry import story
from devil.android.sdk import intent  # pylint: disable=import-error
from contrib.vr_benchmarks import shared_android_vr_page_state as vr_state
from contrib.vr_benchmarks.vr_sample_page import VrSamplePage
from contrib.vr_benchmarks.vr_story_set import VrStorySet
from page_sets import top_10_mobile


def _VrBrowsingInteraction(current_page, action_runner):
  def isNfcAppReady(android_app):
    del android_app
    # TODO(tiborg): Find a way to tell if the NFC app ran successfully.
    return True

  # Enter VR by simulating an NFC tag scan.
  current_page.platform.LaunchAndroidApplication(
      start_intent=intent.Intent(
          component=
          'org.chromium.chrome.browser.vr_shell.nfc_apk/.SimNfcActivity'),
      is_app_ready_predicate=isNfcAppReady,
      app_has_webviews=False).Close()

  # Wait until Chrome is settled in VR.
  # TODO(tiborg): Implement signal indicating that Chrome went into VR
  # Browsing Mode. Wait times are flaky.
  action_runner.Wait(2)

  # MeasureMemory() waits for 10 seconds before measuring memory, which is
  # long enough for us to collect our other data, so no additional sleeps
  # necessary.
  action_runner.MeasureMemory(True)


class Simple2dStillPage(VrSamplePage):
  """A simple 2D page without user interactions."""

  def __init__(self, page_set, sample_page='index'):
    super(Simple2dStillPage, self).__init__(
         sample_page=sample_page, page_set=page_set)

  def RunPageInteractions(self, action_runner):
    _VrBrowsingInteraction(self, action_runner)


class VrBrowsingModeWprPage(page.Page):
  """Class for running a VR browsing story on a WPR page."""

  def __init__(self, page_set, url, name, extra_browser_args=None):
    """
    Args:
      page_set: The StorySet the VrBrowsingModeWprPage is being added to
      url: The URL to navigate to for the story
      name: The name of the story
      extra_browser_args: Extra browser args that are simply forwarded to
          page.Page
    """
    super(VrBrowsingModeWprPage, self).__init__(
        url=url,
        page_set=page_set,
        name=name,
        extra_browser_args=extra_browser_args,
        shared_page_state_class=vr_state.SharedAndroidVrPageState)
    self._shared_page_state = None

  def RunPageInteractions(self, action_runner):
    _VrBrowsingInteraction(self, action_runner)

  def Run(self, shared_state):
    self._shared_page_state = shared_state
    super(VrBrowsingModeWprPage, self).Run(shared_state)

  @property
  def platform(self):
    return self._shared_page_state.platform


class VrBrowsingModePageSet(VrStorySet):
  """Pageset for VR Browsing Mode tests on sample pages."""

  def __init__(self, use_fake_pose_tracker=True):
    super(VrBrowsingModePageSet, self).__init__(
        use_fake_pose_tracker=use_fake_pose_tracker)
    self.AddStory(Simple2dStillPage(self))


class VrBrowsingModeWprPageSet(VrStorySet):
  """Pageset for VR browsing mode on WPR recordings of live sites.

  Re-uses the URL list and WPR archive from the memory.top_10_mobile benchmark.
  """

  def __init__(self, use_fake_pose_tracker=True):
    super(VrBrowsingModeWprPageSet, self).__init__(
        archive_data_file='../../page_sets/data/memory_top_10_mobile.json',
        cloud_storage_bucket=story.PARTNER_BUCKET,
        use_fake_pose_tracker=use_fake_pose_tracker)

    for url in top_10_mobile.URL_LIST:
      name = re.sub(r'\W+', '_', url)
      self.AddStory(VrBrowsingModeWprPage(self, url, name))
