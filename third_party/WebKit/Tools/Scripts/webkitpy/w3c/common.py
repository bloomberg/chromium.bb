# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions used both when importing and exporting."""

import json
import logging

from webkitpy.w3c.chromium_commit import ChromiumCommit
from webkitpy.w3c.chromium_finder import absolute_chromium_dir


WPT_GH_ORG = 'w3c'
WPT_GH_REPO_NAME = 'web-platform-tests'
WPT_GH_URL = 'https://github.com/%s/%s/' % (WPT_GH_ORG, WPT_GH_REPO_NAME)
WPT_MIRROR_URL = 'https://chromium.googlesource.com/external/w3c/web-platform-tests.git'
WPT_GH_SSH_URL_TEMPLATE = 'https://{}@github.com/%s/%s.git' % (WPT_GH_ORG, WPT_GH_REPO_NAME)
WPT_REVISION_FOOTER = 'WPT-Export-Revision:'
DEFAULT_COMMIT_HISTORY_WINDOW = 5000
EXPORT_PR_LABEL = 'chromium-export'
PROVISIONAL_PR_LABEL = 'do not merge yet'

# TODO(qyearsley): Avoid hard-coding third_party/WebKit/LayoutTests.
CHROMIUM_WPT_DIR = 'third_party/WebKit/LayoutTests/external/wpt/'

_log = logging.getLogger(__name__)


def exportable_commits_over_last_n_commits(
        host, local_wpt, wpt_github, number=DEFAULT_COMMIT_HISTORY_WINDOW):
    """Lists exportable commits after a certain point.

    Args:
        number: The number of commits back to look. The commits to check will
            include all commits starting from the commit before HEAD~n, up
            to and including HEAD.
        host: A Host object.
        local_wpt: A LocalWPT instance, used to see whether a Chromium commit
            can be applied cleanly in the upstream repo.
        wpt_github: A WPTGitHub instance, used to check whether PRs are closed.

    Returns:
        A list of ChromiumCommit objects for commits that are exportable after
        the given commit, in chronological order.
    """
    start_commit = 'HEAD~{}'.format(number + 1)
    return _exportable_commits_since(start_commit, host, local_wpt, wpt_github)


def _exportable_commits_since(chromium_commit_hash, host, local_wpt, wpt_github):
    """Lists exportable commits after a certain point.

    Args:
        chromium_commit_hash: The SHA of the Chromium commit from which this
            method will look. This commit is not included in the commits searched.
        host: A Host object.
        local_wpt: A LocalWPT instance.
        wpt_github: A WPTGitHub instance.

    Returns:
        A list of ChromiumCommit objects for commits that are exportable after
        the given commit, in chronological order.
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
    for commit in chromium_commits:
        if is_exportable(commit, local_wpt, wpt_github):
            exportable_commits.append(commit)
    return exportable_commits


def is_exportable(chromium_commit, local_wpt, wpt_github):
    """Checks whether a given patch is exportable and can be applied."""
    message = chromium_commit.message()
    if 'NOEXPORT=true' in message or 'No-Export: true' in message or message.startswith('Import'):
        return False
    patch = chromium_commit.format_patch()
    if not (patch and local_wpt.test_patch(patch, chromium_commit)):
        return False

    # If there's a corresponding closed PR, then this commit should not
    # be considered exportable; the PR might have been merged and reverted,
    # or it might have been closed manually without merging.
    pull_request = wpt_github.pr_for_chromium_commit(chromium_commit)
    if pull_request and pull_request.state == 'closed':
        return False
    return True


def read_credentials(host, credentials_json):
    """Extracts credentials from a JSON file."""
    if not credentials_json:
        return {}
    if not host.filesystem.exists(credentials_json):
        _log.warning('Credentials JSON file not found at %s.', credentials_json)
        return {}
    credentials = {}
    contents = json.loads(host.filesystem.read_text_file(credentials_json))
    for key in ('GH_USER', 'GH_TOKEN', 'GERRIT_USER', 'GERRIT_TOKEN'):
        if key in contents:
            credentials[key] = contents[key]
    return credentials
