# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the filelib module."""

from __future__ import print_function

import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib.paygen import filelib


class TestFileManipulation(cros_test_lib.TempDirTestCase):
  """Test cases for filelib."""

  FILE1 = 'file1a'
  FILE2 = 'file2'
  SUBDIR = 'subdir'
  SUBFILE = '%s/file1b' % SUBDIR
  FILE_GLOB = 'file1*'

  FILE1_CONTENTS = 'Howdy doody there dandy'
  FILE2_CONTENTS = 'Once upon a time in a galaxy far far away.'
  SUBFILE_CONTENTS = 'Five little monkeys jumped on the bed.'

  def _SetUpTempdir(self, tempdir):
    with open(os.path.join(tempdir, self.FILE1), 'w') as out1:
      out1.write(self.FILE1_CONTENTS)

    with open(os.path.join(tempdir, self.FILE2), 'w') as out2:
      out2.write(self.FILE2_CONTENTS)

    subdir = os.path.join(tempdir, self.SUBDIR)
    osutils.SafeMakedirs(subdir)

    with open(os.path.join(tempdir, self.SUBFILE), 'w') as out3:
      out3.write(self.SUBFILE_CONTENTS)

  def testIntegrationScript(self):
    dir1 = os.path.join(self.tempdir, 'filelib_unittest1-')
    osutils.SafeMakedirs(dir1)

    self._SetUpTempdir(dir1)

    dir1_file1 = os.path.join(dir1, self.FILE1)
    dir1_file2 = os.path.join(dir1, self.FILE2)
    dir1_subfile = os.path.join(dir1, self.SUBFILE)
    dir1_top_files = [dir1_file1, dir1_file2]
    dir1_deep_files = dir1_top_files + [dir1_subfile]

    # Test ListFiles with various options.
    self.assertEqual(set(dir1_top_files),
                     set(filelib.ListFiles(dir1)))
    self.assertEqual(set(dir1_deep_files),
                     set(filelib.ListFiles(dir1, recurse=True)))
    self.assertEqual(sorted(dir1_deep_files),
                     filelib.ListFiles(dir1, recurse=True, sort=True))
    self.assertEqual(set([dir1_file1, dir1_subfile]),
                     set(filelib.ListFiles(dir1, recurse=True,
                                           filepattern=self.FILE_GLOB)))


class TestFileLib(cros_test_lib.TempDirTestCase):
  """Test filelib module.

  Note: We use tools for hashes to avoid relying on hashlib since that's what
  the filelib module uses.  We want to verify things rather than have a single
  hashlib bug break both the code and the tests.
  """

  def _MD5Sum(self, file_path):
    """Use RunCommand to get the md5sum of a file."""
    return cros_build_lib.RunCommand(
        ['md5sum', file_path], redirect_stdout=True).output.split(' ')[0]

  def _SHA1Sum(self, file_path):
    """Use sha1sum utility to get SHA1 of a file."""
    # The sha1sum utility gives SHA1 in base 16 encoding.  We need base 64.
    hash16 = cros_build_lib.RunCommand(
        ['sha1sum', file_path], redirect_stdout=True).output.split(' ')[0]
    return hash16.decode('hex').encode('base64').rstrip()

  def _SHA256Sum(self, file_path):
    """Use sha256 utility to get SHA256 of a file."""
    # The sha256sum utility gives SHA256 in base 16 encoding.  We need base 64.
    hash16 = cros_build_lib.RunCommand(
        ['sha256sum', file_path], redirect_stdout=True).output.split(' ')[0]
    return hash16.decode('hex').encode('base64').rstrip()

  def testMD5Sum(self):
    """Test MD5Sum output with the /usr/bin/md5sum binary."""
    file_path = os.path.abspath(__file__)
    self.assertEqual(self._MD5Sum(file_path), filelib.MD5Sum(file_path))

  def testShaSums(self):
    file_path = os.path.abspath(__file__)
    expected_sha1 = self._SHA1Sum(file_path)
    expected_sha256 = self._SHA256Sum(file_path)
    sha1, sha256 = filelib.ShaSums(file_path)
    self.assertEqual(expected_sha1, sha1)
    self.assertEqual(expected_sha256, sha256)

  def testCopyIntoExistingDir(self):
    """Copy a file into a dir that exists."""
    path1 = os.path.join(self.tempdir, 'path1')
    subdir = os.path.join(self.tempdir, 'subdir')
    path2 = os.path.join(subdir, 'path2')

    osutils.Touch(path1)
    osutils.SafeMakedirs(subdir)

    filelib.Copy(path1, path2)
    self.assertExists(path2)

  def testCopyIntoNewDir(self):
    """Copy a file into a dir that does not yet exist."""
    path1 = os.path.join(self.tempdir, 'path1')
    subdir = os.path.join(self.tempdir, 'subdir')
    path2 = os.path.join(subdir, 'path2')

    osutils.Touch(path1)

    filelib.Copy(path1, path2)
    self.assertExists(path2)

  def testCopyRelative(self):
    """Copy a file using relative destination."""
    path1 = os.path.join(self.tempdir, 'path1')
    path2 = os.path.join(self.tempdir, 'path2')
    relative_path = os.path.basename(path2)

    os.chdir(self.tempdir)
    osutils.Touch(path1)

    filelib.Copy(path1, relative_path)
    self.assertExists(path2)
