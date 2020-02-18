# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tool to run ebuild unittests."""

from __future__ import print_function

import multiprocessing
import os

from chromite.lib import commandline
from chromite.lib import constants
from chromite.lib import chroot_util
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import workon_helper
from chromite.lib import portage_util
from chromite.scripts import cros_extract_deps

BOARD_VIRTUAL_PACKAGES = (constants.TARGET_OS_PKG,
                          constants.TARGET_OS_DEV_PKG,
                          constants.TARGET_OS_TEST_PKG,
                          constants.TARGET_OS_FACTORY_PKG)
IMPLICIT_TEST_DEPS = ('virtual/implicit-system',)


def ParseArgs(argv):
  """Parse arguments.

  Args:
    argv: array of arguments passed to the script.
  """
  parser = commandline.ArgumentParser(description=__doc__)

  target = parser.add_mutually_exclusive_group(required=True)
  target.add_argument('--sysroot', type='path', help='Path to the sysroot.')
  target.add_argument('--board', help='Board name.')

  parser.add_argument('--pretend', default=False, action='store_true',
                      help='Show the list of packages to be tested and return.')
  parser.add_argument('--noinstalled_only', dest='installed', default=True,
                      action='store_false',
                      help='Test all testable packages, even if they are not '
                      'currently installed.')
  parser.add_argument('--package_file', type='path',
                      help='Path to a file containing the list of packages '
                      'that should be tested.')
  parser.add_argument('--blacklist_packages', dest='package_blacklist',
                      help='Space-separated list of blacklisted packages.')
  parser.add_argument('--packages',
                      help='Space-separated list of packages to test.')
  parser.add_argument('--nowithdebug', action='store_true',
                      help="Don't build the tests with USE=cros-debug")
  parser.add_argument('--assume-empty-sysroot', default=False,
                      action='store_true', dest='empty_sysroot',
                      help='Set up dependencies and run unit tests for all '
                           'packages that could be installed on target board '
                           'without assuming that any packages have actually '
                           'been merged yet.')

  options = parser.parse_args(argv)
  options.Freeze()
  return options


def determine_board_packages(sysroot, virtual_packages):
  """Returns a set of the dependencies for the given packages"""
  deps = cros_extract_deps.ExtractDeps(sysroot, virtual_packages)
  return set('%s/%s' % (atom['category'], atom['name'])
             for atom in deps.values())


def main(argv):
  opts = ParseArgs(argv)

  cros_build_lib.AssertInsideChroot()

  sysroot = opts.sysroot or cros_build_lib.GetSysroot(opts.board)
  package_blacklist = portage_util.UNITTEST_PACKAGE_BLACKLIST
  if opts.package_blacklist:
    package_blacklist |= set(opts.package_blacklist.split())

  packages = set()
  # The list of packages to test can be passed as a file containing a
  # space-separated list of package names.
  # This is used by the builder to test only the packages that were upreved.
  if opts.package_file and os.path.exists(opts.package_file):
    packages = set(osutils.ReadFile(opts.package_file).split())

  if opts.packages:
    packages |= set(opts.packages.split())

  # If no packages were specified, use all testable packages.
  if not (opts.packages or opts.package_file) and not opts.empty_sysroot:
    workon = workon_helper.WorkonHelper(sysroot)
    packages = (workon.InstalledWorkonAtoms() if opts.installed
                else set(workon.ListAtoms(use_all=True)))

  if opts.empty_sysroot:
    packages |= determine_board_packages(sysroot, BOARD_VIRTUAL_PACKAGES)
    workon = workon_helper.WorkonHelper(sysroot)
    workon_packages = set(workon.ListAtoms(use_all=True))
    packages &= workon_packages

  for cp in packages & package_blacklist:
    logging.info('Skipping blacklisted package %s.', cp)

  packages = packages - package_blacklist
  pkg_with_test = portage_util.PackagesWithTest(sysroot, packages)

  if packages - pkg_with_test:
    logging.warning('The following packages do not have tests:\n  %s',
                    '\n  '.join(sorted(packages - pkg_with_test)))

  if not pkg_with_test:
    logging.error('No testable packages found!')
    return 1

  if opts.pretend:
    print('\n'.join(sorted(pkg_with_test)))
    return 0

  env = None
  if opts.nowithdebug:
    use_flags = os.environ.get('USE', '')
    use_flags += ' -cros-debug'
    env = {'USE': use_flags}

  if opts.empty_sysroot:
    try:
      chroot_util.Emerge(list(IMPLICIT_TEST_DEPS), sysroot, rebuild_deps=False,
                         use_binary=False)
      chroot_util.Emerge(list(pkg_with_test), sysroot, rebuild_deps=False,
                         use_binary=False)
    except cros_build_lib.RunCommandError:
      logging.error('Failed building dependencies for unittests.')
      return 1

  try:
    chroot_util.RunUnittests(sysroot, pkg_with_test, extra_env=env,
                             jobs=min(10, multiprocessing.cpu_count()))
  except cros_build_lib.RunCommandError:
    logging.error('Unittests failed.')
    return 1
