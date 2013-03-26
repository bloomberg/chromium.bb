#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates a sysroot tarball for building a specific package.

Meant for use after setup_board and build_packages have been run.
"""

import os

from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import commandline
from chromite.lib import osutils
from chromite.lib import sudo

DEFAULT_NAME = 'sysroot_%(package)s.tar.xz'
PACKAGE_SEPARATOR = '/'
SYSROOT = 'sysroot'


def ParseCommandLine(argv):
  """Parse args, and run environment-independent checks."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--board', required=True,
                      help=('The board to generate the sysroot for.'))
  parser.add_argument('--package', required=True,
                      help=('The package to generate the sysroot for.'))
  parser.add_argument('--out-dir', type=osutils.ExpandPath, required=True,
                      help='Directory to place the generated tarball.')
  parser.add_argument('--out-file',
                      help=('The name to give to the tarball.  Defaults to %r.'
                            % DEFAULT_NAME))
  options = parser.parse_args(argv)

  if not options.out_file:
    options.out_file = DEFAULT_NAME % {
        'package': options.package.replace(PACKAGE_SEPARATOR, '_')
    }

  return options


class GenerateSysroot(object):
  """Wrapper for generation functionality."""

  PARALLEL_EMERGE = os.path.join(constants.CHROMITE_BIN_DIR, 'parallel_emerge')

  def __init__(self, sysroot, options):
    """Initialize

    Arguments:
      sysroot: Path to sysroot.
      options: Parsed options.
    """
    self.sysroot = sysroot
    self.options = options

  def _InstallToolchain(self):
    cros_build_lib.RunCommand(
        [os.path.join(constants.CROSUTILS_DIR, 'install_toolchain'),
         '--noconfigure', '--board_root', self.sysroot, '--board',
         self.options.board])

  def _InstallKernelHeaders(self):
    cros_build_lib.SudoRunCommand(
        [self.PARALLEL_EMERGE, '--board=%s' % self.options.board,
         '--root-deps=rdeps', '--getbinpkg', '--usepkg',
         '--root=%s' % self.sysroot, 'sys-kernel/linux-headers'])

  def _InstallBuildDependencies(self):
    cros_build_lib.SudoRunCommand(
        [self.PARALLEL_EMERGE, '--board=%s' % self.options.board,
         '--root=%s' % self.sysroot, '--usepkg', '--onlydeps',
         '--usepkg-exclude=%s' % self.options.package, self.options.package])

  def _CreateTarball(self):
    target = os.path.join(self.options.out_dir, self.options.out_file)
    cros_build_lib.CreateTarball(target, self.sysroot, sudo=True)

  def Perform(self):
    """Generate the sysroot."""
    self._InstallToolchain()
    self._InstallKernelHeaders()
    self._InstallBuildDependencies()
    self._CreateTarball()


def FinishParsing(options):
  """Run environment dependent checks on parsed args."""
  target = os.path.join(options.out_dir, options.out_file)
  if os.path.exists(target):
    cros_build_lib.Die('Output file %r already exists.' % target)

  if not os.path.isdir(options.out_dir):
    cros_build_lib.Die(
        'Non-existent directory %r specified for --out-dir' % options.out_dir)


def main(argv):
  options = ParseCommandLine(argv)
  FinishParsing(options)

  cros_build_lib.AssertInsideChroot()

  with sudo.SudoKeepAlive(ttyless_sudo=False):
    with osutils.TempDir(set_global=True, sudo_rm=True) as tempdir:
      sysroot = os.path.join(tempdir, SYSROOT)
      os.mkdir(sysroot)
      GenerateSysroot(sysroot, options).Perform()
