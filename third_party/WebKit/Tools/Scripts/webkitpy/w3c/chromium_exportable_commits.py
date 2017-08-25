# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from webkitpy.w3c.chromium_commit import ChromiumCommit
from webkitpy.w3c.chromium_finder import absolute_chromium_dir
from webkitpy.w3c.common import CHROMIUM_WPT_DIR


DEFAULT_COMMIT_HISTORY_WINDOW = 5000


def exportable_commits_over_last_n_commits(
        host, local_wpt, wpt_github, number=DEFAULT_COMMIT_HISTORY_WINDOW, require_clean=True):
    """Lists exportable commits after a certain point.

    Exportable commits contain changes in the wpt directory and have not been
    exported (no corresponding closed PRs on GitHub). Commits made by importer
    are ignored. Exportable commits may or may not apply cleanly against the
    wpt HEAD (see argument require_clean).

    Args:
        host: A Host object.
        local_wpt: A LocalWPT instance, used to see whether a Chromium commit
            can be applied cleanly in the upstream repo.
        wpt_github: A WPTGitHub instance, used to check whether PRs are closed.
        number: The number of commits back to look. The commits to check will
            include all commits starting from the commit before HEAD~n, up
            to and including HEAD.
        require_clean: Whether to only return exportable commits that can be
            applied cleanly and produce non-empty diff when tested individually.

    Returns:
        (exportable_commits, errors) where exportable_commits is a list of
        ChromiumCommit objects for exportable commits in the given window, and
        errors is a list of error messages when exportable commits fail to apply
        cleanly, both in chronological order.
    """
    start_commit = 'HEAD~{}'.format(number + 1)
    return _exportable_commits_since(start_commit, host, local_wpt, wpt_github, require_clean)


def _exportable_commits_since(chromium_commit_hash, host, local_wpt, wpt_github, require_clean=True):
    """Lists exportable commits after the given commit.

    Args:
        chromium_commit_hash: The SHA of the Chromium commit from which this
            method will look. This commit is not included in the commits searched.

    Return values and remaining arguments are the same as exportable_commits_over_last_n_commits.
    """
    chromium_repo_root = host.executive.run_command([
        'git', 'rev-parse', '--show-toplevel'
    ], cwd=absolute_chromium_dir(host)).strip()

    wpt_path = chromium_repo_root + '/' + CHROMIUM_WPT_DIR
    commit_range = '{}..HEAD'.format(chromium_commit_hash)
    commit_hashes = host.executive.run_command([
        'git', 'rev-list', commit_range, '--reverse', '--', wpt_path
    ], cwd=absolute_chromium_dir(host)).splitlines()
    chromium_commits = [ChromiumCommit(host, sha=sha) for sha in commit_hashes]
    exportable_commits = []
    errors = []
    for commit in chromium_commits:
        state, error = get_commit_export_state(commit, local_wpt, wpt_github)
        if require_clean:
            success = state == CommitExportState.EXPORTABLE_CLEAN
        else:
            success = state in (CommitExportState.EXPORTABLE_CLEAN, CommitExportState.EXPORTABLE_DIRTY)
        if success:
            exportable_commits.append(commit)
        elif error != '':
            errors.append('The following commit did not apply cleanly:\nSubject: %s (%s)\n%s' %
                          (commit.subject(), commit.url(), error))
    return exportable_commits, errors


def get_commit_export_state(chromium_commit, local_wpt, wpt_github):
    """Determines the exportability state of a Chromium commit.

    Returns:
        (state, error): state is one of the members of CommitExportState;
        error is a string of error messages if an exportable patch fails to
        apply (i.e. state=CommitExportState.EXPORTABLE_DIRTY).
    """
    message = chromium_commit.message()
    if 'NOEXPORT=true' in message or 'No-Export: true' in message or message.startswith('Import'):
        return CommitExportState.IGNORED, ''

    patch = chromium_commit.format_patch()
    if not patch:
        return CommitExportState.NO_PATCH, ''

    # If there's a corresponding closed PR, then this commit should not
    # be considered exportable; the PR might have been merged and reverted,
    # or it might have been closed manually without merging.
    pull_request = wpt_github.pr_for_chromium_commit(chromium_commit)
    if pull_request and pull_request.state == 'closed':
        return CommitExportState.EXPORTED, ''

    success, error = local_wpt.test_patch(patch)
    return (CommitExportState.EXPORTABLE_CLEAN, '') if success else (CommitExportState.EXPORTABLE_DIRTY, error)


class CommitExportState(object):
    """An enum class for exportability states of a Chromium commit."""
    # pylint: disable=pointless-string-statement
    # String literals are used as attribute docstrings (PEP 257).

    IGNORED = 'ignored'
    """The commit was an import or contains No-Export tags."""

    NO_PATCH = 'no patch'
    """Failed to format patch from the commit."""

    EXPORTED = 'exported'
    """There is a corresponding upstream PR that has been closed (merged or abandoned)."""

    EXPORTABLE_DIRTY = 'exportable dirty'
    """The commit is exportable, but does not apply cleanly or produces empty diff."""

    EXPORTABLE_CLEAN = 'exportable clean'
    """The commit is exportable."""
