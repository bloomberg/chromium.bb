# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import logging

from telemetry.page import shared_page_state


class AndroidScreenRestorationSharedState(shared_page_state.SharedPageState):
  """ Ensures the screen is on before and after each user story is run. """

  def WillRunUserStory(self, page):
    super(AndroidScreenRestorationSharedState, self).WillRunUserStory(page)
    self._EnsureScreenOn()

  def DidRunUserStory(self, results):
    try:
      super(AndroidScreenRestorationSharedState, self).DidRunUserStory(results)
    finally:
      self._EnsureScreenOn()

  def CanRunOnBrowser(self, browser_info):
    if not browser_info.browser_type.startswith('android'):
      logging.warning('Browser is non-Android, skipping test')
      return False
    return True

  def _EnsureScreenOn(self):
    self.platform.android_action_runner.EnsureScreenOn()
