# Copyright (c) 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import os
import random
import threading

import gerrit_util
import git_common
import owners as owners_db
import scm


def _owner_combinations(owners, num_owners):
  """Iterate owners combinations by decrasing score.

  The score of an owner is its position on the owners list.
  The score of a set of owners is the maximum score of all owners on the set.

  Returns all combinations of up to `num_owners` sorted by decreasing score:
    _owner_combinations(['0', '1', '2', '3'], 2) == [
        # score 1
        ('1', '0'),
        # score 2
        ('2', '0'),
        ('2', '1'),
        # score 3
        ('3', '0'),
        ('3', '1'),
        ('3', '2'),
    ]
  """
  return reversed(list(itertools.combinations(reversed(owners), num_owners)))


class OwnersClient(object):
  """Interact with OWNERS files in a repository.

  This class allows you to interact with OWNERS files in a repository both the
  Gerrit Code-Owners plugin REST API, and the owners database implemented by
  Depot Tools in owners.py:

   - List all the owners for a group of files.
   - Check if files have been approved.
   - Suggest owners for a group of files.

  All code should use this class to interact with OWNERS files instead of the
  owners database in owners.py
  """
  # '*' means that everyone can approve.
  EVERYONE = '*'

  # Possible status of a file.
  # - INSUFFICIENT_REVIEWERS: The path needs owners approval, but none of its
  #   owners is currently a reviewer of the change.
  # - PENDING: An owner of this path has been added as reviewer, but approval
  #   has not been given yet.
  # - APPROVED: The path has been approved by an owner.
  APPROVED = 'APPROVED'
  PENDING = 'PENDING'
  INSUFFICIENT_REVIEWERS = 'INSUFFICIENT_REVIEWERS'

  def ListOwners(self, path):
    """List all owners for a file.

    The returned list is sorted so that better owners appear first.
    """
    raise Exception('Not implemented')

  def BatchListOwners(self, paths):
    """List all owners for a group of files.

    Returns a dictionary {path: [owners]}.
    """
    with git_common.ScopedPool(kind='threads') as pool:
      return dict(pool.imap_unordered(
          lambda p: (p, self.ListOwners(p)), paths))

  def GetFilesApprovalStatus(self, paths, approvers, reviewers):
    """Check the approval status for the given paths.

    Utility method to check for approval status when a change has not yet been
    created, given reviewers and approvers.

    See GetChangeApprovalStatus for description of the returned value.
    """
    approvers = set(approvers)
    if approvers:
      approvers.add(self.EVERYONE)
    reviewers = set(reviewers)
    if reviewers:
      reviewers.add(self.EVERYONE)
    status = {}
    owners_by_path = self.BatchListOwners(paths)
    for path, owners in owners_by_path.items():
      owners = set(owners)
      if owners.intersection(approvers):
        status[path] = self.APPROVED
      elif owners.intersection(reviewers):
        status[path] = self.PENDING
      else:
        status[path] = self.INSUFFICIENT_REVIEWERS
    return status

  def ScoreOwners(self, paths, exclude=None):
    """Get sorted list of owners for the given paths."""
    exclude = exclude or []
    positions_by_owner = {}
    owners_by_path = self.BatchListOwners(paths)
    for owners in owners_by_path.values():
      for i, owner in enumerate(owners):
        if owner in exclude:
          continue
        # Gerrit API lists owners of a path sorted by an internal score, so
        # owners that appear first should be prefered.
        # We define the score of an owner based on the pair
        # (# of files owned, minimum position on all owned files)
        positions_by_owner.setdefault(owner, []).append(i)

    # Sort owners by their score. Rank owners higher for more files owned and
    # lower for a larger minimum position across all owned files. Randomize
    # order for owners with same score to avoid bias.
    return sorted(
        positions_by_owner,
        key=lambda o: (-len(positions_by_owner[o]),
                       min(positions_by_owner[o]) + random.random()))

  def SuggestOwners(self, paths, exclude=None):
    """Suggest a set of owners for the given paths."""
    exclude = exclude or []
    paths_by_owner = {}
    owners_by_path = self.BatchListOwners(paths)
    for path, owners in owners_by_path.items():
      for owner in owners:
        if owner not in exclude:
          paths_by_owner.setdefault(owner, set()).add(path)

    # Select the minimum number of owners that can approve all paths.
    # We start at 2 to avoid sending all changes that require multiple
    # reviewers to top-level owners.
    owners = self.ScoreOwners(paths, exclude=exclude)
    if len(owners) < 2:
      return owners

    # Note that we have to iterate up to len(owners) + 1.
    # e.g. if there are only 2 owners, we should consider num_owners = 2.
    for num_owners in range(2, len(owners) + 1):
      # Iterate all combinations of `num_owners` by decreasing score, and
      # select the first one that covers all paths.
      for selected in _owner_combinations(owners, num_owners):
        covered = set.union(*(paths_by_owner[o] for o in selected))
        if len(covered) == len(paths):
          return list(selected)

    return []


class DepotToolsClient(OwnersClient):
  """Implement OwnersClient using owners.py Database."""
  def __init__(self, root, branch, fopen=open, os_path=os.path):
    super(DepotToolsClient, self).__init__()

    self._root = root
    self._branch = branch
    self._fopen = fopen
    self._os_path = os_path
    self._db = None
    self._db_lock = threading.Lock()

  def _ensure_db(self):
    if self._db is not None:
      return
    self._db = owners_db.Database(self._root, self._fopen, self._os_path)
    self._db.override_files = self._GetOriginalOwnersFiles()

  def _GetOriginalOwnersFiles(self):
    return {
      f: scm.GIT.GetOldContents(self._root, f, self._branch).splitlines()
      for _, f in scm.GIT.CaptureStatus(self._root, self._branch)
      if os.path.basename(f) == 'OWNERS'
    }

  def ListOwners(self, path):
    # all_possible_owners is not thread safe.
    with self._db_lock:
      self._ensure_db()
      # all_possible_owners returns a dict {owner: [(path, distance)]}. We want
      # to return a list of owners sorted by increasing distance.
      distance_by_owner = self._db.all_possible_owners([path], None)
      # We add a small random number to the distance, so that owners at the
      # same distance are returned in random order to avoid overloading those
      # who would appear first.
      return sorted(
          distance_by_owner,
          key=lambda o: distance_by_owner[o][0][1] + random.random())


class GerritClient(OwnersClient):
  """Implement OwnersClient using OWNERS REST API."""
  def __init__(self, host, project, branch):
    super(GerritClient, self).__init__()

    self._host = host
    self._project = project
    self._branch = branch
    self._owners_cache = {}

  def ListOwners(self, path):
    # Always use slashes as separators.
    path = path.replace(os.sep, '/')
    if path not in self._owners_cache:
      # GetOwnersForFile returns a list of account details sorted by order of
      # best reviewer for path. If owners have the same score, the order is
      # random.
      data = gerrit_util.GetOwnersForFile(
          self._host, self._project, self._branch, path,
          resolve_all_users=False)
      self._owners_cache[path] = [
        d['account']['email']
        for d in data['code_owners']
        if 'account' in d and 'email' in d['account']
      ]
      # If owned_by_all_users is true, add everyone as an owner at the end of
      # the owners list.
      if data.get('owned_by_all_users', False):
        self._owners_cache[path].append(self.EVERYONE)
    return self._owners_cache[path]


def GetCodeOwnersClient(root, host, project, branch):
  """Get a new OwnersClient.

  Defaults to GerritClient, and falls back to DepotToolsClient if code-owners
  plugin is not available."""
  if gerrit_util.IsCodeOwnersEnabled(host):
    return GerritClient(host, project, branch)
  return DepotToolsClient(root, branch)
