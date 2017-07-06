# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import logging
import os
import urllib

from webkitpy.w3c.common import CHROMIUM_WPT_DIR

_log = logging.getLogger(__name__)
URL_BASE = 'https://chromium-review.googlesource.com'


class GerritAPI(object):
    """A utility class for the Chromium code review API.

    Wraps the API for Chromium's Gerrit instance at chromium-review.googlesource.com.
    """

    def __init__(self, host, user, token):
        self.host = host
        self.user = user
        self.token = token

    def get(self, path, raw=False):
        url = URL_BASE + path
        raw_data = self.host.web.get_binary(url)
        if raw:
            return raw_data

        # Gerrit API responses are prefixed by a 5-character JSONP preamble
        return json.loads(raw_data[5:])

    def post(self, path, data):
        url = URL_BASE + path
        assert self.user and self.token, 'Gerrit user and token required for authenticated routes.'

        b64auth = base64.b64encode('{}:{}'.format(self.user, self.token))
        headers = {
            'Authorization': 'Basic {}'.format(b64auth),
            'Content-Type': 'application/json',
        }
        return self.host.web.request('POST', url, data=json.dumps(data), headers=headers)

    def query_exportable_open_cls(self, limit=200):
        path = ('/changes/?q=project:\"chromium/src\"+status:open'
                '&o=CURRENT_FILES&o=CURRENT_REVISION&o=COMMIT_FOOTERS'
                '&o=DETAILED_LABELS&n={}').format(limit)
        open_cls_data = self.get(path)
        open_cls = [GerritCL(data, self) for data in open_cls_data]

        return [cl for cl in open_cls if cl.is_exportable()]


class GerritCL(object):
    """A data wrapper for a Chromium Gerrit CL."""

    def __init__(self, data, api):
        assert data['change_id']
        self._data = data
        self.api = api

    @property
    def url(self):
        return 'https://chromium-review.googlesource.com/c/%s' % self._data['_number']

    @property
    def subject(self):
        return self._data['subject']

    @property
    def change_id(self):
        return self._data['change_id']

    @property
    def owner_email(self):
        return self._data['owner']['email']

    @property
    def current_revision_sha(self):
        return self._data['current_revision']

    @property
    def current_revision(self):
        return self._data['revisions'][self.current_revision_sha]

    @property
    def has_review_started(self):
        return self._data.get('has_review_started')

    def latest_commit_message_with_footers(self):
        return self.current_revision['commit_with_footers']

    @property
    def current_revision_description(self):
        return self.current_revision['description']

    def post_comment(self, message):
        path = '/a/changes/{change_id}/revisions/current/review'.format(
            change_id=self.change_id,
        )
        return self.api.post(path, {'message': message})

    def is_exportable(self):
        files = self.current_revision['files'].keys()

        # Guard against accidental CLs that touch thousands of files.
        if len(files) > 1000:
            _log.info('Rejecting CL with over 1000 files: %s (ID: %s) ', self.subject, self.change_id)
            return False

        if self.subject.startswith('Import wpt@'):
            return False

        if 'Import' in self.subject:
            return False

        if 'No-Export: true' in self.current_revision['commit_with_footers']:
            return False

        if 'NOEXPORT=true' in self.current_revision['commit_with_footers']:
            return False

        files_in_wpt = [f for f in files if f.startswith('third_party/WebKit/LayoutTests/external/wpt')]
        if not files_in_wpt:
            return False

        exportable_files = [f for f in files_in_wpt if self.exportable_filename(f)]

        if not exportable_files:
            return False

        return True

    def exportable_filename(self, filename):
        """Returns True if the file could be exportable, or False otherwise."""
        filename = os.path.basename(filename.lower())
        return (
            not filename.endswith('-expected.txt')
            and not filename.startswith('.')
            and not filename.endswith('.json')
        )

    def get_patch(self):
        """Gets patch for latest revision of CL.

        Filtered to only contain diffs for changes in WPT.
        """
        path = '/changes/%s/revisions/current/patch' % self.change_id
        patch = base64.b64decode(self.api.get(path, raw=True))
        patch = self.filter_transform_patch(patch)

        return patch

    def filter_transform_patch(self, patch):
        """Filters a patch for only exportable changes.

        This method expects a `git diff`-formatted patch.
        """
        filtered_patch = []

        # Patch begins with message, always applicable.
        in_exportable_diff = True

        for line in patch.splitlines():
            # If we're not changing files, continue same behavior.
            if not line.startswith('diff --git'):
                if in_exportable_diff:
                    filtered_patch.append(line)
                continue

            # File is being changed, detect if it's exportable.
            if CHROMIUM_WPT_DIR in line and not line.endswith('-expected.txt'):
                in_exportable_diff = True
                filtered_patch.append(line)
            else:
                in_exportable_diff = False

        # Join into string; the newline at the end is required.
        if not filtered_patch[-1].strip():
            filtered_patch = filtered_patch[:-1]
        patch = '\n'.join(filtered_patch) + '\n'
        return patch.replace(CHROMIUM_WPT_DIR, '')
