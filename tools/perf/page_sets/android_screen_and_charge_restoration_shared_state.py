# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from telemetry.page import shared_page_state


class AndroidScreenAndChargeRestorationSharedState(
    shared_page_state.SharedMobilePageState):
  """
  Ensures the screen is on and the power charging before and after each user
  story is run.
  """

  def WillRunStory(self, page):
    super(AndroidScreenAndChargeRestorationSharedState, self).WillRunStory(page)
    self._EnsureScreenOnAndCharging()

  def DidRunStory(self, results):
    try:
      super(AndroidScreenAndChargeRestorationSharedState, self).DidRunStory(
          results)
    finally:
      self._EnsureScreenOnAndCharging()

  def CanRunOnBrowser(self, browser_info, _):
    if not browser_info.browser_type.startswith('android'):
      logging.warning('Browser is non-Android, skipping test')
      return False
    return True

  def _EnsureScreenOnAndCharging(self):
    self.platform.android_action_runner.EnsureScreenOn()
    self.platform.android_action_runner.SetCharging(True)
