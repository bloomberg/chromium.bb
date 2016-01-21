# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core import exceptions
from telemetry import decorators
from telemetry.testing import tab_test_case

class TabStackTraceTest(tab_test_case.TabTestCase):

  # For now just work on a single platform (mac).
  @decorators.Enabled('mac')
   # Stack traces do not currently work on 10.6, but they are also being
   # disabled shortly so just disable it for now.
  @decorators.Disabled('snowleopard')
  def testStackTrace(self):
    try:
      self._tab.Navigate('chrome://crash', timeout=5)
    except exceptions.DevtoolsTargetCrashException as e:
      self.assertIn('Thread 0 (crashed)', '\n'.join(e.stack_trace))

  # Currently stack traces do not work on windows: http://crbug.com/476110
  # Linux stack traces depends on fission support: http://crbug.com/405623
  @decorators.Enabled('mac')
  @decorators.Disabled('snowleopard')
  def testCrashSymbols(self):
    try:
      self._tab.Navigate('chrome://crash', timeout=5)
    except exceptions.DevtoolsTargetCrashException as e:
      self.assertIn('CrashIntentionally', '\n'.join(e.stack_trace))
