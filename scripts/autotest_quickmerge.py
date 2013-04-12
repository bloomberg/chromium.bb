#!/usr/bin/python

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""
Simple script to be run inside the chroot. Used as a fast approximation of
emerge-$board autotest-all, by simply rsync'ing changes from trunk to sysroot.
"""

import os
import re
import sys
from collections import namedtuple

from chromite.buildbot import constants
from chromite.buildbot import portage_utilities
from chromite.lib import cros_build_lib
from chromite.lib import git

import argparse

if cros_build_lib.IsInsideChroot():
  # Only import portage after we've checked that we're inside the chroot.
  import portage

INCLUDE_PATTERNS_FILENAME = 'autotest-quickmerge-includepatterns'
AUTOTEST_PROJECT_NAME = 'chromiumos/third_party/autotest'
AUTOTEST_TESTS_EBUILD = 'chromeos-base/autotest-tests'


# Data structure describing a single rsync filesystem change.
#
# change_description: An 11 character string, the rsync change description
#                     for the particular file.
# absolute_path: The absolute path of the created or modified file.
ItemizedChange = namedtuple('ItemizedChange', ['change_description',
                                               'absolute_path'])


# Data structure describing the rsync new/modified files or directories.
#
# new_files: A list of ItemizedChange objects for new files.
# modified_files: A list of ItemizedChange objects for modified files.
# new_directories: A list of ItemizedChange objects for new directories.
ItemizedChangeReport = namedtuple('ItemizedChangeReport',
                                  ['new_files', 'modified_files',
                                   'new_directories'])


def ItemizeChangesFromRsyncOutput(rsync_output, destination_path):
  """Convert the output of an rsync with `-i` to a ItemizedChangeReport object.

  Arguments:
    rsync_output: String stdout of rsync command that was run with `-i` option.
    destination_path: String absolute path of the destination directory for the
                      rsync operations. This argument is necessary because
                      rsync's output only gives the relative path of
                      touched/added files.

  Returns:
    ItemizedChangeReport object giving the absolute paths of files that were
    created or modified by rsync.
  """
  modified_matches = re.findall(r'([.>]f[^+]{9}) (.*)', rsync_output)
  new_matches = re.findall(r'(>f\+{9}) (.*)', rsync_output)
  new_symlink_matches = re.findall(r'(cL\+{9}) (.*) -> .*', rsync_output)
  new_dir_matches = re.findall(r'(cd\+{9}) (.*)', rsync_output)

  absolute_modified = [ItemizedChange(c, os.path.join(destination_path, f))
                       for (c, f) in modified_matches]

  # Note: new symlinks are treated as new files.
  absolute_new = [ItemizedChange(c, os.path.join(destination_path, f))
                  for (c, f) in new_matches + new_symlink_matches]

  absolute_new_dir = [ItemizedChange(c, os.path.join(destination_path, f))
                      for (c, f) in new_dir_matches]

  return ItemizedChangeReport(new_files=absolute_new,
                              modified_files=absolute_modified,
                              new_directories=absolute_new_dir)


def UpdatePackageContents(change_report, package_cp,
                          portage_root=None):
  """
  Add newly created files/directors to package contents.

  Given an ItemizedChangeReport, add the newly created files and directories
  to the CONTENTS of an installed portage package, such that these files are
  considered owned by that package.

  Arguments:
    changereport: ItemizedChangeReport object for the changes to be
                  made to the package.
    package_cp: A string similar to 'chromeos-base/autotest-tests' giving
                the package category and name of the package to be altered.
    portage_root: Portage root path, corresponding to the board that
                  we are working on. Defaults to '/'
  """
  if portage_root is None:
    portage_root = portage.root # pylint: disable-msg=E1101
  # Ensure that portage_root ends with trailing slash.
  portage_root = os.path.join(portage_root, '')

  # Create vartree object corresponding to portage_root
  trees = portage.create_trees(portage_root, portage_root)
  vartree = trees[portage_root]['vartree']

  # List matching installed packages in cpv format
  matching_packages = vartree.dbapi.cp_list(package_cp)

  if not matching_packages:
    raise ValueError('No matching package for %s in portage_root %s' % (
        package_cp, portage_root))

  if len(matching_packages) > 1:
    raise ValueError('Too many matching packages for %s in portage_root '
        '%s' % (package_cp, portage_root))

  # Convert string match to package dblink
  package_cpv = matching_packages[0]
  package_split = portage_utilities.SplitCPV(package_cpv)
  package = portage.dblink(package_split.category, # pylint: disable-msg=E1101
                           package_split.pv, settings=vartree.settings,
                           vartree=vartree)

  # Append new contents to package contents dictionary
  contents = package.getcontents().copy()
  for _, filename in change_report.new_files:
    contents.setdefault(filename, (u'obj', '0', '0'))
  for _, dirname in change_report.new_directories:
    # String trailing slashes if present.
    dirname = dirname.rstrip('/')
    contents.setdefault(dirname, (u'dir',))

  # Write new contents dictionary to file
  vartree.dbapi.writeContentsToContentsFile(package, contents)


def RsyncQuickmerge(source_path, sysroot_autotest_path,
                    include_pattern_file=None, pretend=False,
                    overwrite=False):
  """Run rsync quickmerge command, with specified arguments.
  Command will take form `rsync -a [options] --exclude=**.pyc
                         --exclude=**.pyo
                         [optional --include-from argument]
                         --exclude=* [source_path] [sysroot_autotest_path]`

  Arguments:
    pretend:   True to use the '-n' option to rsync, to perform dry run.
    overwrite: True to omit '-u' option, overwrite all files in sysroot,
               not just older files.
  """
  command = ['rsync', '-a']

  if pretend:
    command += ['-n']

  if not overwrite:
    command += ['-u']

  command += ['-i']

  command += ['--exclude=**.pyc']
  command += ['--exclude=**.pyo']

  # Exclude files with a specific substring in their name, because
  # they create an ambiguous itemized report. (see unit test file for details)
  command += ['--exclude=** -> *']

  if include_pattern_file:
    command += ['--include-from=%s' % include_pattern_file]

  command += ['--exclude=*']

  command += [source_path, sysroot_autotest_path]

  return cros_build_lib.SudoRunCommand(command, redirect_stdout=True)


def ParseArguments(argv):
  """Parse command line arguments

  Returns: parsed arguments.
  """
  parser = argparse.ArgumentParser(description='Perform a fast approximation '
                                   'to emerge-$board autotest-all, by '
                                   'rsyncing source tree to sysroot.')

  parser.add_argument('--board', metavar='BOARD', default=None, required=True)
  parser.add_argument('--pretend', action='store_true',
                      help='Dry run only, do not modify sysroot autotest.')
  parser.add_argument('--overwrite', action='store_true',
                      help='Overwrite existing files even if newer.')
  parser.add_argument('--quiet', action='store_true',
                      help='Suppress output of list of modified files.')

  return parser.parse_args(argv)


def main(argv):
  cros_build_lib.AssertInsideChroot()

  args = ParseArguments(argv)

  if not os.geteuid()==0:
    try:
      cros_build_lib.SudoRunCommand([sys.executable] + sys.argv)
    except cros_build_lib.RunCommandError:
      return 1
    return 0

  if not args.board:
    print 'No board specified, and no default board. Aborting.'
    return 1

  manifest = git.ManifestCheckout.Cached(constants.SOURCE_ROOT)
  source_path = manifest.GetProjectPath(AUTOTEST_PROJECT_NAME, absolute=True)
  source_path = os.path.join(source_path, '')

  script_path = os.path.dirname(__file__)
  include_pattern_file = os.path.join(script_path, INCLUDE_PATTERNS_FILENAME)

  # TODO: Determine the following string programatically.
  sysroot_path = os.path.join('/build', args.board, '')
  sysroot_autotest_path = os.path.join(sysroot_path, 'usr', 'local',
                                       'autotest', '')

  rsync_output = RsyncQuickmerge(source_path, sysroot_autotest_path,
      include_pattern_file, args.pretend, args.overwrite)

  change_report = ItemizeChangesFromRsyncOutput(rsync_output.output,
                                                sysroot_autotest_path)

  if not args.pretend:
    UpdatePackageContents(change_report, AUTOTEST_TESTS_EBUILD,
                          sysroot_path)

  if args.pretend:
    print 'The following message is pretend only. No filesystem changes made.'
  print 'Quickmerge complete. Created or modified %s files.' % (
        len(change_report.new_files) + len(change_report.modified_files))

if __name__ == '__main__':
  sys.exit(main(sys.argv))
