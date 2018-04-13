#!/usr/bin/env vpython
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Pulls the latest revisions of the web-platform-tests."""

import os
import sys

# Without abspath(), PathFinder can't find chromium_base correctly.
sys.path.append(os.path.abspath(
    os.path.join(os.path.dirname(__file__), '..', '..',
                 'WebKit', 'Tools', 'Scripts')))
from webkitpy.common import exit_codes
from webkitpy.common.host import Host
from webkitpy.w3c.test_importer import TestImporter


def main():
    host = Host()
    importer = TestImporter(host)
    try:
        host.exit(importer.main())
    except KeyboardInterrupt:
        host.print_("Interrupted, exiting")
        host.exit(exit_codes.INTERRUPTED_EXIT_STATUS)


if __name__ == '__main__':
    main()
