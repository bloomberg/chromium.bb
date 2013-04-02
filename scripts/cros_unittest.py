#!/usr/bin/python

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

sys.path.insert(0, os.path.abspath('%s/../..' % os.path.dirname(__file__)))
from chromite.lib import cros_test_lib
from chromite.lib import stats
from chromite.lib import stats_unittest
from chromite.scripts import cros


# pylint: disable=W0212


class RunScriptTest(cros_test_lib.MockTempDirTestCase):
  """Test the main functionality."""

  def setUp(self):
    self.StartPatcher(stats_unittest.StatsModuleMock())
    self.PatchObject(cros, '_RunSubCommand', autospec=True)

  def testStatsUpload(self):
    """Test stats uploading."""
    return_value = cros.main(['chrome-sdk', '--board', 'lumpy'])
    self.assertEquals(stats.StatsUploader._Upload.call_count, 1)
    self.assertEquals(return_value, 0)


if __name__ == '__main__':
  cros_test_lib.main()
