# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test service.

Handles test related functionality.
"""

from __future__ import print_function

import os
import re

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import failures_lib
from chromite.lib import moblab_vm
from chromite.lib import osutils
from chromite.lib import portage_util


class Error(Exception):
  """The module's base error class."""


class BuildTargetUnitTestResult(object):
  """Result value object."""

  def __init__(self, return_code, failed_cpvs):
    """Init method.

    Args:
      return_code (int): The return code from the command execution.
      failed_cpvs (list[portage_util.CPV]|None): List of packages whose tests
        failed.
    """
    self.return_code = return_code
    self.failed_cpvs = failed_cpvs or []

  @property
  def success(self):
    return self.return_code == 0 and len(self.failed_cpvs) == 0


def BuildTargetUnitTest(build_target, chroot, blacklist=None, was_built=True):
  """Run the ebuild unit tests for the target.

  Args:
    build_target (build_target_util.BuildTarget): The build target.
    chroot (chroot_lib.Chroot): The chroot where the tests are running.
    blacklist (list[str]|None): Tests to skip.
    was_built (bool): Whether packages were built.

  Returns:
    BuildTargetUnitTestResult
  """
  # TODO(saklein) Refactor commands.RunUnitTests to use this/the API.
  # TODO(crbug.com/960805) Move cros_run_unit_tests logic here.
  cmd = ['cros_run_unit_tests', '--board', build_target.name]

  if blacklist:
    cmd.extend(['--blacklist_packages', ' '.join(blacklist)])

  if not was_built:
    cmd.append('--assume-empty-sysroot')

  # Set up the parallel emerge status file.
  extra_env = chroot.env
  base_dir = os.path.join(chroot.path, 'tmp')
  with osutils.TempDir(base_dir=base_dir) as tempdir:
    full_sf_path = os.path.join(tempdir, 'status_file')
    chroot_sf_path = full_sf_path.replace(chroot.path, '')
    extra_env[constants.PARALLEL_EMERGE_STATUS_FILE_ENVVAR] = chroot_sf_path

    result = cros_build_lib.RunCommand(cmd, enter_chroot=True,
                                       extra_env=extra_env,
                                       chroot_args=chroot.GetEnterArgs(),
                                       error_code_ok=True)

    failed_pkgs = portage_util.ParseParallelEmergeStatusFile(full_sf_path)

  return BuildTargetUnitTestResult(result.returncode, failed_pkgs)


def BuildTargetUnitTestTarball(chroot, sysroot, result_path):
  """Build the unittest tarball.

  Args:
    chroot (chroot_lib.Chroot): Chroot where the tests were run.
    sysroot (sysroot_lib.Sysroot): The sysroot where the tests were run.
    result_path (str): The directory where the archive should be created.
  """
  tarball = 'unit_tests.tar'
  tarball_path = os.path.join(result_path, tarball)

  cwd = os.path.join(chroot.path, sysroot.path.lstrip(os.sep),
                     constants.UNITTEST_PKG_PATH)

  result = cros_build_lib.CreateTarball(tarball_path, cwd, chroot=chroot.path,
                                        compression=cros_build_lib.COMP_NONE,
                                        error_code_ok=True)

  return tarball_path if result.returncode == 0 else None


def DebugInfoTest(sysroot_path):
  """Run the debug info tests.

  Args:
    sysroot_path (str): The sysroot being tested.

  Returns:
    bool: True iff all tests passed, False otherwise.
  """
  cmd = ['debug_info_test', os.path.join(sysroot_path, 'usr/lib/debug')]
  result = cros_build_lib.RunCommand(cmd, enter_chroot=True, error_code_ok=True)

  return result.returncode == 0


def CreateMoblabVm(workspace_dir, chroot_dir, image_dir):
  """Create the moblab VMs.

  Assumes that image_dir is in exactly the state it was after building
  a test image and then converting it to a VM image.

  Args:
    workspace_dir (str): Workspace for the moblab VM.
    chroot_dir (str): Directory containing the chroot for the moblab VM.
    image_dir (str): Directory containing the VM image.

  Returns:
    MoblabVm: The resulting VM.
  """
  vms = moblab_vm.MoblabVm(workspace_dir, chroot_dir=chroot_dir)
  vms.Create(image_dir, dut_image_dir=image_dir, create_vm_images=False)
  return vms


def PrepareMoblabVmImageCache(vms, builder, payload_dirs):
  """Preload the given payloads into the moblab VM image cache.

  Args:
    vms (MoblabVm): The Moblab VM.
    builder (str): The builder path, used to name the cache dir.
    payload_dirs (list[str]): List of payload directories to load.

  Returns:
    str: Absolute path to the image cache path.
  """
  with vms.MountedMoblabDiskContext() as disk_dir:
    image_cache_root = os.path.join(disk_dir, 'static/prefetched')
    # If by any chance this path exists, the permission bits are surely
    # nonsense, since 'moblab' user doesn't exist on the host system.
    osutils.RmDir(image_cache_root, ignore_missing=True, sudo=True)

    image_cache_dir = os.path.join(image_cache_root, builder)
    osutils.SafeMakedirsNonRoot(image_cache_dir)
    for payload_dir in payload_dirs:
      osutils.CopyDirContents(payload_dir, image_cache_dir, allow_nonempty=True)

  image_cache_rel_dir = image_cache_dir[len(disk_dir):].strip('/')
  return os.path.join('/', 'mnt/moblab', image_cache_rel_dir)


def RunMoblabVmTest(chroot, vms, builder, image_cache_dir, results_dir):
  """Run Moblab VM tests.

  Args:
    chroot (chroot_lib.Chroot): The chroot in which to run tests.
    builder (str): The builder path, used to find artifacts on GS.
    vms (MoblabVm): The Moblab VMs to test.
    image_cache_dir (str): Path to artifacts cache.
    results_dir (str): Path to output test results.
  """
  with vms.RunVmsContext():
    # TODO(evanhernandez): Move many of these arguments to test config.
    test_args = [
        # moblab in VM takes longer to bring up all upstart services on first
        # boot than on physical machines.
        'services_init_timeout_m=10',
        'target_build="%s"' % builder,
        'test_timeout_hint_m=90',
        'clear_devserver_cache=False',
        'image_storage_server="%s"' % (image_cache_dir.rstrip('/') + '/'),
    ]
    cros_build_lib.RunCommand(
        [
            'test_that',
            '--no-quickmerge',
            '--results_dir', results_dir,
            '-b', 'moblab-generic-vm',
            'localhost:%s' % vms.moblab_ssh_port,
            'moblab_DummyServerNoSspSuite',
            '--args', ' '.join(test_args),
        ],
        enter_chroot=True,
        chroot_args=chroot.get_enter_args(),
    )


def ValidateMoblabVmTest(results_dir):
  """Determine if the VM test passed or not.

  Args:
    results_dir (str): Path to directory containing test_that results.

  Raises:
    failures_lib.TestFailure: If dummy_PassServer did not run or failed.
  """
  log_file = os.path.join(results_dir, 'debug', 'test_that.INFO')
  if not os.path.isfile(log_file):
    raise failures_lib.TestFailure('Found no test_that logs at %s' % log_file)

  log_file_contents = osutils.ReadFile(log_file)
  if not re.match(r'dummy_PassServer\s*\[\s*PASSED\s*]', log_file_contents):
    raise failures_lib.TestFailure('Moblab run_suite succeeded, but did '
                                   'not successfully run dummy_PassServer.')
