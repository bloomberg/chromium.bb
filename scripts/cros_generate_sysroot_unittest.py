#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mock
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..'))
from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.scripts import cros_generate_sysroot as cros_gen
from chromite.lib import osutils
from chromite.lib import partial_mock


Dir = cros_test_lib.Directory


class CrosGenMock(partial_mock.PartialMock):
  TARGET = 'chromite.scripts.cros_generate_sysroot.GenerateSysroot'
  ATTRS = ('_InstallToolchain', '_InstallKernelHeaders',
           '_InstallBuildDependencies')

  TOOLCHAIN = 'toolchain'
  KERNEL_HEADERS = 'kernel_headers'
  BUILD_DEPS = 'build-deps'

  def _InstallToolchain(self, inst):
    osutils.Touch(os.path.join(inst.sysroot, self.TOOLCHAIN))

  def _InstallKernelHeaders(self, inst):
    osutils.Touch(os.path.join(inst.sysroot, self.KERNEL_HEADERS))

  def _InstallBuildDependencies(self, inst):
    osutils.Touch(os.path.join(inst.sysroot, self.BUILD_DEPS))

  def VerifyTarball(self, tarball):
    dir_struct = [Dir('.', []), self.TOOLCHAIN, self.KERNEL_HEADERS,
                  self.BUILD_DEPS]
    cros_test_lib.VerifyTarball(tarball, dir_struct)


BOARD = 'lumpy'
TAR_NAME = 'test.tar.xz'


class OverallTest(cros_test_lib.TempDirTestCase):

  def setUp(self):
    self.cg_mock = CrosGenMock()
    self.cg_mock.Start()

  def tearDown(self):
    self.cg_mock.Stop()
    mock.patch.stopall()

  def testTarballGeneration(self):
    """End-to-end test of tarball generation."""
    mock.patch.object(cros_build_lib, 'IsInsideChroot').start()
    cros_build_lib.IsInsideChroot.returnvalue = True
    cros_gen.main(
        ['--board', BOARD, '--out-dir', self.tempdir,
         '--out-file', TAR_NAME, '--package', constants.CHROME_CP])
    self.cg_mock.VerifyTarball(os.path.join(self.tempdir, TAR_NAME))


class InterfaceTest(cros_test_lib.TempDirTestCase):
  """Test Parsing and error checking functionality."""

  BAD_TARGET_DIR = '/path/to/nowhere'

  def _Parse(self, extra_args):
    return cros_gen.ParseCommandLine(
        ['--board', BOARD, '--out-dir', self.tempdir,
         '--package', constants.CHROME_CP] + extra_args)

  def testDefaultTargetName(self):
    """We are getting the right default target name."""
    options = self._Parse([])
    self.assertEquals(
        options.out_file, 'sysroot_chromeos-base_chromeos-chrome.tar.xz')

  def testExistingTarget(self):
    """Erroring out on pre-existing target."""
    options = self._Parse(['--out-file', TAR_NAME])
    osutils.Touch(os.path.join(self.tempdir, TAR_NAME))
    self.assertRaises(cros_build_lib.DieSystemExit,
                      cros_gen.FinishParsing, options)

  def testNonExisting(self):
    """Erroring out on non-existent output dir."""
    options = cros_gen.ParseCommandLine(
        ['--board', BOARD, '--out-dir', self.BAD_TARGET_DIR, '--package',
         constants.CHROME_CP])
    self.assertRaises(cros_build_lib.DieSystemExit,
                      cros_gen.FinishParsing, options)


if __name__ == '__main__':
  cros_test_lib.main()
