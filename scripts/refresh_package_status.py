#!/usr/bin/python2.6
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Refresh online Portage package status spreadsheet."""

import optparse
import os
import shutil

from chromite.lib import cros_build_lib as cros_lib
from chromite.lib import operation

oper = operation.Operation('refresh_package_status')
oper.verbose = True # Without verbose Info messages don't show up.

TMP_ROOT = '/tmp'
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
# TODO(mtennant): Remove these two and replace with variables in gdata_lib.
GDATA_CRED_FILE = '.gdata_cred.txt'
GDATA_TOKEN_FILE = '.gdata_token'
GENTOO_DIR = 'gentoo-portage'
PRTG_GIT_URL = 'ssh://gerrit.chromium.org:29418/chromiumos/overlays/portage.git'
FUNTOO_GIT_URL = 'git://github.com/funtoo/portage.git'

def RunGit(cwd, cmd, args=[]):
  # pylint: disable=W0102
  """Run the git |cmd| with |args| in the |cwd| directory."""
  cmdline = ['git', cmd] + args
  cros_lib.RunCommand(cmdline, cwd=cwd)


def UpdateOriginGentoo():
  """Update origin/gentoo on server and return path to local copy."""
  gentoo_root = '%s/%s' % (TMP_ROOT, GENTOO_DIR)
  oper.Info('Performing update of origin/gentoo upstream mirror as requested.')

  if os.path.exists(gentoo_root):
    shutil.rmtree(gentoo_root)

  RunGit(cwd=TMP_ROOT, cmd='clone', args=[PRTG_GIT_URL, GENTOO_DIR])
  RunGit(cwd=gentoo_root, cmd='remote', args=['add', 'funtoo', FUNTOO_GIT_URL])
  RunGit(cwd=gentoo_root, cmd='checkout',
         args=['-b', 'gentoo', '--track', 'origin/gentoo'])
  RunGit(cwd=gentoo_root, cmd='fetch', args=['funtoo'])
  RunGit(cwd=gentoo_root, cmd='merge', args=['remotes/funtoo/gentoo.org'])
  RunGit(cwd=gentoo_root, cmd='push')

  return gentoo_root


def CleanupOriginGentoo(gentoo_root):
  """Remove the local copy of origin/gentoo source."""
  if os.path.exists(gentoo_root):
    oper.Info('Removing local copy of origin/gentoo upstream mirror now.')
    shutil.rmtree(gentoo_root)


def PrepareBoards(boards):
  """Run setup_board for any boards that need it."""
  scripts_dir = os.path.join(SCRIPT_DIR, '..', '..', 'src', 'scripts')
  for board in boards.split(':'):
    if not os.path.isdir('/build/%s' % board):
      oper.Info('Running setup_board for board=%s' % board)
      cros_lib.RunCommand(['./setup_board', '--board=%s' % board],
                          cwd=scripts_dir)


def PrepareCSVRoot():
  """Prepare CSV output directory for use."""
  csv_root = '%s/%s' % (TMP_ROOT, 'csv')
  if os.path.exists(csv_root):
    shutil.rmtree(csv_root)

  os.mkdir(csv_root)

  return csv_root


def RefreshPackageStatus(board, upstream, csv_root, test,
                         token_file, cred_file):
  """Run through steps to refresh package status spreadsheet.

  |board| is a colon-separated list of chromeos boards to use.
  |upstream| is a path to origin/gentoo upstream.  Can be None.
  Put all csv files under the |csv_root| directory.
  If |test| is True, upload to test spreadsheet instead of real one.
  Use |token_file| as auth token file for spreadsheet upload if it exists.
  Otherwise, use |cred_file| as credentials file for spreadsheet upload.
  """

  # Run all chromeos targets for all boards.
  oper.Info('Getting package status for all chromeos targets on all boards.')
  cpu_cmd_baseline = ['cros_portage_upgrade', '--board=%s' % board]
  if upstream:
    cpu_cmd_baseline.append('--upstream=%s' % upstream)

  cros_csv = '%s/chromeos-boards.csv' % csv_root
  cros_lib.RunCommand(cpu_cmd_baseline +
                      ['--to-csv=%s' % cros_csv,
                       'chromeos'])

  crosdev_csv = '%s/chromeos-dev-boards.csv' % csv_root
  cros_lib.RunCommand(cpu_cmd_baseline +
                      ['--to-csv=%s' % crosdev_csv,
                       'chromeos', 'chromeos-dev'])

  crostest_csv = '%s/chromeos-test-boards.csv' % csv_root
  cros_lib.RunCommand(cpu_cmd_baseline +
                      ['--to-csv=%s' % crostest_csv,
                       'chromeos', 'chromeos-dev', 'chromeos-test'])

  # Run all host targets (world, hard-host-depends) for the host.
  oper.Info('Getting package status for all host targets.')
  cpu_host_baseline = ['cros_portage_upgrade', '--host']
  if upstream:
    cpu_host_baseline.append('--upstream=%s' % upstream)

  hostworld_csv = '%s/world-host.csv' % csv_root
  cros_lib.RunCommand(cpu_host_baseline +
                      ['--to-csv=%s' % hostworld_csv,
                       'world'])

  hosthhd_csv = '%s/hhd-host.csv' % csv_root
  cros_lib.RunCommand(cpu_host_baseline +
                      ['--to-csv=%s' % hosthhd_csv,
                       'hard-host-depends'])

  # Merge all csv tables into one.
  oper.Info('Merging all package status files into one.')
  allboards_csv = '%s/all-boards.csv' % csv_root
  cros_lib.RunCommand(['merge_package_status',
                       '--out=%s' % allboards_csv,
                       cros_csv, crosdev_csv, crostest_csv])

  allhost_csv = '%s/all-host.csv' % csv_root
  cros_lib.RunCommand(['merge_package_status',
                       '--out=%s' % allhost_csv,
                       hostworld_csv, hosthhd_csv])

  allfinal_csv = '%s/all-final.csv' % csv_root
  cros_lib.RunCommand(['merge_package_status',
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
  cros_lib.RunCommand(upload_cmdline)


def main(argv):
  """Main function."""
  usage = 'Usage: %prog --board=<board>:... [options]'
  epilog = ('\n'
            'This must be run inside the chroot.\n'
            'This script encapsulates the steps involved in updating the '
            'online Portage package status spreadsheet.\n'
            'It was created for use by a buildbot, but can be run manually.\n'
            '\n'
            )

  # pylint: disable=R0904
  class MyOptParser(optparse.OptionParser):
    """Override default epilog formatter, which strips newlines."""
    def format_epilog(self, formatter):
      return self.epilog

  parser = MyOptParser(usage=usage, epilog=epilog)
  parser.add_option('--token-file', dest='token_file', type='string',
                    action='store',
                    default='%s/%s' % (os.environ['HOME'], GDATA_TOKEN_FILE),
                    help='Path to gdata auth token file [default: "%default"]')
  parser.add_option('--board', dest='board', type='string', action='store',
                    default=None, help='Target board(s), colon-separated')
  parser.add_option('--cred-file', dest='cred_file', type='string',
                    action='store',
                    default='%s/%s' % (os.environ['HOME'], GDATA_CRED_FILE),
                    help='Path to gdata credentials file [default: "%default"]')
  parser.add_option('--test-spreadsheet', dest='test',
                    action='store_true', default=False,
                    help='Upload changes to test spreadsheet instead')
  parser.add_option('--update-gentoo-first', dest='update_gentoo',
                    action='store_true', default=False,
                    help='Perform update to origin/gentoo before refresh')

  (options, args) = parser.parse_args(argv)

  if args:
    parser.print_help()
    oper.Die('No extra arguments allowed.')

  if not options.board:
    parser.print_help()
    oper.Die('Board is required.')

  upstream = None
  if options.update_gentoo:
    upstream = UpdateOriginGentoo()

  csv_root = PrepareCSVRoot()
  PrepareBoards(options.board)

  RefreshPackageStatus(board=options.board, upstream=upstream,
                       csv_root=csv_root, test=options.test,
                       token_file=options.token_file,
                       cred_file=options.cred_file)

  if options.update_gentoo:
    CleanupOriginGentoo(upstream)
