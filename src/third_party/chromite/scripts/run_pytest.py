# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper to execute pytest inside the chromite virtualenv."""

from __future__ import print_function

import argparse
import os
import sys

import pytest  # pylint: disable=import-error

from chromite.lib import cgroups
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import gs
from chromite.lib import cros_logging as logging
from chromite.lib import namespaces


def main(argv):
  parser = get_parser()
  opts, pytest_args = parser.parse_known_args()
  if opts.quick:
    if not cros_build_lib.IsInsideChroot() and opts.chroot:
      logging.warning('Tests start up faster when run from inside the chroot.')

  if opts.chroot:
    ensure_chroot_exists()
    re_execute_inside_chroot(argv)
  else:
    os.chdir(constants.CHROMITE_DIR)
    pytest_args += ['--no-chroot']

  # This is a cheesy hack to make sure gsutil is populated in the cache before
  # we run tests. This is a partial workaround for crbug.com/468838.
  gs.GSContext.GetDefaultGSUtilBin()

  if opts.quick:
    logging.info('Skipping test namespacing due to --quickstart.')
    # Default to running in a single process under --quickstart. User args can
    # still override this.
    pytest_args = ['-n', '0'] + pytest_args
  else:
    # Namespacing is enabled by default because tests may break each other or
    # interfere with parts of the running system if not isolated in a namespace.
    # Disabling namespaces is not recommended for general use.
    re_execute_with_namespace([sys.argv[0]] + argv)

  sys.exit(pytest.main(pytest_args))


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


def get_parser():
  """Build the parser for command line arguments."""
  parser = argparse.ArgumentParser(
      description=__doc__,
      epilog='To see the help output for pytest, run `pytest --help` inside '
      'the chroot.',
  )
  parser.add_argument(
      '--quickstart',
      dest='quick',
      action='store_true',
      help='Skip normal test sandboxing and namespacing for faster start up '
      'time.',
  )
  parser.add_argument(
      '--no-chroot',
      dest='chroot',
      action='store_false',
      help="Don't initialize or enter a chroot for the test invocation. May "
      'cause tests to unexpectedly fail!',
  )
  return parser
