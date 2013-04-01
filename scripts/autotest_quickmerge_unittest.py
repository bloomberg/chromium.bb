#!/usr/bin/python

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for autotest_quickmerge."""

from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.scripts import autotest_quickmerge

class RsyncCommandTest(cros_build_lib_unittest.RunCommandTestCase):

  def testRsyncQuickmergeCommand(self):
    """Test that RsyncQuickMerge makes correct call to SudoRunCommand"""
    include_file_name = 'an_include_file_name'
    source_path = 'a_source_path'
    sysroot_path = 'a_sysroot_path'

    expected_command = ['rsync', '-a', '-n', '-u', '-i',
                        '--exclude=**.pyc', '--exclude=**.pyo',
                        '--include-from=%s' % include_file_name,
                        '--exclude=*',
                        source_path,
                        sysroot_path]

    autotest_quickmerge.RsyncQuickmerge(source_path, sysroot_path,
                                        include_file_name,
                                        pretend=True,
                                        overwrite=False,
                                        quiet=False)

    self.assertCommandContains(expected_command)


if __name__ == '__main__':
  cros_test_lib.main()