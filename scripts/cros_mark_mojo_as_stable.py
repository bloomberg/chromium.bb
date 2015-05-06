# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module uprevs Mojo for cbuildbot.

If a new version of Mojo is found, a stable ebuild for the new version
is created and older stable ebuilds will be deleted. The changes will be
contained in a single commit on the "stabilizing_branch" branch which
will be created if it does not already exist. Additionally, the program
will print out

 MOJO_VERSION_ATOM="version atom string"

This can be used with emerge to build the newly uprevved version:

 $ cros_mark_mojo_as_stable
 MOJO_VERSION_ATOM=dev-libs/mojo-0.20141202.181307-r1
 $ emerge-lumpy =dev-libs/mojo-0.20141202.181307-r1

If no new version of Mojo is found, the program will not print anything.
"""

from __future__ import print_function

import os

from chromite.cbuildbot import constants
from chromite.lib import commandline
from chromite.lib import git
from chromite.lib import gob_util
from chromite.lib import portage_util
from chromite.scripts import cros_mark_as_stable


MOJO_CATEGORY = 'dev-libs'
MOJO_PN = 'mojo'
MOJO_CP = MOJO_CATEGORY + '/' + MOJO_PN
MOJO_EBUILD_PATH = 'third_party/chromiumos-overlay/' + MOJO_CP
MOJO_REPO_URL = 'https://chromium.googlesource.com/external/mojo'


def GetStableEBuilds(ebuild_dir):
  """Gets all stable ebuilds from the given directory.

  Args:
    ebuild_dir: Path to the directory to look in.

  Returns:
    An array of ebuilds in the given directory.
  """
  return [x for x in os.listdir(ebuild_dir)
          if x.endswith('.ebuild') and not
          x.endswith(portage_util.WORKON_EBUILD_SUFFIX)]


def UprevStableEBuild(ebuild_dir, commit_to_use, date_of_commit,
                      tracking_branch='cros/master'):
  """Checks if there already if a stable Mojo ebuild for the given commit.

  If there already is a stable Mojo ebuild for the given commit, this
  function does nothing and returns None. Otherwise creates a stabilization
  branch with a single commit that creates a new stable ebuild and deletes
  all other stable ebuilds.

  Args:
    ebuild_dir: Path to the directory holding Mojo ebuilds.
    commit_to_use: The upstream Mojo commit id.
    date_of_commit: The date of the commit.
    tracking_branch: The branch that the stabilization branch should track.

  Returns:
    None or a version atom describing the newly created ebuild.
  """
  # There is no version number or other monotonically increasing value
  # that is suitable to use as a version number except for the point
  # in time that a commit was added to the repository. So we use that
  # for now.
  pvr = date_of_commit.strftime('0.%Y%m%d.%H%M%S-r1')
  mojo_stable_pn = '%s-%s.ebuild' % (MOJO_PN, pvr)

  # Find existing stable ebuilds and only add a new one if there's a
  # newer commit.
  existing_ebuilds = GetStableEBuilds(ebuild_dir)
  if mojo_stable_pn in existing_ebuilds:
    return None

  # OK. First create a stablizing branch.
  tracking_branch_full = 'remotes/m/%s' % os.path.basename(tracking_branch)
  existing_branch = git.GetCurrentBranch(ebuild_dir)
  work_branch = cros_mark_as_stable.GitBranch(constants.STABLE_EBUILD_BRANCH,
                                              tracking_branch_full,
                                              ebuild_dir)
  work_branch.CreateBranch()

  # In the case of uprevving overlays that have patches applied to them,
  # include the patched changes in the stabilizing branch.
  if existing_branch:
    git.RunGit(ebuild_dir, ['rebase', existing_branch])

  # Create a new ebuild.
  unstable_ebuild_path = os.path.join(ebuild_dir, MOJO_PN +
                                      portage_util.WORKON_EBUILD_SUFFIX)
  new_stable_ebuild_path = os.path.join(ebuild_dir, mojo_stable_pn)
  variables = {'MOJO_REVISION': commit_to_use}
  portage_util.EBuild.MarkAsStable(unstable_ebuild_path,
                                   new_stable_ebuild_path,
                                   variables, make_stable=True)

  # Add it to the repo.
  git.RunGit(ebuild_dir, ['add', new_stable_ebuild_path])

  # Nuke the now stale older ebuilds.
  for f in existing_ebuilds:
    git.RunGit(ebuild_dir, ['rm', '-f', f])

  # ... and finally commit the change.
  portage_util.EBuild.CommitChange('Updated %s to upstream commit %s.' %
                                   (MOJO_CP, commit_to_use),
                                   ebuild_dir)
  # Return version atom for newly created ebuild.
  return MOJO_CP + '-' + pvr


def main(_argv):
  parser = commandline.ArgumentParser(usage=__doc__)
  parser.add_argument('--force_version', default=None,
                      help='git revision hash to use')
  parser.add_argument('--repo_url', default=MOJO_REPO_URL)
  parser.add_argument('--srcroot', type='path',
                      default=os.path.join(os.environ['HOME'], 'trunk', 'src'),
                      help='Path to the src directory')
  parser.add_argument('--tracking_branch', default='cros/master',
                      help='Branch we are tracking changes against')
  options = parser.parse_args()
  options.Freeze()

  mojo_version_atom = None
  ebuild_dir = os.path.join(options.srcroot, MOJO_EBUILD_PATH)

  # Figure out commit to use and its date.
  if options.force_version:
    commit_to_use = options.force_version
  else:
    commit_to_use = gob_util.GetTipOfTrunkRevision(options.repo_url)
  date_of_commit = gob_util.GetCommitDate(options.repo_url, commit_to_use)

  # Do the uprev and explicit print version to inform caller if we
  # made a change.
  mojo_version_atom = UprevStableEBuild(ebuild_dir, commit_to_use,
                                        date_of_commit, options.tracking_branch)
  if mojo_version_atom:
    print('MOJO_VERSION_ATOM=%s' % mojo_version_atom)
