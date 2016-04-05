# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core import exceptions
from telemetry import decorators
from telemetry.testing import tab_test_case


class TabStackTraceTest(tab_test_case.TabTestCase):

  # TODO(dyen): For now this doesn't work on Android but continue to
  # expand this.
  # TODO(kbr): this test doesn't work on the Windows Swarming bots
  # yet. http://crbug.com/561763
  @decorators.Enabled('mac', 'linux')
  # Stack traces do not currently work on 10.6, but they are also being
  # disabled shortly so just disable it for now.
  @decorators.Disabled('snowleopard')
  def testStackTrace(self):
    try:
      self._tab.Navigate('chrome://crash', timeout=5)
    except exceptions.DevtoolsTargetCrashException as e:
      self.assertIn('Thread 0 (crashed)', '\n'.join(e.stack_trace))

  # Stack traces aren't working on Android yet.
  # TODO(kbr): this test doesn't work on the Windows Swarming bots
  # yet. http://crbug.com/561763
  @decorators.Enabled('mac', 'linux')
  @decorators.Disabled('snowleopard')
  def testCrashSymbols(self):
    try:
      self._tab.Navigate('chrome://crash', timeout=5)
    except exceptions.DevtoolsTargetCrashException as e:
      self.assertIn('CrashIntentionally', '\n'.join(e.stack_trace))
