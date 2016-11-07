# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import base64
import httplib2
import json
import logging
import os

from webkitpy.common.host import Host
from webkitpy.common.system.executive import ScriptError
from webkitpy.w3c.chromium_wpt import ChromiumWPT
from webkitpy.w3c.local_wpt import LocalWPT
from webkitpy.w3c.test_importer import configure_logging

_log = logging.getLogger(__name__)


def main():
    configure_logging()
    parser = argparse.ArgumentParser(description='WPT Sync')
    parser.add_argument('--no-fetch', action='store_true')
    options = parser.parse_args()

    host = Host()

    # TODO(jeffcarp): the script does not handle reverted changes right now

    local_wpt = LocalWPT(host, no_fetch=options.no_fetch, use_github=True)
    chromium_wpt = ChromiumWPT(host)
    wpt_commit, chromium_commit = local_wpt.most_recent_chromium_commit()

    if chromium_commit:
        _log.info('Found last exported WPT commit:')
        _log.info('- web-platform-tests@%s', wpt_commit)
        _log.info('- chromium@%s', chromium_commit)
    else:
        _log.info('No Chromium export commits found in WPT, stopping.')
        return

    _log.info('Finding exportable commits in Chromium since %s...', chromium_commit)
    exportable_commits = chromium_wpt.exportable_commits_since(chromium_commit)

    if exportable_commits:
        _log.info('Found %s exportable commits in chromium:', len(exportable_commits))
        for commit in exportable_commits:
            _log.info('- %s %s', commit, chromium_wpt.subject(commit))
    else:
        _log.info('No exportable commits found in Chromium, stopping.')
        return

    for commit in exportable_commits:
        _log.info('Uploading %s', chromium_wpt.subject(commit))
        patch = chromium_wpt.format_patch(commit)
        message = chromium_wpt.message(commit)
        try:
            commit_position = chromium_wpt.commit_position(commit)
        except ScriptError as exp:
            _log.error(exp)
            _log.error('This could mean you have local commits on your chromium branch '
                       '(That lack a Cr-Commit-Position footer).')
            # TODO(jeffcarp): include flag that lets you exclude local commits
            raise

        assert commit_position
        message += '\n\nCr-Commit-Position: {}'.format(commit_position)
        branch_name = 'chromium-try-{}'.format(commit)
        local_wpt.create_branch_with_patch(branch_name, message, patch)

        desc_title = chromium_wpt.subject(commit)
        user = os.environ.get('GH_USER')
        assert user
        pr_branch_name = '{}:{}'.format(user, branch_name)
        github_create_pr(pr_branch_name, desc_title)


def github_auth_token():
    user = os.environ.get('GH_USER')
    token = os.environ.get('GH_TOKEN')
    if not (user and token):
        _log.error('GH_USER and/or GH_TOKEN env vars are not set.')
    assert user and token
    return base64.encodestring('{}:{}'.format(user, token))


def github_create_pr(branch_name, desc_title):
    # https://developer.github.com/v3/pulls/#create-a-pull-request
    conn = httplib2.Http()
    headers = {
        "Accept": "application/vnd.github.v3+json",
        "Authorization": "Basic " + github_auth_token()
    }
    body = {
        "title": desc_title,
        "body": "Test PR - testing export from Chromium",
        "head": branch_name,
        "base": "master"
    }
    resp, content = conn.request("https://api.github.com/repos/w3c/web-platform-tests/pulls",
                                 "POST", body=json.JSONEncoder().encode(body), headers=headers)
    _log.info("GitHub response: %s", content)
    if resp["status"] != "201":
        return None
    return json.loads(content)
