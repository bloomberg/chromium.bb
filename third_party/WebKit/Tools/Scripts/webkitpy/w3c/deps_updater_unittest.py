# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.w3c.deps_updater import DepsUpdater
from webkitpy.common.host_mock import MockHost


class DepsUpdaterTest(unittest.TestCase):

    def test_parse_try_job_results(self):
        output = """Successes:
linux_builder     http://example.com/linux_builder/builds/222
mac_builder       http://example.com/mac_builder/builds/222
win_builder       http://example.com/win_builder/builds/222
Failures:
android_builder   http://example.com/android_builder/builds/111
chromeos_builder  http://example.com/chromeos_builder/builds/111
win_builder       http://example.com/win_builder/builds/111
Started:
chromeos_generic  http://example.com/chromeos_generic/builds/111
chromeos_daisy    http://example.com/chromeos_daisy/builds/111
Total: 8 tryjobs
"""
        host = MockHost()
        updater = DepsUpdater(host)
        self.assertEqual(updater.parse_try_job_results(output), {
            'Successes': set([
                'mac_builder',
                'win_builder',
                'linux_builder'
            ]),
            'Failures': set([
                'android_builder',
                'chromeos_builder'
            ]),
            'Started': set([
                'chromeos_generic',
                'chromeos_daisy'
            ])
        })
