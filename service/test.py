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

from chromite.lib import cros_build_lib
from chromite.lib import failures_lib
from chromite.lib import moblab_vm
from chromite.lib import osutils


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


def CreateMoblabVm(workspace_dir, image_dir):
  """Create the moblab VMs.

  Assumes that image_dir is in exactly the state it was after building
  a test image and then converting it to a VM image.

  Args:
    workspace_dir (str): Workspace for the moblab VM.
    image_dir (str): Directory containing the VM image.

  Returns:
    MoblabVm: The resulting VM.
  """
  vms = moblab_vm.MoblabVm(workspace_dir)
  vms.Create(image_dir, create_vm_images=False)
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
      osutils.CopyDirContents(payload_dir, image_cache_dir)

  image_cache_rel_dir = image_cache_dir[len(disk_dir):].strip('/')
  return os.path.join('/', 'mnt/moblab', image_cache_rel_dir)


def RunMoblabVmTest(vms, builder, image_cache_dir, results_dir):
  """Run Moblab VM tests.

  Args:
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
        ])


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
