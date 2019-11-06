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

  def testCopyFileSegment(self):
    """Test copying on a simple file segment."""
    a = os.path.join(self.tempdir, 'a.txt')
    osutils.WriteFile(a, '789')
    b = os.path.join(self.tempdir, 'b.txt')
    osutils.WriteFile(b, '0123')
    filelib.CopyFileSegment(a, 'r', 2, b, 'r+', in_seek=1)
    self.assertEqual(osutils.ReadFile(b), '8923')
