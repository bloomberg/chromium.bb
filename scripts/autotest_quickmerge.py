#!/usr/bin/python

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""
Simple script to be run inside the chroot. Used as a fast approximation of
emerge-$board autotest-all, by simply rsync'ing changes from trunk to sysroot.
"""

import os
import sys

from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import git

import argparse

INCLUDE_PATTERNS_FILENAME = 'autotest-quickmerge-includepatterns'
AUTOTEST_PROJECT_NAME = 'chromiumos/third_party/autotest'

def RsyncQuickmerge(source_path, sysroot_autotest_path,
                    include_pattern_file=None, pretend=False,
                    overwrite=False, quiet=False):
  """Run rsync quickmerge command, with specified arguments.
  Command will take form `rsync -a [options] --exclude=**.pyc
                         --exclude=**.pyo
                         [optional --include-from argument]
                         --exclude=* [source_path] [sysroot_autotest_path]`

  Arguments:
    pretend:   True to use the '-n' option to rsync, to perform dry run.
    overwrite: True to omit '-u' option, overwrite all files in sysroot,
               not just older files.
    quiet:     True to omit the '-i' option, silence rsync change log.
  """
  command = ['rsync', '-a']

  if pretend:
    command += ['-n']

  if not overwrite:
    command += ['-u']

  if not quiet:
    command += ['-i']

  command += ['--exclude=**.pyc']
  command += ['--exclude=**.pyo']

  if include_pattern_file:
    command += ['--include-from=%s' % include_pattern_file]

  command += ['--exclude=*']

  command += [source_path, sysroot_autotest_path]

  cros_build_lib.SudoRunCommand(command)


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

  if not args.board:
    print "No board specified, and no default board. Aborting."
    return 1

  manifest = git.ManifestCheckout.Cached(constants.SOURCE_ROOT)
  source_path = manifest.GetProjectPath(AUTOTEST_PROJECT_NAME, absolute=True)
  source_path = os.path.join(source_path, '')

  script_path = os.path.dirname(__file__)
  include_pattern_file = os.path.join(script_path, INCLUDE_PATTERNS_FILENAME)

  # TODO: Determine the following string programatically.
  sysroot_autotest_path = os.path.join('/build', args.board, 'usr', 'local',
                                       'autotest', '')

  RsyncQuickmerge(source_path, sysroot_autotest_path, include_pattern_file,
                  args.pretend, args.overwrite, args.quiet)

  print 'Done'


if __name__ == '__main__':
  sys.exit(main(sys.argv))
