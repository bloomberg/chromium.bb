# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test cros_choose_profile."""

from __future__ import print_function

import os
import sys

from chromite.lib import commandline
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.scripts import cros_choose_profile


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class ParseArgsTest(cros_test_lib.TestCase):
  """Tests for argument parsing and validation rules."""

  def testInvalidArgs(self):
    """Test invalid argument parsing."""
    with self.assertRaises(SystemExit):
      cros_choose_profile.ParseArgs([])

    with self.assertRaises(SystemExit):
      cros_choose_profile.ParseArgs(['--profile', 'profile',
                                     '--variant', 'variant'])


class BoardTest(cros_test_lib.TestCase):
  """Tests for the Board class logic."""

  def setUp(self):
    """Set up the boards with the different construction variations."""
    # For readability's sake.
    Board = cros_choose_profile.Board
    self.board_variant1 = Board(board='board_variant')
    self.board_variant2 = Board(board='board', variant='variant')
    self.board_variant3 = Board(board_root='/build/board_variant')
    self.board_variant4 = Board(board='board_variant',
                                board_root='/build/ignored_value')

  def testBoardVariant(self):
    """Board.{board, variant, board_variant} building tests."""
    self.assertEqual('board', self.board_variant1.board)
    self.assertEqual('variant', self.board_variant1.variant)
    self.assertEqual('board_variant', self.board_variant1.board_variant)

    self.assertEqual('board', self.board_variant2.board)
    self.assertEqual('variant', self.board_variant2.variant)
    self.assertEqual('board_variant', self.board_variant2.board_variant)

    self.assertEqual('board', self.board_variant3.board)
    self.assertEqual('variant', self.board_variant3.variant)
    self.assertEqual('board_variant', self.board_variant3.board_variant)

    self.assertEqual('board', self.board_variant4.board)
    self.assertEqual('variant', self.board_variant4.variant)
    self.assertEqual('board_variant', self.board_variant4.board_variant)

  def testRoot(self):
    """Board.root tests."""
    self.assertEqual(self.board_variant1.root, self.board_variant2.root)
    self.assertEqual(self.board_variant1.root, self.board_variant3.root)
    self.assertEqual(self.board_variant1.root, self.board_variant4.root)


class ProfileTest(cros_test_lib.TempDirTestCase):
  """Tests for the Profile class and functions, and ChooseProfile."""

  def setUp(self):
    """Setup filesystem for the profile tests."""
    # Make sure everything will use the filesystem we're setting up.
    cros_choose_profile.PathPrefixDecorator.prefix = self.tempdir

    D = cros_test_lib.Directory
    filesystem = (
        D('board1-overlay', (
            D('profiles', (
                D('base', ('parent', )),
                D('profile1', ('parent',)),
                D('profile2', ('parent',)),
            )),
        )),
        D('build', (
            D('board1', (
                D('etc', (
                    D('portage', ()), # make.profile parent directory.
                    'make.conf.board_setup',
                )),
                D('var', (
                    D('cache', (
                        D('edb', ('chromeos',)),
                    )),
                )),
            )),
        )),
    )

    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, filesystem)

    # Generate the required filesystem content.
    # Build out the file names that need content.
    b1_build_root = '/build/board1'
    b1_board_setup = os.path.join(b1_build_root, 'etc/make.conf.board_setup')
    self.b1_sysroot = os.path.join(b1_build_root, 'var/cache/edb/chromeos')
    b1_profiles = '/board1-overlay/profiles'

    base_directory = os.path.join(b1_profiles, 'base')
    base_parent = os.path.join(base_directory, 'parent')
    p1_directory = os.path.join(b1_profiles, 'profile1')
    p1_parent = os.path.join(p1_directory, 'parent')
    p2_directory = os.path.join(b1_profiles, 'profile2')
    p2_parent = os.path.join(p2_directory, 'parent')

    # Contents to write to the corresponding file.

    # self.profile_override is assumed to be a profile name in testGetProfile.
    # Update code there if this changes.
    self.profile_override = 'profile1'
    path_contents = {
        b1_board_setup: 'ARCH="arch"\nBOARD_OVERLAY="/board1-overlay"',
        self.b1_sysroot: 'PROFILE_OVERRIDE="%s"' % self.profile_override,
        base_parent: 'base parent contents',
        p1_parent: 'profile1 parent contents',
        p2_parent: 'profile2 parent contents',
    }

    for filepath, contents in path_contents.items():
      osutils.WriteFile(self._TempdirPath(filepath), contents)

    # Mapping between profile argument and the expected parent contents.
    self.profile_expected_parent = {
        'base': path_contents[base_parent],
        'profile1': path_contents[p1_parent],
        'profile2': path_contents[p2_parent],
    }

    # Mapping between the profile argument and the profile's directory.
    self.profile_directory = {
        'base': base_directory,
        'profile1': p1_directory,
        'profile2': p2_directory,
    }

    # The make profile directory from which parent files are read.
    self.board1_make_profile = '/build/board1/etc/portage/make.profile'

    self.board1 = cros_choose_profile.Board(board_root=b1_build_root)

    osutils.SafeSymlink(self._TempdirPath(p1_directory),
                        self._TempdirPath(self.board1_make_profile))

  def tearDown(self):
    # Reset the prefix.
    cros_choose_profile.PathPrefixDecorator.prefix = None

  def _TempdirPath(self, path):
    """Join the tempdir base path to the given path."""
    # lstrip leading / to prevent it returning the path without the tempdir.
    return os.path.join(self.tempdir, path.lstrip(os.sep))

  def testChooseProfile(self):
    """ChooseProfile tests: verify profiles are properly chosen."""
    b1_parent_path = self._TempdirPath(os.path.join(self.board1_make_profile,
                                                    'parent'))
    # Verify initial state - profile1.
    self.assertEqual(self.profile_expected_parent['profile1'],
                     osutils.ReadFile(b1_parent_path))

    for profile_name, parent in self.profile_expected_parent.items():
      # Call ChooseProfile for the given profile and check contents as specified
      # by self.profile_expected_parent (built in setUp).

      profile_dir = self.profile_directory[profile_name]
      profile = cros_choose_profile.Profile(profile_name, profile_dir,
                                            profile_name)

      # Test the profile changing.
      cros_choose_profile.ChooseProfile(self.board1, profile)
      self.assertEqual(parent, osutils.ReadFile(b1_parent_path))

      # Test the profile staying the same.
      cros_choose_profile.ChooseProfile(self.board1, profile)
      self.assertEqual(parent, osutils.ReadFile(b1_parent_path))

  def testGetProfile(self):
    """Test each profile parameter type behaves as expected when fetched."""
    # pylint: disable=protected-access
    # Test an invalid profile name.
    args = commandline.ArgumentNamespace(profile='doesnotexist')
    self.assertRaises(cros_choose_profile.ProfileDirectoryNotFoundError,
                      cros_choose_profile._GetProfile, args, self.board1)

    # Profile values for following tests.
    profile_name = self.profile_override
    profile_path = self._TempdirPath(self.profile_directory[profile_name])

    # Test using the profile name.
    args = commandline.ArgumentNamespace(profile=profile_name)
    profile = cros_choose_profile._GetProfile(args, self.board1)
    self.assertEqual(profile_name, profile.name)
    self.assertEqual(profile_path, profile.directory)
    self.assertEqual(profile_name, profile.override)

    # Test using the profile path.
    args = commandline.ArgumentNamespace(profile=profile_path)
    profile = cros_choose_profile._GetProfile(args, self.board1)
    self.assertEqual(profile_name, profile.name)
    self.assertEqual(profile_path, profile.directory)
    self.assertEqual(profile_path, profile.override)

    # Test using PROFILE_OVERRIDE.
    args = commandline.ArgumentNamespace(profile=None)
    profile = cros_choose_profile._GetProfile(args, self.board1)
    self.assertEqual(profile_name, profile.name)
    self.assertEqual(profile_path, profile.directory)
    self.assertEqual(self.profile_override, profile.override)

    # No override value, using default 'base'.
    osutils.WriteFile(self._TempdirPath(self.b1_sysroot), '')
    args = commandline.ArgumentNamespace(profile=None)
    profile = cros_choose_profile._GetProfile(opts=args, board=self.board1)
    self.assertEqual('base', profile.name)
    self.assertEqual(self._TempdirPath(self.profile_directory['base']),
                     profile.directory)
    self.assertIsNone(profile.override)
