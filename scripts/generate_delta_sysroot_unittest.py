#!/usr/bin/python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..'))

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.scripts import generate_delta_sysroot as gds


# pylint: disable=W0212
def _Parse(argv):
  return gds._ParseCommandLine(argv)


class InterfaceTest(cros_test_lib.OutputTestCase,
                    cros_test_lib.TempDirTestCase):
  """Test the commandline interface of the script"""

  def testNoBoard(self):
    """Test no board specified."""
    argv = ['--out-dir', '/path/to/nowhere']
    self.assertParseError(argv)

  def testNoOutDir(self):
    """Test no out dir specified."""
    argv = ['--board', 'link']
    self.assertParseError(argv)

  def testCorrectArgv(self):
    """Test successful parsing"""
    argv = ['--board', 'link', '--out-dir', self.tempdir]
    options =  _Parse(argv)
    gds.FinishParsing(options)

  def testTestsSet(self):
    """Test successful parsing"""
    argv = ['--board', 'link', '--out-dir', self.tempdir]
    options =  _Parse(argv)
    self.assertTrue(options.build_tests)

  def testNoTestsSet(self):
    """Test successful parsing"""
    argv = ['--board', 'link', '--out-dir', self.tempdir, '--skip-tests']
    options =  _Parse(argv)
    self.assertFalse(options.build_tests)

  def assertParseError(self, argv):
    """Helper to assert parsing error, given argv."""
    with self.OutputCapturer():
      self.assertRaises2(SystemExit, _Parse, argv)


class TestCreateBatchFile(cros_test_lib.TempDirTestCase):
  """Test the batch file creation."""

  def testSourceDirDoesNotExist(self):
    """Test error is raised if there is no source directory."""
    no_source = os.path.join(self.tempdir, 'foo/bar/cow')

    self.assertRaises2(cros_build_lib.RunCommandError, gds.CreateBatchFile,
        no_source, self.tempdir, os.path.join(self.tempdir,'batch'))


if __name__ == '__main__':
  cros_test_lib.main()
