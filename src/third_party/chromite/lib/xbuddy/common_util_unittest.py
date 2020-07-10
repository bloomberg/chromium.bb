# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for common_util module."""

from __future__ import print_function

import os
import shutil
import tempfile

from chromite.lib import cros_test_lib
from chromite.lib.xbuddy import common_util
from chromite.lib.xbuddy import devserver_constants


# Fake Dev Server Layout:
TEST_LAYOUT = {
    'test-board-1': ['R17-1413.0.0-a1-b1346', 'R17-18.0.0-a1-b1346'],
    'test-board-2': ['R16-2241.0.0-a0-b2', 'R17-2.0.0-a1-b1346'],
    'test-board-3': []
}



class CommonUtilTest(cros_test_lib.TestCase):
  """Base class for CommonUtil tests."""

  def setUp(self):
    self._static_dir = tempfile.mkdtemp('common_util_unittest')
    self._outside_sandbox_dir = tempfile.mkdtemp('common_util_unittest')

    # Set up some basic existing structure used by GetLatest* tests.
    for board, builds in TEST_LAYOUT.items():
      board_path = os.path.join(self._static_dir, board)
      os.mkdir(board_path)
      for build in builds:
        build_path = os.path.join(board_path, build)
        os.mkdir(build_path)
        with open(os.path.join(
            build_path, devserver_constants.TEST_IMAGE_FILE), 'w') as f:
          f.write('TEST_IMAGE_FILE')
        with open(os.path.join(
            build_path, devserver_constants.STATEFUL_FILE), 'w') as f:
          f.write('STATEFUL_FILE')
        with open(os.path.join(
            build_path, devserver_constants.UPDATE_FILE), 'w') as f:
          f.write('UPDATE_FILE')

  def tearDown(self):
    shutil.rmtree(self._static_dir)
    shutil.rmtree(self._outside_sandbox_dir)

  def testPathInDir(self):
    """Various tests around the PathInDir test."""
    # Path is in sandbox.
    self.assertTrue(
        common_util.PathInDir(
            self._static_dir, os.path.join(self._static_dir, 'some-board')))

    # Path is sandbox.
    self.assertFalse(
        common_util.PathInDir(self._static_dir, self._static_dir))

    # Path is outside the sandbox.
    self.assertFalse(common_util.PathInDir(
        self._static_dir, self._outside_sandbox_dir))

    # Path contains '..'.
    self.assertFalse(
        common_util.PathInDir(
            self._static_dir, os.path.join(self._static_dir, os.pardir)))

    # Path contains symbolic link references.
    os.chdir(self._static_dir)
    os.symlink(os.pardir, 'parent')
    self.assertFalse(
        common_util.PathInDir(
            self._static_dir, os.path.join(self._static_dir, os.pardir)))

  def testGetLatestBuildVersion(self):
    """Tests that the latest version is correct given our setup."""
    self.assertEqual(
        common_util.GetLatestBuildVersion(self._static_dir, 'test-board-1'),
        'R17-1413.0.0-a1-b1346')

  def testGetLatestBuildVersionLatest(self):
    """Test that we raise CommonUtilError when a build dir is empty."""
    self.assertRaises(common_util.CommonUtilError,
                      common_util.GetLatestBuildVersion,
                      self._static_dir, 'test-board-3')

  def testGetLatestBuildVersionUnknownBuild(self):
    """Test that we raise CommonUtilError when a build dir does not exist."""
    self.assertRaises(common_util.CommonUtilError,
                      common_util.GetLatestBuildVersion,
                      self._static_dir, 'bad-dir')

  def testGetLatestBuildVersionMilestone(self):
    """Test that we can get builds based on milestone."""
    expected_build_str = 'R16-2241.0.0-a0-b2'
    milestone = 'R16'
    build_str = common_util.GetLatestBuildVersion(
        self._static_dir, 'test-board-2', milestone)
    self.assertEqual(expected_build_str, build_str)

  def testGetControlFile(self):
    """Creates a fake control file and verifies that we can get it."""
    control_file_dir = os.path.join(
        self._static_dir, 'test-board-1', 'R17-1413.0.0-a1-b1346', 'autotest',
        'server', 'site_tests', 'network_VPN')
    os.makedirs(control_file_dir)
    with open(os.path.join(control_file_dir, 'control'), 'w') as f:
      f.write('hello!')

    control_content = common_util.GetControlFile(
        self._static_dir, 'test-board-1/R17-1413.0.0-a1-b1346',
        os.path.join('server', 'site_tests', 'network_VPN', 'control'))
    self.assertEqual(control_content, 'hello!')

  def testSymlinkFile(self):
    link_fd, link_base = tempfile.mkstemp(prefix='common-symlink-test')
    link_a = link_base + '-link-a'
    link_b = link_base + '-link-b'

    # Create the "link a" --> "base".
    common_util.SymlinkFile(link_base, link_a)
    self.assertTrue(os.path.lexists(link_a))
    self.assertEqual(os.readlink(link_a), link_base)

    # Create the "link b" --> "base".
    common_util.SymlinkFile(link_base, link_b)
    self.assertTrue(os.path.lexists(link_b))
    self.assertEqual(os.readlink(link_b), link_base)

    # Replace the existing "link b" to point to "link a".
    common_util.SymlinkFile(link_a, link_b)
    self.assertTrue(os.path.lexists(link_b))
    self.assertEqual(os.readlink(link_b), link_a)

    os.close(link_fd)
    os.unlink(link_b)
    os.unlink(link_a)
    os.unlink(link_base)

  def testExtractTarballMissingFile(self):
    """Verify that stderr from tar is printed if in encounters an error."""
    tarball = 'a-tarball-which-does-not-exist.tar.gz'
    dest = self._static_dir

    try:
      common_util.ExtractTarball(tarball, dest)
    except common_util.CommonUtilError as e:
      # Check to see that tar's error message is printed in the exception.
      self.assertTrue('Cannot open: No such file or directory' in e.args[0],
                      ("tar's stderr is missing from the exception.\n%s" %
                       e.args[0]))
