# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A database of OWNERS files."""

class Assertion(AssertionError):
  pass


class SyntaxErrorInOwnersFile(Exception):
  def __init__(self, path, line, msg):
    super(SyntaxErrorInOwnersFile, self).__init__((path, line, msg))
    self.path = path
    self.line = line
    self.msg = msg

  def __str__(self):
    if self.msg:
      return "%s:%d syntax error: %s" % (self.path, self.line, self.msg)
    else:
      return "%s:%d syntax error" % (self.path, self.line)


# Wildcard email-address in the OWNERS file.
ANYONE = '*'


class Database(object):
  """A database of OWNERS files for a repository.

  This class allows you to find a suggested set of reviewers for a list
  of changed files, and see if a list of changed files is covered by a
  list of reviewers."""

  def __init__(self, root, fopen, os_path):
    """Args:
      root: the path to the root of the Repository
      all_owners: the list of every owner in the system
      open: function callback to open a text file for reading
      os_path: module/object callback with fields for 'exists',
        'dirname', and 'join'
    """
    self.root = root
    self.fopen = fopen
    self.os_path = os_path

    # Mapping of files to authorized owners.
    self.files_owned_by = {}

    # Mapping of owners to the files they own.
    self.owners_for = {}

    # In-memory cached map of files to their OWNERS files.
    self.owners_file_for = {}

    # In-memory cache of OWNERS files and their contents
    self.owners_files = {}

  def ReviewersFor(self, files):
    """Returns a suggested set of reviewers that will cover the set of files.

    The set of files are paths relative to (and under) self.root."""
    self._LoadDataNeededFor(files)
    return self._CoveringSetOfOwnersFor(files)

  def FilesAreCoveredBy(self, files, reviewers):
    return not self.FilesNotCoveredBy(files, reviewers)

  def FilesNotCoveredBy(self, files, reviewers):
    covered_files = set()
    for reviewer in reviewers:
      covered_files = covered_files.union(self.files_owned_by[reviewer])
    return files.difference(covered_files)

  def _LoadDataNeededFor(self, files):
    for f in files:
      self._LoadOwnersFor(f)

  def _LoadOwnersFor(self, f):
    if f not in self.owners_for:
      owner_file = self._FindOwnersFileFor(f)
      self.owners_file_for[f] = owner_file
      self._ReadOwnersFile(owner_file, f)

  def _FindOwnersFileFor(self, f):
    # This is really a "do ... until dirname = ''"
    dirname = self.os_path.dirname(f)
    while dirname:
      owner_path = self.os_path.join(dirname, 'OWNERS')
      if self.os_path.exists(owner_path):
        return owner_path
      dirname = self.os_path.dirname(dirname)
    owner_path = self.os_path.join(dirname, 'OWNERS')
    if self.os_path.exists(owner_path):
      return owner_path
    raise Assertion('No OWNERS file found for %s' % f)

  def _ReadOwnersFile(self, owner_file, affected_file):
    owners_for = self.owners_for.setdefault(affected_file, set())
    for owner in self.fopen(owner_file):
      owner = owner.strip()
      self.files_owned_by.setdefault(owner, set()).add(affected_file)
      owners_for.add(owner)

  def _CoveringSetOfOwnersFor(self, files):
    # TODO(dpranke): implement the greedy algorithm for covering sets, and
    # consider returning multiple options in case there are several equally
    # short combinations of owners.
    every_owner = set()
    for f in files:
      every_owner = every_owner.union(self.owners_for[f])
    return every_owner
