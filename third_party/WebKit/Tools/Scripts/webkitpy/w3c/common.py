# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions used both when importing and exporting."""

import logging

from webkitpy.w3c.chromium_commit import ChromiumCommit
from webkitpy.w3c.chromium_finder import absolute_chromium_dir


WPT_DEST_NAME = 'wpt'
WPT_GH_ORG = 'w3c'
WPT_GH_REPO_NAME = 'web-platform-tests'
WPT_GH_URL = 'https://github.com/%s/%s/' % (WPT_GH_ORG, WPT_GH_REPO_NAME)
WPT_GH_SSH_URL_TEMPLATE = 'https://{}@github.com/%s/%s.git' % (WPT_GH_ORG, WPT_GH_REPO_NAME)
WPT_REVISION_FOOTER = 'WPT-Export-Revision:'
DEFAULT_COMMIT_HISTORY_WINDOW = 5000

# TODO(qyearsley): This directory should be able to be constructed with
# PathFinder and WPT_DEST_NAME, plus the string "external".
CHROMIUM_WPT_DIR = 'third_party/WebKit/LayoutTests/external/wpt/'

# Our mirrors of the official wpt repo, which we pull from.
WPT_REPO_URL = 'https://chromium.googlesource.com/external/w3c/web-platform-tests.git'


_log = logging.getLogger(__name__)


def _exportable_commits_since(chromium_commit_hash, host, local_wpt):
    """Lists exportable commits after a certain point.

    Args:
        chromium_commit_hash: The SHA of the Chromium commit from which this
            method will look. This commit is not included in the commits searched.
        host: A Host object.
        local_wpt: A LocalWPT instance, used to see whether a Chromium commit
            can be applied cleanly in the upstream repo.

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
    return [commit for commit in chromium_commits if is_exportable(commit, local_wpt)]


def exportable_commits_over_last_n_commits(host, local_wpt, number=DEFAULT_COMMIT_HISTORY_WINDOW):
    """Lists exportable commits after a certain point.

    Args:
        number: Number of commits back to look. The commits to check will
            include all commits starting from the commit before HEAD~n, up
            to and including HEAD.
        host: A Host object.
        local_wpt: A LocalWPT instance, used to see whether a Chromium commit
            can be applied cleanly in the upstream repo.

    Returns:
        A list of ChromiumCommit objects for commits that are exportable after
        the given commit, in chronological order.
    """
    return _exportable_commits_since('HEAD~{}'.format(number + 1), host, local_wpt)


def is_exportable(chromium_commit, local_wpt):
    """Checks whether a given patch is exportable and can be applied."""
    patch = chromium_commit.format_patch()

    return ('NOEXPORT=true' not in chromium_commit.message() and
            not chromium_commit.message().startswith('Import') and
            patch and
            local_wpt.test_patch(patch, chromium_commit))
