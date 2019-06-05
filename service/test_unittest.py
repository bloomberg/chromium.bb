# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for test service."""

from __future__ import print_function

import contextlib
import mock
import os

from chromite.lib import cros_test_lib
from chromite.lib import failures_lib
from chromite.lib import moblab_vm
from chromite.lib import osutils
from chromite.service import test


class MoblabVmTestCase(cros_test_lib.RunCommandTempDirTestCase):
  """Tests for the SetupBoardRunConfig class."""

  def MockDirectory(self, path):
    """Create an empty directory.

    Args:
      path (str): Relative path for the directory.

    Returns:
      str: Path to the directory.
    """
    path = os.path.join(self.tempdir, path)
    osutils.SafeMakedirs(path)
    return path

  def setUp(self):
    self.builder = 'moblab-generic-vm/R12-3.4.5-67-890'
    self.image_dir = self.MockDirectory('files/image')
    self.payload_dir = self.MockDirectory('files/payload')
    self.results_dir = self.MockDirectory('results')
    self.vms = moblab_vm.MoblabVm(self.tempdir)


class CreateMoblabVmTest(MoblabVmTestCase):
  """Unit tests for CreateMoblabVm."""

  def setUp(self):
    self.mock_vm_create = self.PatchObject(moblab_vm.MoblabVm, 'Create')

  def testBasic(self):
    vms = test.CreateMoblabVm(self.tempdir, self.image_dir)
    self.assertEqual(vms.workspace, self.tempdir)
    self.assertEqual(
        self.mock_vm_create.call_args_list,
        [mock.call(self.image_dir, create_vm_images=False)])


class PrepareMoblabVmImageCacheTest(MoblabVmTestCase):
  """Unit tests for PrepareMoblabVmImageCache."""

  def setUp(self):
    @contextlib.contextmanager
    def MountedMoblabDiskContextMock(*args, **kwargs):
      yield self.tempdir
    self.PatchObject(moblab_vm.MoblabVm, 'MountedMoblabDiskContext',
                     MountedMoblabDiskContextMock)

    self.payload_file_name = 'payload.bin'
    self.payload_file = os.path.join(self.payload_dir, self.payload_file_name)
    self.payload_file_content = 'A Lannister always pays his debts.'
    osutils.WriteFile(os.path.join(self.payload_dir, self.payload_file_name),
                      self.payload_file_content)

  def testBasic(self):
    """PrepareMoblabVmImageCache loads all payloads into the vm."""
    image_cache_dir = test.PrepareMoblabVmImageCache(self.vms, self.builder,
                                                     [self.payload_dir])
    expected_cache_dir = 'static/prefetched/moblab-generic-vm/R12-3.4.5-67-890'
    self.assertEqual(image_cache_dir,
                     os.path.join('/mnt/moblab/', expected_cache_dir))

    copied_payload_file = os.path.join(self.tempdir, expected_cache_dir,
                                       self.payload_file_name)
    self.assertTrue(os.path.exists(copied_payload_file))
    self.assertEqual(osutils.ReadFile(copied_payload_file),
                     self.payload_file_content)


class RunMoblabVmTestTest(MoblabVmTestCase):
  """Unit tests for RunMoblabVmTestTest."""

  def setUp(self):
    self.image_cache_dir = '/mnt/moblab/whatever'
    self.PatchObject(moblab_vm.MoblabVm, 'Start')
    self.PatchObject(moblab_vm.MoblabVm, 'Stop')

  def testBasic(self):
    """RunMoblabVmTest calls test_that with correct args."""
    test.RunMoblabVmTest(self.vms, self.builder, self.image_cache_dir,
                         self.results_dir)
    self.assertCommandContains([
        'test_that', '--no-quickmerge',
        '--results_dir', self.results_dir,
        '-b', 'moblab-generic-vm',
        'moblab_DummyServerNoSspSuite',
        '--args',
        'services_init_timeout_m=10 '
        'target_build="%s" '
        'test_timeout_hint_m=90 '
        'clear_devserver_cache=False '
        'image_storage_server="%s"' % (self.builder,
                                       self.image_cache_dir + '/'),
    ])


class ValidateMoblabVmTestTest(MoblabVmTestCase):
  """Unit tests for ValidateMoblabVmTest."""

  def setUp(self):
    self.logs_dir = os.path.join(self.results_dir, 'debug')
    osutils.SafeMakedirs(self.logs_dir)
    self.logs_file = os.path.join(self.logs_dir, 'test_that.INFO')

  def testValidateMoblabVmTestSuccess(self):
    """ValidateMoblabVmTest does not die when tests succeeded."""
    osutils.WriteFile(self.logs_file, 'dummy_PassServer [PASSED]')
    test.ValidateMoblabVmTest(self.results_dir)

  def testValidateMoblabVmTestNoLogs(self):
    """ValidateMoblabVmTest dies when test_that logs not present."""
    self.assertRaises(failures_lib.TestFailure,
                      test.ValidateMoblabVmTest, self.results_dir)

  def testValidateMoblabVmTestFailure(self):
    """ValidateMoblabVmTest dies when tests failed."""
    osutils.WriteFile(self.logs_file, 'dummy_PassServer [FAILED]')
    self.assertRaises(failures_lib.TestFailure,
                      test.ValidateMoblabVmTest, self.results_dir)
