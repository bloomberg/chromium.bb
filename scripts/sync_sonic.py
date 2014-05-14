# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sync the Chrome source code used by Sonic to the specified directory."""

from chromite.lib import commandline
from chromite.lib import cros_build_lib, git
from chromite.cbuildbot import repository
from chromite.lib import osutils


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('--reset', help='Revert local changes',
                      action='store_true', default=False)
  parser.add_argument('chrome_root', help='Directory to sync chrome in')

  return parser


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)

  mc = git.ManifestCheckout.Cached(__file__)

  # TODO(cmasone): http://crbug.com/241805 support "internal"
  # checkouts of sonic chrome, once source access and codename issues
  # are sorted out.
  repo_url = (mc.remotes['sonic-partner']['fetch'] +
              mc.FindProjectCheckout('partner/manifest')['name'])

  repo = repository.RepoRepository(repo_url=repo_url,
                                   directory=options.chrome_root)

  # Revert any lingering local changes.
  if not osutils.SafeMakedirs(options.chrome_root) and options.reset:
    try:
      repo.Detach()
    except cros_build_lib.RunCommandError:
      osutils.RmDir(options.chrome_root)
      osutils.SafeMakedirs(options.chrome_root)

  repo.Sync()

  return 0
