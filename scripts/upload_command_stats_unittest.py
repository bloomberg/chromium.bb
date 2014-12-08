# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for upload_command_stats_unittest.py."""

from __future__ import print_function

import os

from chromite.cbuildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import stats
from chromite.lib import stats_unittest
from chromite.scripts import upload_command_stats

TEST_FILE = """\
Chromium OS Build Command Stats - Version 1
cpu_count 32
cmd_args --board=lumpy
host typewriter.mtv.corp.google.com
run_time 0
cmd_line ./build_packages --board=lumpy
username monkey@chromium.org
cmd_base build_packages
cpu_type Intel(R) Xeon(R) CPU E5-2690 0 @ 2.90GHz
board lumpy
"""

class RunScriptTest(cros_test_lib.MockTempDirTestCase,
                    cros_test_lib.LoggingTestCase):
  """Test the main functionality."""
  # pylint: disable=W0212

  def setUp(self):
    self.upload_file = os.path.join(self.tempdir, 'upload_File')
    osutils.WriteFile(self.upload_file, TEST_FILE)
    self.argv = [self.upload_file]
    self.PatchObject(cros_build_lib, 'GetHostDomain', autospec=True,
                     return_value='noname.com')
    self.StartPatcher(stats_unittest.StatsUploaderMock())

  def testNormalRun(self):
    """Going for code coverage."""
    upload_command_stats.main(self.argv)
    self.assertEquals(stats.StatsUploader._Upload.call_count, 1)

  def testStatsDebugMsg(self, golo=False):
    """We hide debug messages from stats module when not in golo."""
    stats.StatsUploader._Upload.side_effect = EnvironmentError()
    with cros_test_lib.LoggingCapturer() as logs:
      upload_command_stats.main(self.argv)
      self.AssertLogsContain(
          logs, stats.StatsUploader.ENVIRONMENT_ERROR, inverted=(not golo))

  def testGoloRun(self):
    """Test when running in the golo."""
    cros_build_lib.GetHostDomain.return_value = constants.GOLO_DOMAIN
    upload_command_stats.main(self.argv)
    self.assertEquals(stats.StatsUploader._Upload.call_count, 1)
    self.testStatsDebugMsg(golo=True)

  def LogContainsOnError(self, msg):
    """Verifies a logging.error() message is printed."""
    with cros_test_lib.LoggingCapturer() as logs:
      self.assertRaises2(SystemExit, upload_command_stats.main, self.argv,
                         check_attrs={'code': 1})
      self.AssertLogsContain(logs, msg)

  def testLoadFileErrorIgnore(self):
    """We don't propagate timeouts during upload."""
    self.PatchObject(
        upload_command_stats.StatsLoader, 'LoadFile',
        side_effect=upload_command_stats.LoadError(), autospec=True)
    self.LogContainsOnError(
        upload_command_stats.FILE_LOAD_ERROR % self.upload_file)

  def testUploadErrorIgnore(self):
    """We don't propagate timeouts during upload."""
    stats.StatsUploader._Upload.side_effect = Exception()
    # Logging level for the error is logging.ERROR.
    self.LogContainsOnError(upload_command_stats.UNCAUGHT_ERROR)
