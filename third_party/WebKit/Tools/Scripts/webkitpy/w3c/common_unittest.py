# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.w3c.common import read_credentials


class CommonTest(unittest.TestCase):

    def test_get_credentials_empty(self):
        host = MockHost()
        host.filesystem.write_text_file('/tmp/credentials.json', '{}')
        self.assertEqual(read_credentials(host, '/tmp/credentials.json'), {})

    def test_get_credentials_none(self):
        self.assertEqual(read_credentials(MockHost(), None), {})

    def test_get_credentials_gets_values_from_file(self):
        host = MockHost()
        host.filesystem.write_text_file(
            '/tmp/credentials.json',
            json.dumps({
                'GH_USER': 'user-github',
                'GH_TOKEN': 'pass-github',
                'GERRIT_USER': 'user-gerrit',
                'GERRIT_TOKEN': 'pass-gerrit',
            }))
        self.assertEqual(
            read_credentials(host, '/tmp/credentials.json'),
            {
                'GH_USER': 'user-github',
                'GH_TOKEN': 'pass-github',
                'GERRIT_USER': 'user-gerrit',
                'GERRIT_TOKEN': 'pass-gerrit',
            })
