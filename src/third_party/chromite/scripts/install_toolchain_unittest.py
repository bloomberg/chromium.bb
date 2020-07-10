# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test install_toolchain."""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import sysroot_lib
from chromite.scripts import install_toolchain


class ParseArgsTest(cros_test_lib.TempDirTestCase):
  """Tests for argument parsing and validation rules."""

  # pylint: disable=protected-access
  def setUp(self):
    D = cros_test_lib.Directory
    filesystem = (
        D('build', (
            D('board', (
                D('etc', (
                    'make.conf.board_setup',
                )),
            )),
        )),
    )
    cros_test_lib.CreateOnDiskHierarchy(self.tempdir, filesystem)

    self.chost_value = 'chost'
    osutils.WriteFile(os.path.join(self.tempdir,
                                   'build/board/etc/make.conf.board_setup'),
                      'CHOST="%s"' % self.chost_value)

    self.sysroot_dir = os.path.join(self.tempdir, 'build/board')

  def testInvalidArgs(self):
    """Test invalid argument parsing."""
    with self.assertRaises(SystemExit):
      install_toolchain._ParseArgs([])

  def testSysrootCreate(self):
    """Test the sysroot is correctly built."""
    opts = install_toolchain._ParseArgs(['--sysroot', self.sysroot_dir])
    self.assertIsInstance(opts.sysroot, sysroot_lib.Sysroot)
    self.assertEqual(self.sysroot_dir, opts.sysroot.path)

  def testToolchain(self):
    """Test toolchain arg precedence/fallback."""
    toolchain = 'toolchain'
    opts = install_toolchain._ParseArgs(['--sysroot', self.sysroot_dir,
                                         '--toolchain', toolchain])
    self.assertEqual(toolchain, opts.toolchain)

    opts = install_toolchain._ParseArgs(['--sysroot', self.sysroot_dir])
    self.assertEqual(self.chost_value, opts.toolchain)
