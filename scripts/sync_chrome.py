# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sync the Chrome source code used by Chrome OS to the specified directory."""

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import gclient
from chromite.lib import osutils


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  version = parser.add_mutually_exclusive_group()
  version.add_argument('--tag', help='Sync to specified Chrome release',
                       dest='version')
  version.add_argument('--revision', help='Sync to specified SVN revision',
                       type=int, dest='version')

  parser.add_argument('--internal', help='Sync internal version of Chrome',
                      action='store_true', default=False)
  parser.add_argument('--pdf', help='Sync PDF source code',
                      action='store_true', default=False)
  parser.add_argument('--reset', help='Revert local changes',
                      action='store_true', default=False)
  parser.add_argument('--gclient', help=commandline.argparse.SUPPRESS,
                      default='gclient')
  parser.add_argument('chrome_root', help='Directory to sync chrome in')

  return parser


def main(args):
  parser = GetParser()
  options = parser.parse_args(args)

  # Revert any lingering local changes.
  if not osutils.SafeMakedirs(options.chrome_root) and options.reset:
    try:
      gclient.Revert(options.gclient, options.chrome_root)
    except cros_build_lib.RunCommandError:
      osutils.RmDir(options.chrome_root)
      osutils.SafeMakedirs(options.chrome_root)

  # Sync new Chrome.
  gclient.WriteConfigFile(options.gclient, options.chrome_root,
                          options.internal, options.pdf, options.version)
  gclient.Sync(options.gclient, options.chrome_root, options.reset)
  return 0
