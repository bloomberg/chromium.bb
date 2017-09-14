# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry import story
from devil.android.sdk import intent  # pylint: disable=import-error
from contrib.vr_benchmarks.vr_sample_page import VrSamplePage


class Simple2dStillPage(VrSamplePage):
  """A simple 2D page without user interactions."""

  def __init__(self, page_set):
    super(Simple2dStillPage, self).__init__(
         sample_page='index', page_set=page_set)

  def RunPageInteractions(self, action_runner):

    def isNfcAppReady(android_app):
      del android_app
      # TODO(tiborg): Find a way to tell if the NFC app ran successfully.
      return True

    # Enter VR by simulating an NFC tag scan.
    self.platform.LaunchAndroidApplication(
        start_intent=intent.Intent(
            component=
            'org.chromium.chrome.browser.vr_shell.nfc_apk/.SimNfcActivity'),
        is_app_ready_predicate=isNfcAppReady,
        app_has_webviews=False).Close()

    # Wait until Chrome is settled in VR.
    # TODO(tiborg): Implement signal indicating that Chrome went into VR
    # Browsing Mode. Wait times are flaky.
    action_runner.Wait(2)

    with action_runner.CreateInteraction('Still'):
      action_runner.Wait(5)


class VrBrowsingModePageSet(story.StorySet):
  """Pageset for VR Browsing Mode tests."""

  def __init__(self):
    super(VrBrowsingModePageSet, self).__init__()
    self.AddStory(Simple2dStillPage(self))
