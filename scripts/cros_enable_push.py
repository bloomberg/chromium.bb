#!/usr/bin/python
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Script to setup push configuration in a git-repo checkout."""

import os
import sys
from chromite.buildbot import configure_repo
from chromite.buildbot import repository
from chromite.lib import git


def _Usage(handle):
  print >> handle, (
     "Tool to fix git configuration and add push configuration to a repo.")
  print >> handle
  print >> handle, (
"""Fix git configuration, and enable git pushing to the remote.

To run this script, either invoke it from within a repo checkout, or pass it
the path of a repo checkout you'd like to configure.

Once that has been done, what would've been (for example):

git push ssh://gerrit.chromium.org:29418/chromiumos/chromite\
 HEAD:refs/for/master

Can now be done as:

git push cros HEAD:refs/for/master
""")

def main(argv):
  if len(argv) > 1:
    _Usage(sys.stderr)
    return 1
  if not argv:
    path = git.FindRepoCheckoutRoot(os.getcwd())
    if path is None:
      _Usage(sys.stderr)
      print >> sys.stderr, "We're not in a repo checkout."
      return 1
  else:
    if argv[0] in ('-h', '--help', '--usage'):
      _Usage(sys.stderr)
      return 0
    path = argv[0]
    if not repository.IsARepoRoot(path):
      _Usage(sys.stderr)
      print >> sys.stderr, ("Path %s doesn't point to the root of a repository."
                           % (argv[0],))
      return 1

  print "Cleaning stale git configuration from %s" % (path,)
  configure_repo.FixBrokenExistingRepos(path)
  print "\nAdding git push configuration to %s" % (path,)
  configure_repo.FixExternalRepoPushUrls(path)
