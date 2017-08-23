# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions used both when importing and exporting."""

import json
import logging


WPT_GH_ORG = 'w3c'
WPT_GH_REPO_NAME = 'web-platform-tests'
WPT_GH_URL = 'https://github.com/%s/%s/' % (WPT_GH_ORG, WPT_GH_REPO_NAME)
WPT_MIRROR_URL = 'https://chromium.googlesource.com/external/w3c/web-platform-tests.git'
WPT_GH_SSH_URL_TEMPLATE = 'https://{}@github.com/%s/%s.git' % (WPT_GH_ORG, WPT_GH_REPO_NAME)
WPT_REVISION_FOOTER = 'WPT-Export-Revision:'
EXPORT_PR_LABEL = 'chromium-export'
PROVISIONAL_PR_LABEL = 'do not merge yet'

# TODO(qyearsley): Avoid hard-coding third_party/WebKit/LayoutTests.
CHROMIUM_WPT_DIR = 'third_party/WebKit/LayoutTests/external/wpt/'

_log = logging.getLogger(__name__)


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
