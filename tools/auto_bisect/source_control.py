# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module contains the SourceControl class and related functions."""

import os

import bisect_utils

CROS_VERSION_PATTERN = 'new version number from %s'


def DetermineAndCreateSourceControl(opts):
  """Attempts to determine the underlying source control workflow and returns
  a SourceControl object.

  Returns:
    An instance of a SourceControl object, or None if the current workflow
    is unsupported.
  """
  (output, _) = bisect_utils.RunGit(['rev-parse', '--is-inside-work-tree'])

  if output.strip() == 'true':
    return GitSourceControl(opts)

  return None


# TODO(qyearsley): Almost all of the methods below could be top-level functions
# (or class methods). Refactoring may make this simpler.
# pylint: disable=R0201
class SourceControl(object):
  """SourceControl is an abstraction over the source control system."""

  def __init__(self):
    super(SourceControl, self).__init__()

  def SyncToRevisionWithGClient(self, revision):
    """Uses gclient to sync to the specified revision.

    This is like running gclient sync --revision <revision>.

    Args:
      revision: A git SHA1 hash or SVN revision number (depending on workflow).

    Returns:
      The return code of the call.
    """
    return bisect_utils.RunGClient(['sync', '--verbose', '--reset', '--force',
        '--delete_unversioned_trees', '--nohooks', '--revision', revision])

  def SyncToRevisionWithRepo(self, timestamp):
    """Uses the repo command to sync all the underlying git depots to the
    specified time.

    Args:
      timestamp: The Unix timestamp to sync to.

    Returns:
      The return code of the call.
    """
    return bisect_utils.RunRepoSyncAtTimestamp(timestamp)


class GitSourceControl(SourceControl):
  """GitSourceControl is used to query the underlying source control."""

  def __init__(self, opts):
    super(GitSourceControl, self).__init__()
    self.opts = opts

  def IsGit(self):
    return True

  def GetRevisionList(self, revision_range_end, revision_range_start, cwd=None):
    """Retrieves a list of revisions between |revision_range_start| and
    |revision_range_end|.

    Args:
      revision_range_end: The SHA1 for the end of the range.
      revision_range_start: The SHA1 for the beginning of the range.

    Returns:
      A list of the revisions between |revision_range_start| and
      |revision_range_end| (inclusive).
    """
    revision_range = '%s..%s' % (revision_range_start, revision_range_end)
    cmd = ['log', '--format=%H', '-10000', '--first-parent', revision_range]
    log_output = bisect_utils.CheckRunGit(cmd, cwd=cwd)

    revision_hash_list = log_output.split()
    revision_hash_list.append(revision_range_start)

    return revision_hash_list

  def SyncToRevision(self, revision, sync_client=None):
    """Syncs to the specified revision.

    Args:
      revision: The revision to sync to.
      use_gclient: Specifies whether or not we should sync using gclient or
        just use source control directly.

    Returns:
      True if successful.
    """

    if not sync_client:
      results = bisect_utils.RunGit(['checkout', revision])[1]
    elif sync_client == 'gclient':
      results = self.SyncToRevisionWithGClient(revision)
    elif sync_client == 'repo':
      results = self.SyncToRevisionWithRepo(revision)

    return not results

  def ResolveToRevision(self, revision_to_check, depot, depot_deps_dict,
                        search, cwd=None):
    """Tries to resolve an SVN revision or commit position to a git SHA1.

    Args:
      revision_to_check: The user supplied revision string that may need to be
        resolved to a git SHA1.
      depot: The depot the revision_to_check is from.
      depot_deps_dict: A dictionary with information about different depots.
      search: The number of changelists to try if the first fails to resolve
        to a git hash. If the value is negative, the function will search
        backwards chronologically, otherwise it will search forward.

    Returns:
      A string containing a git SHA1 hash, otherwise None.
    """
    # Android-chrome is git only, so no need to resolve this to anything else.
    if depot == 'android-chrome':
      return revision_to_check

    if depot != 'cros':
      if not bisect_utils.IsStringInt(revision_to_check):
        return revision_to_check

      depot_svn = 'svn://svn.chromium.org/chrome/trunk/src'

      if depot != 'chromium':
        depot_svn = depot_deps_dict[depot]['svn']

      svn_revision = int(revision_to_check)
      git_revision = None

      if search > 0:
        search_range = xrange(svn_revision, svn_revision + search, 1)
      else:
        search_range = xrange(svn_revision, svn_revision + search, -1)

      for i in search_range:
        svn_pattern = 'git-svn-id: %s@%d' % (depot_svn, i)
        commit_position_pattern = '^Cr-Commit-Position: .*@{#%d}' % i
        cmd = ['log', '--format=%H', '-1', '--grep', svn_pattern,
               '--grep', commit_position_pattern, 'origin/master']

        (log_output, return_code) = bisect_utils.RunGit(cmd, cwd=cwd)

        assert not return_code, 'An error occurred while running'\
                                ' "git %s"' % ' '.join(cmd)

        if not return_code:
          log_output = log_output.strip()

          if log_output:
            git_revision = log_output

            break

      return git_revision
    else:
      if bisect_utils.IsStringInt(revision_to_check):
        return int(revision_to_check)
      else:
        cwd = os.getcwd()
        os.chdir(os.path.join(os.getcwd(), 'src', 'third_party',
            'chromiumos-overlay'))
        pattern = CROS_VERSION_PATTERN % revision_to_check
        cmd = ['log', '--format=%ct', '-1', '--grep', pattern]

        git_revision = None

        log_output = bisect_utils.CheckRunGit(cmd, cwd=cwd)
        if log_output:
          git_revision = log_output
          git_revision = int(log_output.strip())
        os.chdir(cwd)

        return git_revision

  def IsInProperBranch(self):
    """Confirms they're in the master branch for performing the bisection.
    This is needed or gclient will fail to sync properly.

    Returns:
      True if the current branch on src is 'master'
    """
    cmd = ['rev-parse', '--abbrev-ref', 'HEAD']
    log_output = bisect_utils.CheckRunGit(cmd)
    log_output = log_output.strip()

    return log_output == "master"

  def GetCommitPosition(self, git_revision, cwd=None):
    """Finds git commit postion for the given git hash.

    This function executes "git footer --position-num <git hash>" command to get
    commit position the given revision.

    Args:
      git_revision: The git SHA1 to use.
      cwd: Working directory to run the command from.

    Returns:
      Git commit position as integer or None.
    """
    cmd = ['footers', '--position-num', git_revision]
    output = bisect_utils.CheckRunGit(cmd, cwd)
    commit_position = output.strip()

    if bisect_utils.IsStringInt(commit_position):
      return int(commit_position)

    return None

  def QueryRevisionInfo(self, revision, cwd=None):
    """Gathers information on a particular revision, such as author's name,
    email, subject, and date.

    Args:
      revision: Revision you want to gather information on.

    Returns:
      A dict in the following format:
      {
        'author': %s,
        'email': %s,
        'date': %s,
        'subject': %s,
        'body': %s,
      }
    """
    commit_info = {}

    formats = ['%aN', '%aE', '%s', '%cD', '%b']
    targets = ['author', 'email', 'subject', 'date', 'body']

    for i in xrange(len(formats)):
      cmd = ['log', '--format=%s' % formats[i], '-1', revision]
      output = bisect_utils.CheckRunGit(cmd, cwd=cwd)
      commit_info[targets[i]] = output.rstrip()

    return commit_info

  def CheckoutFileAtRevision(self, file_name, revision, cwd=None):
    """Performs a checkout on a file at the given revision.

    Returns:
      True if successful.
    """
    return not bisect_utils.RunGit(
        ['checkout', revision, file_name], cwd=cwd)[1]

  def RevertFileToHead(self, file_name):
    """Un-stages a file and resets the file's state to HEAD.

    Returns:
      True if successful.
    """
    # Reset doesn't seem to return 0 on success.
    bisect_utils.RunGit(['reset', 'HEAD', file_name])

    return not bisect_utils.RunGit(['checkout', bisect_utils.FILE_DEPS_GIT])[1]

  def QueryFileRevisionHistory(self, filename, revision_start, revision_end):
    """Returns a list of commits that modified this file.

    Args:
        filename: Name of file.
        revision_start: Start of revision range.
        revision_end: End of revision range.

    Returns:
        Returns a list of commits that touched this file.
    """
    cmd = ['log', '--format=%H', '%s~1..%s' % (revision_start, revision_end),
           '--', filename]
    output = bisect_utils.CheckRunGit(cmd)

    return [o for o in output.split('\n') if o]
