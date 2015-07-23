# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Refresh online Portage package status spreadsheet.

This must be run inside the chroot.

This script encapsulates the steps involved in updating the online Portage
package status spreadsheet.

It was created for use by a buildbot, but can be run manually.
"""

from __future__ import print_function

import os

from chromite.cbuildbot import config_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import operation
from chromite.lib import osutils


site_config = config_lib.GetConfig()


oper = operation.Operation('refresh_package_status')
oper.verbose = True  # Without verbose Info messages don't show up.

TMP_ROOT = '/tmp'
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
# TODO(mtennant): Remove these two and replace with variables in gdata_lib.
GDATA_CRED_FILE = os.path.expanduser('~/.gdata_cred.txt')
GDATA_TOKEN_FILE = os.path.expanduser('~/.gdata_token')
GENTOO_DIR = 'gentoo-portage'
PRTG_GIT_URL = (
    '%s/chromiumos/overlays/portage.git' % site_config.params.EXTERNAL_GOB_URL)
FUNTOO_GIT_URL = 'git://github.com/funtoo/portage.git'


def RunGit(cwd, cmd, args=()):
  """Run the git |cmd| with |args| in the |cwd| directory."""
  cmdline = ['git', cmd] + list(args)
  cros_build_lib.RunCommand(cmdline, cwd=cwd)


def PrepareBoards(boards):
  """Run setup_board for any boards that need it."""
  scripts_dir = os.path.join(SCRIPT_DIR, '..', '..', 'src', 'scripts')
  for board in boards.split(':'):
    if not os.path.isdir(cros_build_lib.GetSysroot(board=board)):
      oper.Info('Running setup_board for board=%s' % board)
      cros_build_lib.RunCommand(['./setup_board', '--board=%s' % board],
                                cwd=scripts_dir)


def PrepareCSVRoot():
  """Prepare CSV output directory for use."""
  csv_root = '%s/%s' % (TMP_ROOT, 'csv')
  osutils.RmDir(csv_root, ignore_missing=True)

  os.mkdir(csv_root)

  return csv_root


def RefreshPackageStatus(board, csv_root, test,
                         token_file, cred_file):
  """Run through steps to refresh package status spreadsheet.

  Args:
    board: A colon-separated list of chromeos boards to use.
    csv_root: Put all csv files under this directory.
    test: If True, upload to test spreadsheet instead of real one.
    token_file: Use as auth token file for spreadsheet upload if it exists.
    cred_file: If |token_file| is missing, use this as credentials file for
      spreadsheet upload.
  """

  # Run all chromeos targets for all boards.
  oper.Info('Getting package status for all chromeos targets on all boards.')
  cpu_cmd_baseline = ['cros_portage_upgrade', '--board=%s' % board]

  cros_csv = '%s/chromeos-boards.csv' % csv_root
  cros_build_lib.RunCommand(cpu_cmd_baseline +
                            ['--to-csv=%s' % cros_csv,
                             'virtual/target-os'])

  crosdev_csv = '%s/chromeos-dev-boards.csv' % csv_root
  cros_build_lib.RunCommand(cpu_cmd_baseline +
                            ['--to-csv=%s' % crosdev_csv,
                             'virtual/target-os', 'virtual/target-os-dev'])

  crostest_csv = '%s/chromeos-test-boards.csv' % csv_root
  cros_build_lib.RunCommand(cpu_cmd_baseline +
                            ['--to-csv=%s' % crostest_csv,
                             'virtual/target-os', 'virtual/target-os-dev',
                             'virtual/target-os-test'])

  # Run all host targets for the sdk.
  oper.Info('Getting package status for all host (sdk) targets.')
  cpu_host_baseline = ['cros_portage_upgrade', '--host']

  hostworld_csv = '%s/world-host.csv' % csv_root
  cros_build_lib.RunCommand(cpu_host_baseline +
                            ['--to-csv=%s' % hostworld_csv,
                             'world'])

  hosthhd_csv = '%s/hhd-host.csv' % csv_root
  cros_build_lib.RunCommand(cpu_host_baseline +
                            ['--to-csv=%s' % hosthhd_csv,
                             'virtual/target-sdk'])

  # Merge all csv tables into one.
  oper.Info('Merging all package status files into one.')
  allboards_csv = '%s/all-boards.csv' % csv_root
  cros_build_lib.RunCommand(['merge_package_status',
                             '--out=%s' % allboards_csv,
                             cros_csv, crosdev_csv, crostest_csv])

  allhost_csv = '%s/all-host.csv' % csv_root
  cros_build_lib.RunCommand(['merge_package_status',
                             '--out=%s' % allhost_csv,
                             hostworld_csv, hosthhd_csv])

  allfinal_csv = '%s/all-final.csv' % csv_root
  cros_build_lib.RunCommand(['merge_package_status',
                             '--out=%s' % allfinal_csv,
                             allboards_csv, allhost_csv])

  # Upload the final csv file to the online spreadsheet.
  oper.Info('Uploading package status to online spreadsheet.')
  upload_cmdline = ['upload_package_status', '--verbose',
                    '--cred-file=%s' % cred_file,
                    '--auth-token-file=%s' % token_file]
  if test:
    upload_cmdline.append('--test-spreadsheet')

  upload_cmdline.append(allfinal_csv)
  cros_build_lib.RunCommand(upload_cmdline)


def main(argv):
  """Main function."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--token-file', type='path', default=GDATA_TOKEN_FILE,
                      help='Path to gdata auth token file '
                           '[default: %(default)s]')
  parser.add_argument('--board', type=str, required=True,
                      help='Target board(s), colon-separated')
  parser.add_argument('--cred-file', type='path', default=GDATA_CRED_FILE,
                      help='Path to gdata credentials file '
                           '[default: %(default)s]')
  parser.add_argument('--test-spreadsheet', dest='test',
                      action='store_true', default=False,
                      help='Upload changes to test spreadsheet instead')
  options = parser.parse_args(argv)

  csv_root = PrepareCSVRoot()
  PrepareBoards(options.board)

  RefreshPackageStatus(board=options.board, csv_root=csv_root,
                       test=options.test, token_file=options.token_file,
                       cred_file=options.cred_file)
