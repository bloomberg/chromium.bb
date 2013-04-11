#!/usr/bin/python

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for autotest_quickmerge."""

import os
import sys
import unittest


sys.path.insert(0, os.path.abspath('%s/../..' % os.path.dirname(__file__)))
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.scripts import autotest_quickmerge


RSYNC_TEST_OUTPUT = """.d..t...... ./
>f..t...... touched file with spaces
>f..t...... touched_file
>f.st...... modified_contents_file
.f...p..... modified_permissions_file
.f....o.... modified_owner_file
>f+++++++++ new_file
cL+++++++++ new_symlink -> directory_a/new_file_in_directory
.d..t...... directory_a/
>f+++++++++ directory_a/new_file_in_directory
>f..t...... directory_a/touched_file_in_directory
cd+++++++++ new_empty_directory/
.d..t...... touched_empty_directory/"""
# The output format of rsync's itemized changes has a few unusual cases
# that are ambiguous. For instance, if the operation involved creating a
# symbolic link named "a -> b" to a file named "c", the rsync output would be:
# cL+++++++++ a -> b -> c
# which is indistinguishable from the output for creating a symbolic link named
# "a" to a file named "b -> c".
# Since there is no easy resolution to this ambiguity, and it seems like a case
# that would rarely or never be encountered in the wild, rsync quickmerge
# will exclude all files which contain the substring " -> " in their name.

class ItemizeChangesFromRsyncOutput(unittest.TestCase):

  def testItemizeChangesFromRsyncOutput(self):
    """Test that rsync output parser returns correct FileMutations."""
    destination_path = '/foo/bar'

    expected_new = set(
        [('>f+++++++++', '/foo/bar/new_file'),
         ('>f+++++++++', '/foo/bar/directory_a/new_file_in_directory'),
         ('cL+++++++++', '/foo/bar/new_symlink')])

    expected_mod = set(
        [('>f..t......', '/foo/bar/touched file with spaces'),
         ('>f..t......', '/foo/bar/touched_file'),
         ('>f.st......', '/foo/bar/modified_contents_file'),
         ('.f...p.....', '/foo/bar/modified_permissions_file'),
         ('.f....o....', '/foo/bar/modified_owner_file'),
         ('>f..t......', '/foo/bar/directory_a/touched_file_in_directory')])

    expected_dir = set([('cd+++++++++', '/foo/bar/new_empty_directory/')])

    report = autotest_quickmerge.ItemizeChangesFromRsyncOutput(
        RSYNC_TEST_OUTPUT, destination_path)

    self.assertEqual(expected_new, set(report.new_files))
    self.assertEqual(expected_mod, set(report.modified_files))
    self.assertEqual(expected_dir, set(report.new_directories))


class RsyncCommandTest(cros_build_lib_unittest.RunCommandTestCase):

  def testRsyncQuickmergeCommand(self):
    """Test that RsyncQuickMerge makes correct call to SudoRunCommand"""
    include_file_name = 'an_include_file_name'
    source_path = 'a_source_path'
    sysroot_path = 'a_sysroot_path'

    expected_command = ['rsync', '-a', '-n', '-u', '-i',
                        '--exclude=**.pyc', '--exclude=**.pyo',
                        '--exclude=** -> *',
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
