# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A script for exporting and importing changes between the Chromium repo
and the web-platform-tests repo.

TODO(jeffcarp): does not handle reverted changes right now
TODO(jeffcarp): it also doesn't handle changes to -expected.html files
TODO(jeffcarp): Currently this script only does export; also add an option
import as well.
"""

import argparse
import logging

from webkitpy.common.host import Host
from webkitpy.w3c.wpt_github import WPTGitHub
from webkitpy.w3c.test_exporter import TestExporter
from webkitpy.w3c.test_importer import configure_logging

_log = logging.getLogger(__name__)


def main():
    configure_logging()
    options = parse_args()
    host = Host()
    wpt_github = WPTGitHub(host)
    test_exporter = TestExporter(host, wpt_github, dry_run=options.dry_run)

    test_exporter.run()


def parse_args():
    parser = argparse.ArgumentParser(description='WPT Sync')
    parser.add_argument('--no-fetch', action='store_true')
    parser.add_argument('--dry-run', action='store_true')
    return parser.parse_args()
