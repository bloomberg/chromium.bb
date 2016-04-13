# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import tempfile
import os

from telemetry.core import exceptions
from telemetry import decorators
from telemetry.testing import tab_test_case


class TabStackTraceTest(tab_test_case.TabTestCase):

  # TODO(dyen): For now this doesn't work on Android but continue to
  # expand this.
  # TODO(kbr): currently failing on Windows because the symbolized
  # stack trace format is unexpected. http://crbug.com/561763
  @decorators.Enabled('mac', 'linux')
  # Stack traces do not currently work on 10.6, but they are also being
  # disabled shortly so just disable it for now.
  @decorators.Disabled('snowleopard')
  def testStackTrace(self):
    with self.assertRaises(exceptions.DevtoolsTargetCrashException) as c:
      self._tab.Navigate('chrome://crash', timeout=5)
      self.assertIn('Thread 0 (crashed)', '\n'.join(c.exception.stack_trace))

  # Stack traces aren't working on Android yet.
  @decorators.Enabled('mac', 'linux', 'win')
  @decorators.Disabled('snowleopard')
  def testCrashSymbols(self):
    with self.assertRaises(exceptions.DevtoolsTargetCrashException) as c:
      self._tab.Navigate('chrome://crash', timeout=5)
      self.assertIn('CrashIntentionally', '\n'.join(c.exception.stack_trace))

  # The breakpad file specific test only apply to platforms which use the
  # breakpad symbol format. This also must be tested in isolation because it can
  # potentially interfere with other tests symbol parsing.
  @decorators.Enabled('mac', 'linux')
  @decorators.Isolated
  def testBadBreakpadFileIgnored(self):
    # pylint: disable=protected-access
    executable_path = self._browser._browser_backend._executable
    executable = os.path.basename(executable_path)
    with tempfile.NamedTemporaryFile(mode='wt',
                                     dir=os.path.dirname(executable_path),
                                     prefix=executable + '.breakpad',
                                     delete=True) as f:
      f.write('Garbage Data 012345')
      f.flush()
      with self.assertRaises(exceptions.DevtoolsTargetCrashException) as c:
        self._tab.Navigate('chrome://crash', timeout=5)
        # The symbol directory should now contain our breakpad file.
        tmp_dir = os.path.join(self._browser._browser_backend._tmp_minidump_dir)

        # Symbol directory should have been created.
        symbol_dir = os.path.join(tmp_dir, 'symbols', executable)
        self.assertTrue(os.path.isdir(symbol_dir))

        # A single symbol file should still exist here.
        self.assertEqual(1, len(os.listdir(symbol_dir)))

        # Stack trace should still work.
        self.assertIn('CrashIntentionally', '\n'.join(c.exception.stack_trace))
