# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for updating and building in the chroot environment."""

from __future__ import print_function

import contextlib
import os
import sys

from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_sdk_lib
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import sysroot_lib

if cros_build_lib.IsInsideChroot():
  # These import libraries outside chromite. See brbug.com/472.
  from chromite.scripts import cros_list_modified_packages as workon
  from chromite.scripts import cros_setup_toolchains as toolchain


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


_HOST_PKGS = ('virtual/target-sdk', 'world',)

_DEFAULT_MAKE_CONF_USER = """
# This file is useful for doing global (chroot and all board) changes.
# Tweak emerge settings, ebuild env, etc...
#
# Make sure to append variables unless you really want to clobber all
# existing settings.  e.g. You most likely want:
#   FEATURES="${FEATURES} ..."
#   USE="${USE} foo"
# and *not*:
#   USE="foo"
#
# This also is a good place to setup ACCEPT_LICENSE.
"""


def _GetToolchainPackages():
  """Get a list of host toolchain packages."""
  # Load crossdev cache first for faster performance.
  toolchain.Crossdev.Load(False)
  packages = toolchain.GetTargetPackages('host')
  return [toolchain.GetPortagePackage('host', x) for x in packages]


def GetEmergeCommand(sysroot=None):
  """Returns the emerge command to use for |sysroot| (host if None)."""
  cmd = [os.path.join(constants.CHROMITE_BIN_DIR, 'parallel_emerge')]
  if sysroot and sysroot != '/':
    cmd += ['--sysroot=%s' % sysroot]
  return cmd


def Emerge(packages, sysroot, with_deps=True, rebuild_deps=True,
           use_binary=True, jobs=None, debug_output=False):
  """Emerge the specified |packages|.

  Args:
    packages: List of packages to emerge.
    sysroot: Path to the sysroot in which to emerge.
    with_deps: Whether to include dependencies.
    rebuild_deps: Whether to rebuild dependencies.
    use_binary: Whether to use binary packages.
    jobs: Number of jobs to run in parallel.
    debug_output: Emit debug level output.

  Raises:
    cros_build_lib.RunCommandError: If emerge returns an error.
  """
  cros_build_lib.AssertInsideChroot()
  if not packages:
    raise ValueError('No packages provided')

  cmd = GetEmergeCommand(sysroot)
  cmd.append('-uNv')

  modified_packages = workon.ListModifiedWorkonPackages(
      sysroot_lib.Sysroot(sysroot))
  if modified_packages is not None:
    mod_pkg_list = ' '.join(modified_packages)
    cmd += ['--reinstall-atoms=' + mod_pkg_list,
            '--usepkg-exclude=' + mod_pkg_list]

  cmd.append('--deep' if with_deps else '--nodeps')
  if use_binary:
    cmd += ['-g', '--with-bdeps=y']
    if sysroot == '/':
      # Only update toolchains in the chroot when binpkgs are available. The
      # toolchain rollout process only takes place when the chromiumos sdk
      # builder finishes a successful build and pushes out binpkgs.
      cmd += ['--useoldpkg-atoms=%s' % ' '.join(_GetToolchainPackages())]

  if rebuild_deps:
    cmd.append('--rebuild-if-unbuilt')
  if jobs:
    cmd.append('--jobs=%d' % jobs)
  if debug_output:
    cmd.append('--show-output')

  # We might build chrome, in which case we need to pass 'CHROME_ORIGIN'.
  cros_build_lib.sudo_run(cmd + packages, preserve_env=True)


def UpdateChroot(board=None, update_host_packages=True):
  """Update the chroot."""
  # Run chroot update hooks.
  logging.notice('Updating the chroot. This may take several minutes.')
  cros_sdk_lib.RunChrootVersionHooks()

  # Update toolchains.
  cmd = [os.path.join(constants.CHROMITE_BIN_DIR, 'cros_setup_toolchains')]
  if board:
    cmd += ['--targets=boards', '--include-boards=%s' % board]
  cros_build_lib.sudo_run(cmd, debug_level=logging.DEBUG)

  # Update the host before updating the board.
  if update_host_packages:
    Emerge(list(_HOST_PKGS), '/', rebuild_deps=False)

  # Automatically discard all CONFIG_PROTECT'ed files. Those that are
  # protected should not be overwritten until the variable is changed.
  # Autodiscard is option "-9" followed by the "YES" confirmation.
  cros_build_lib.sudo_run(['etc-update'], input='-9\nYES\n',
                          debug_level=logging.DEBUG)


def SetupBoard(board, update_chroot=True,
               update_host_packages=True, use_binary=True):
  """Set up a sysroot for |board|.

  This invokes UpdateChroot() with the given board values, unless
  otherwise instructed.

  Args:
    board: Board name to set up a sysroot for.
    update_chroot: Whether we should update the chroot first.
    update_host_packages: Whether to update host packages in the chroot.
    use_binary: If okay to use binary packages during the update.
  """
  if update_chroot:
    UpdateChroot(board=board, update_host_packages=update_host_packages)

  cmd = ['setup_board', '--skip-toolchain-update', '--skip-chroot-upgrade',
         '--board=%s' % board]

  if not use_binary:
    cmd.append('--nousepkg')

  cros_build_lib.run(cmd)


def RunUnittests(sysroot, packages, extra_env=None, verbose=False,
                 retries=None, jobs=None):
  """Runs the unit tests for |packages|.

  Args:
    sysroot: Path to the sysroot to build the tests in.
    packages: List of packages to test.
    extra_env: Python dictionary containing the extra environment variable to
      pass to the build command.
    verbose: If True, show the output from emerge, even when the tests succeed.
    retries: Number of time we should retry a failed packages. If None, use
      parallel_emerge's default.
    jobs: Max number of parallel jobs. (optional)

  Raises:
    RunCommandError if the unit tests failed.
  """
  env = extra_env.copy() if extra_env else {}
  env.update({
      'FEATURES': 'test',
      'PKGDIR': os.path.join(sysroot, constants.UNITTEST_PKG_PATH),
  })

  command = [os.path.join(constants.CHROMITE_BIN_DIR, 'parallel_emerge'),
             '--sysroot=%s' % sysroot]
  if verbose:
    command += ['--show-output']

  if retries is not None:
    command += ['--retries=%s' % retries]

  if jobs is not None:
    command += ['--jobs=%s' % jobs]

  command += list(packages)

  cros_build_lib.sudo_run(command, extra_env=env)


@contextlib.contextmanager
def TempDirInChroot(**kwargs):
  """A context to create and use a tempdir inside the chroot.

  Args:
    prefix: See tempfile.mkdtemp documentation.
    base_dir: The directory to place the temporary directory in the chroot.
    set_global: See osutils.TempDir documentation.
    delete: See osutils.TempDir documentation.
    sudo_rm: See osutils.TempDir documentation.

  Yields:
    A host path (not chroot path) to a tempdir inside the chroot. This tempdir
    is cleaned up when exiting the context.
  """
  base_dir = kwargs.pop('base_dir', '/tmp')
  kwargs['base_dir'] = path_util.FromChrootPath(base_dir)
  tempdir = osutils.TempDir(**kwargs)
  yield tempdir.tempdir


def CreateMakeConfUser():
  """Create default make.conf.user file in the chroot if it does not exist."""
  path = '/etc/make.conf.user'
  if not cros_build_lib.IsInsideChroot():
    path = path_util.FromChrootPath(path)

  if not os.path.exists(path):
    osutils.WriteFile(path, _DEFAULT_MAKE_CONF_USER, sudo=True)
