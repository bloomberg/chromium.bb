# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the sysroot library."""

from __future__ import print_function

import os

from chromite.lib import chroot_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib import sysroot_lib
from chromite.lib import toolchain


class SysrootLibTest(cros_test_lib.MockTempDirTestCase):
  """Unittest for sysroot_lib.py"""

  def setUp(self):
    """Setup the test environment."""
    # Fake being root to avoid running all filesystem commands with sudo_run.
    self.PatchObject(os, 'getuid', return_value=0)
    self.PatchObject(os, 'geteuid', return_value=0)
    sysroot_path = os.path.join(self.tempdir, 'sysroot')
    osutils.SafeMakedirs(sysroot_path)
    self.sysroot = sysroot_lib.Sysroot(sysroot_path)
    self.relative_sysroot = sysroot_lib.Sysroot('sysroot')

  def testGetStandardField(self):
    """Tests that standard field can be fetched correctly."""
    self.sysroot.WriteConfig('FOO="bar"')
    self.assertEqual('bar', self.sysroot.GetStandardField('FOO'))

    # Works with multiline strings
    multiline = """foo
bar
baz
"""
    self.sysroot.WriteConfig('TEST="%s"' % multiline)
    self.assertEqual(multiline, self.sysroot.GetStandardField('TEST'))

  def testReadWriteCache(self):
    """Tests that we can write and read to the cache."""
    # If a field is not defined we get None.
    self.assertEqual(None, self.sysroot.GetCachedField('foo'))

    # If we set a field, we can get it.
    self.sysroot.SetCachedField('foo', 'bar')
    self.assertEqual('bar', self.sysroot.GetCachedField('foo'))

    # Setting a field in an existing cache preserve the previous values.
    self.sysroot.SetCachedField('hello', 'bonjour')
    self.assertEqual('bar', self.sysroot.GetCachedField('foo'))
    self.assertEqual('bonjour', self.sysroot.GetCachedField('hello'))

    # Setting a field to None unsets it.
    self.sysroot.SetCachedField('hello', None)
    self.assertEqual(None, self.sysroot.GetCachedField('hello'))

  def testErrorOnBadCachedValue(self):
    """Tests that we detect bad value for the sysroot cache."""
    forbidden = [
        'hello"bonjour',
        'hello\\bonjour',
        'hello\nbonjour',
        'hello$bonjour',
        'hello`bonjour',
    ]
    for value in forbidden:
      with self.assertRaises(ValueError):
        self.sysroot.SetCachedField('FOO', value)

  def testGenerateConfigNoToolchainRaisesError(self):
    """Tests _GenerateConfig() with no toolchain raises an error."""
    self.PatchObject(toolchain, 'FilterToolchains', autospec=True,
                     return_value={})

    with self.assertRaises(sysroot_lib.ConfigurationError):
      # pylint: disable=protected-access
      self.sysroot._GenerateConfig({}, ['foo_overlay'], ['foo_overlay'], '')

  def testExists(self):
    """Tests the Exists method."""
    self.assertTrue(self.sysroot.Exists())

    dne_sysroot = sysroot_lib.Sysroot(os.path.join(self.tempdir, 'DNE'))
    self.assertFalse(dne_sysroot.Exists())

  def testExistsInChroot(self):
    """Test the Exists method with a chroot."""
    chroot = chroot_lib.Chroot(self.tempdir)
    self.assertTrue(self.relative_sysroot.Exists(chroot=chroot))

  def testEquals(self):
    """Sanity check for the __eq__ methods."""
    sysroot1 = sysroot_lib.Sysroot(self.tempdir)
    sysroot2 = sysroot_lib.Sysroot(self.tempdir)
    self.assertEqual(sysroot1, sysroot2)


class SysrootLibInstallConfigTest(cros_test_lib.MockTempDirTestCase):
  """Unittest for sysroot_lib.py"""

  # pylint: disable=protected-access

  def setUp(self):
    """Setup the test environment."""
    # Fake being root to avoid running all filesystem commands with sudo_run.
    self.PatchObject(os, 'getuid', return_value=0)
    self.PatchObject(os, 'geteuid', return_value=0)
    self.sysroot = sysroot_lib.Sysroot(self.tempdir)
    self.make_conf_generic_target = os.path.join(self.tempdir,
                                                 'make.conf.generic-target')
    self.make_conf_user = os.path.join(self.tempdir, 'make.conf.user')

    D = cros_test_lib.Directory
    filesystem = (
        D('etc', ()),
        'make.conf.generic-target',
        'make.conf.user',
    )

    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, filesystem)

  def testInstallMakeConf(self):
    """Test make.conf installation."""
    self.PatchObject(sysroot_lib, '_GetMakeConfGenericPath',
                     return_value=self.make_conf_generic_target)

    self.sysroot.InstallMakeConf()

    filepath = os.path.join(self.tempdir, sysroot_lib._MAKE_CONF)
    self.assertExists(filepath)

  def testInstallMakeConfBoard(self):
    """Test make.conf.board installation."""
    self.PatchObject(self.sysroot, 'GenerateBoardMakeConf', return_value='#foo')
    self.PatchObject(self.sysroot, 'GenerateBinhostConf', return_value='#bar')

    self.sysroot.InstallMakeConfBoard()

    filepath = os.path.join(self.tempdir, sysroot_lib._MAKE_CONF_BOARD)
    content = '#foo\n#bar\n'
    self.assertExists(filepath)
    self.assertFileContents(filepath, content)

  def testInstallMakeConfBoardSetup(self):
    """Test make.conf.board_setup installation."""
    self.PatchObject(self.sysroot, 'GenerateBoardSetupConfig',
                     return_value='#foo')

    self.sysroot.InstallMakeConfBoardSetup('board')

    filepath = os.path.join(self.tempdir, sysroot_lib._MAKE_CONF_BOARD_SETUP)
    content = '#foo'
    self.assertExists(filepath)
    self.assertFileContents(filepath, content)

  def testInstallMakeConfUser(self):
    """Test make.conf.user installation."""
    self.PatchObject(sysroot_lib, '_GetChrootMakeConfUserPath',
                     return_value=self.make_conf_user)

    self.sysroot.InstallMakeConfUser()

    filepath = os.path.join(self.tempdir, sysroot_lib._MAKE_CONF_USER)
    self.assertExists(filepath)


class SysrootLibToolchainUpdateTest(cros_test_lib.RunCommandTempDirTestCase):
  """Sysroot.ToolchanUpdate tests."""

  def setUp(self):
    """Setup the test environment."""
    # Fake being root to avoid running commands with sudo_run.
    self.PatchObject(os, 'getuid', return_value=0)
    self.PatchObject(os, 'geteuid', return_value=0)

    self.sysroot = sysroot_lib.Sysroot(self.tempdir)
    self.emerge = os.path.join(constants.CHROMITE_BIN_DIR, 'parallel_emerge')

  def testDefaultUpdateToolchain(self):
    """Test the default path."""
    self.PatchObject(toolchain, 'InstallToolchain')

    self.sysroot.UpdateToolchain('board')
    self.assertCommandContains(
        [self.emerge, '--board=board', '--getbinpkg', '--usepkg'])

  def testNoLocalInitUpdateToolchain(self):
    """Test the nousepkg and not local case."""
    self.PatchObject(toolchain, 'InstallToolchain')

    self.sysroot.UpdateToolchain('board', local_init=False)
    self.assertCommandContains(['--getbinpkg', '--usepkg'], expected=False)
    self.assertCommandContains([self.emerge, '--board=board'])

  def testReUpdateToolchain(self):
    """Test behavior when not running for the first time."""
    self.PatchObject(toolchain, 'InstallToolchain')

    self.PatchObject(self.sysroot, 'IsToolchainInstalled', return_value=True)
    self.sysroot.UpdateToolchain('board')
    self.assertCommandContains([self.emerge], expected=False)

  def testInstallToolchainError(self):
    """Test error handling from the libc install."""
    failed = ['cat/pkg', 'cat/pkg2']
    failed_cpvs = [portage_util.SplitCPV(pkg, strict=False) for pkg in failed]
    result = cros_build_lib.CommandResult(returncode=1)
    error = toolchain.ToolchainInstallError('Error', result=result,
                                            tc_info=failed_cpvs)
    self.PatchObject(toolchain, 'InstallToolchain', side_effect=error)

    try:
      self.sysroot.UpdateToolchain('board')
    except sysroot_lib.ToolchainInstallError as e:
      self.assertTrue(e.failed_toolchain_info)
      self.assertEqual(failed_cpvs, e.failed_toolchain_info)
    except Exception as e:
      self.fail('Unexpected exception raised: %s' % type(e))
    else:
      self.fail('Expected an exception.')

  def testEmergeError(self):
    """Test the emerge error handling."""
    self.PatchObject(toolchain, 'InstallToolchain')
    # pylint: disable=protected-access
    command = self.sysroot._UpdateToolchainCommand('board', True)

    err = cros_build_lib.RunCommandError(
        'Error', cros_build_lib.CommandResult(returncode=1))
    self.rc.AddCmdResult(command, side_effect=err)

    with self.assertRaises(sysroot_lib.ToolchainInstallError):
      self.sysroot.UpdateToolchain('board', local_init=True)
