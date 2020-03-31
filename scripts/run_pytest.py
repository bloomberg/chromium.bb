# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper to execute pytest inside the chromite virtualenv."""

from __future__ import print_function

import os
import sys

import pytest  # pylint: disable=import-error

from chromite.lib import cgroups
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import gs
from chromite.lib import namespaces


def main(argv):
  ensure_chroot_exists()
  re_execute_inside_chroot(argv)

  # This is a cheesy hack to make sure gsutil is populated in the cache before
  # we run tests. This is a partial workaround for crbug.com/468838.
  gs.GSContext.GetDefaultGSUtilBin()

  re_execute_with_namespace([sys.argv[0]] + argv)

  sys.exit(pytest.main(argv))


def re_execute_with_namespace(argv, network=False):
  """Re-execute as root so we can unshare resources."""
  if os.geteuid() != 0:
    cmd = [
        'sudo',
        'HOME=%s' % os.environ['HOME'],
        'PATH=%s' % os.environ['PATH'],
        '--',
    ] + argv
    os.execvp(cmd[0], cmd)
  else:
    cgroups.Cgroup.InitSystem()
    namespaces.SimpleUnshare(net=not network, pid=True)
    # We got our namespaces, so switch back to the user to run the tests.
    gid = int(os.environ.pop('SUDO_GID'))
    uid = int(os.environ.pop('SUDO_UID'))
    user = os.environ.pop('SUDO_USER')
    os.initgroups(user, gid)
    os.setresgid(gid, gid, gid)
    os.setresuid(uid, uid, uid)
    os.environ['USER'] = user


def re_execute_inside_chroot(argv):
  """Re-execute the test wrapper inside the chroot."""
  cmd = [
      'cros_sdk',
      '--',
      os.path.join('..', '..', 'chromite', 'run_pytest'),
  ]
  if not cros_build_lib.IsInsideChroot():
    os.execvp(cmd[0], cmd + argv)
  else:
    os.chdir(constants.CHROMITE_DIR)


def ensure_chroot_exists():
  """Ensure that a chroot exists for us to run tests in."""
  chroot = os.path.join(constants.SOURCE_ROOT, constants.DEFAULT_CHROOT_DIR)
  if not os.path.exists(chroot) and not cros_build_lib.IsInsideChroot():
    cros_build_lib.run(['cros_sdk', '--create'])
