# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Create a manifest snapshot of a repo checkout.

This starts with the output of `repo manifest -r` and updates the manifest to
account for local changes that may not already be available remotely; for any
commits that aren't already reachable from the upstream tracking branch, push
refs to the remotes so that this snapshot can be reproduced remotely.
"""

from __future__ import print_function

import os
import sys

from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import git
from chromite.lib import parallel
from chromite.lib import repo_util


BRANCH_REF_PREFIX = 'refs/heads/'


def GetParser():
  """Creates the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)
  parser.add_argument('--repo-path', type='path', default='.',
                      help='Path to the repo to snapshot.')
  parser.add_argument('--snapshot-ref',
                      help='Remote ref to create for projects whose HEAD is '
                           'not reachable from its current upstream branch. '
                           'Projects with multiple checkouts may have a '
                           'unique suffix appended to this ref.')
  parser.add_argument('--output-file', type='path',
                      help='Path to write the manifest snapshot XML to.')
  parser.add_argument('--dry-run', action='store_true',
                      help='Do not actually push to remotes.')
  # This is for limiting network traffic to the git remote(s).
  parser.add_argument('--jobs', type=int, default=16,
                      help='The number of parallel processes to run for '
                           'git push operations.')
  return parser


def _GetUpstreamBranch(project):
  """Return a best guess at the project's upstream branch name."""
  branch = project.upstream
  if branch and branch.startswith(BRANCH_REF_PREFIX):
    branch = branch[len(BRANCH_REF_PREFIX):]
  return branch


def _NeedsSnapshot(repo_root, project):
  """Test if project's revision is reachable from its upstream ref."""
  # Some projects don't have an upstream set. Try 'master' anyway.
  branch = _GetUpstreamBranch(project) or 'master'
  upstream_ref = 'refs/remotes/%s/%s' % (project.Remote().GitName(), branch)
  project_path = os.path.join(repo_root, project.Path())
  try:
    if git.IsReachable(project_path, project.revision, upstream_ref):
      return False
  except cros_build_lib.RunCommandError as e:
    logging.debug('Reachability check failed: %s', e)
  logging.info('Project %s revision %s not reachable from upstream %r.',
               project.name, project.revision, upstream_ref)
  return True


def _MakeUniqueRef(project, base_ref, used_refs):
  """Return a git ref for project that isn't in used_refs.

  Args:
    project: The Project object to create a ref for.
    base_ref: A base ref name; this may be appended to to generate a unique ref.
    used_refs: A set of ref names to uniquify against. It is updated with the
      newly generated ref.
  """
  ref = base_ref

  # If the project upstream is a non-master branch, append it to the ref.
  branch = _GetUpstreamBranch(project)
  if branch and branch != 'master':
    ref = '%s/%s' % (ref, branch)

  if ref in used_refs:
    # Append incrementing numbers until we find an unused ref.
    for i in range(1, len(used_refs) + 2):
      numbered = '%s/%d' % (ref, i)
      if numbered not in used_refs:
        ref = numbered
        break
    else:
      raise AssertionError('failed to make unique ref (ref=%s used_refs=%r)' %
                           (ref, used_refs))

  used_refs.add(ref)
  return ref


def _GitPushProjectUpstream(repo_root, project, dry_run):
  """Push the project revision to its remote upstream."""
  git.GitPush(
      os.path.join(repo_root, project.Path()),
      project.revision,
      git.RemoteRef(project.Remote().GitName(), project.upstream),
      dry_run=dry_run)


def main(argv):
  parser = GetParser()
  options = parser.parse_args(argv)
  options.Freeze()

  snapshot_ref = options.snapshot_ref
  if snapshot_ref and not snapshot_ref.startswith('refs/'):
    snapshot_ref = BRANCH_REF_PREFIX + snapshot_ref

  repo = repo_util.Repository.Find(options.repo_path)
  if repo is None:
    cros_build_lib.Die('No repo found in --repo_path %r.', options.repo_path)

  manifest = repo.Manifest(revision_locked=True)
  projects = list(manifest.Projects())

  # Check if projects need snapshots (in parallel).
  needs_snapshot_results = parallel.RunTasksInProcessPool(
      _NeedsSnapshot, [(repo.root, x) for x in projects])

  # Group snapshot-needing projects by project name.
  snapshot_projects = {}
  for project, needs_snapshot in zip(projects, needs_snapshot_results):
    if needs_snapshot:
      snapshot_projects.setdefault(project.name, []).append(project)

  if snapshot_projects and not snapshot_ref:
    cros_build_lib.Die('Some project(s) need snapshot refs but no '
                       '--snapshot-ref specified.')

  # Push snapshot refs (in parallel).
  with parallel.BackgroundTaskRunner(_GitPushProjectUpstream,
                                     repo.root, dry_run=options.dry_run,
                                     processes=options.jobs) as queue:
    for projects in snapshot_projects.values():
      # Since some projects (e.g. chromiumos/third_party/kernel) are checked out
      # multiple places, we may need to push each checkout to a unique ref.
      need_unique_refs = len(projects) > 1
      used_refs = set()
      for project in projects:
        if need_unique_refs:
          ref = _MakeUniqueRef(project, snapshot_ref, used_refs)
        else:
          ref = snapshot_ref
        # Update the upstream ref both for the push and the output XML.
        project.upstream = ref
        queue.put([project])

  dest = options.output_file
  if dest is None or dest == '-':
    dest = sys.stdout

  manifest.Write(dest)
