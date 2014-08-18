#!/usr/bin/python
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for dep_tracker.py."""

import os

from chromite.lib import cros_test_lib
from chromite.lib import unittest_lib
from chromite.scripts import dep_tracker

# Allow access private members for testing:
# pylint: disable=W0212


class MainTest(cros_test_lib.MoxOutputTestCase):
  """Tests for the main() function."""

  def testHelp(self):
    """Test that --help is functioning."""
    argv = [ '--help' ]

    with self.OutputCapturer() as output:
      # Running with --help should exit with code==0.
      self.AssertFuncSystemExitZero(dep_tracker.main, argv)

    # Verify that a message beginning with "usage: " was printed.
    stdout = output.GetStdout()
    self.assertTrue(stdout.startswith('usage: '))


class DepTrackerTest(cros_test_lib.TempDirTestCase):
  """Tests for the DepTracker() class."""

  def testSimpleDep(self):
    unittest_lib.BuildELF(os.path.join(self.tempdir, 'libabc.so'),
                          ['func_a', 'func_b', 'func_c'])
    unittest_lib.BuildELF(os.path.join(self.tempdir, 'abc_main'),
                          undefined_symbols=['func_b'],
                          used_libs=['abc'],
                          executable=True)
    dt = dep_tracker.DepTracker(self.tempdir)
    dt.Init()
    dt.ComputeELFFileDeps()

    self.assertEquals(sorted(dt._files.keys()), ['abc_main', 'libabc.so'])


if __name__ == '__main__':
  cros_test_lib.main()
