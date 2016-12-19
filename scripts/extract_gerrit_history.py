# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build database associating Gerrit change # with commit metadata."""

from __future__ import print_function

import collections
import re

import git

from chromite.lib import clactions
from chromite.lib import commandline


_GERRIT_MESSAGE_REXP = "Reviewed-on: https://(.*?)(?:/gerrit)?/([0-9]*)\n"


def _GetParser():
  """Create the argparse parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('repo', action='store',
                      help='Path to git repository to examine.')
  parser.add_argument('--since', action='store', default='2014-01-01',
                      help='Date of earliest commit to examine. '
                           'Default: 2014-01-01')

  return parser


CommitInfo = collections.namedtuple('CommitInfo',
                                    ['gerrit_host', 'change_number',
                                     'hexsha'])


def _ParseCommitMessage(commit_message):
  """Extract gerrit_host and change_number from commit message.

  Args:
    commit_message: String commit message.

  Returns:
    gerrit_host, change_number tuple.

  Raises:
    ValueError if commit message does not match.
  """
  m = re.findall(_GERRIT_MESSAGE_REXP, commit_message)
  if not m:
    raise ValueError(
        'Commit message does not conform to Gerrit-reviewed pattern.')
  m = m[-1]
  return (m[0], m[1])


def _ProcessCommit(commit):
  """Extract info from a given commit.

  Args:
    commit: a git.Commit instance to process.

  Returns:
    CommitInfo instance if the given commit is a Gerrit-reviewed commit.
    None otherwise.
  """
  try:
    gerrit_host, change_number = _ParseCommitMessage(commit.message)
    return CommitInfo(gerrit_host, change_number, commit.hexsha)
  except ValueError:
    pass
  return None


def _ProcessRepo(repo, since):
  """Extracts gerrit information and associates with commit info.

  Args:
    repo: Path to git repository.
    since: date in YYYY-MM-DD format to process from.

  Returns:
    A list of (GerritChangeTuple, CommitInfo) for Gerrit-reviewed
    commits found in this |repo| after time |since|. Note: this list may
    contain duplicates, as historically some commits were cherry-picked
    forcefully or landed outside of the CQ, and may have innaccurate
    gerrit info extracted from their commit message.
  """
  r = git.Repo(repo)
  print('Examining git repository at %s' % r.working_dir)
  commits = r.iter_commits(since=since)
  commit_infos = []

  for c in commits:
    ci = _ProcessCommit(c)
    if ci:
      try:
        change_tuple = clactions.GerritChangeTuple.FromHostAndNumber(
            ci.gerrit_host, ci.change_number)
        commit_infos.append((change_tuple, ci))
      except ValueError:
        print('Unable to determine Gerrit host for commit %s' % ci.hexsha)

  return commit_infos


def DedupelicateChanges(changes):
  """Deduplicate a list of changes and index by change.

  Args:
    changes: A (GerritChangeTuple, CommitInfo) list to examine.

  Returns:
    A tuple of (GerritChangeTuple -> CommitInfo for all unique changes,
                GerritChangeTuple -> [CommitInfo] for duplicate changes.
  """
  changes_dict = {}
  duplicates = {}
  for c in changes:
    if c[0] in changes_dict:
      ci = changes_dict.pop(c[0])
      duplicates[c[0]] = [ci]
      duplicates[c[0]].append(c[1])
    elif c[0] in duplicates:
      duplicates[c[0]].append(c[1])
    else:
      changes_dict[c[0]] = c[1]

  return changes_dict, duplicates


def main(argv):
  parser = _GetParser()
  options = parser.parse_args(argv)

  changes_list = _ProcessRepo(options.repo, options.since)
  print('Found %s Gerrit-reviewed commits since %s.' %
        (len(changes_list), options.since))
  changes_dict, duplicates = DedupelicateChanges(changes_list)
  print('There were %s unique changes and %s changes with duplicates' %
        (len(changes_dict), len(duplicates)))

  if duplicates:
    for cl, commits in duplicates.items():
      print('The following hexshas are duplicates for change %s:\n%s'
            % (cl, ', '.join([c.hexsha for c in commits])))
