# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.host_mock import MockHost
from webkitpy.common.net.buildbot import Build
from webkitpy.common.net.git_cl import TryJobStatus
from webkitpy.common.net.git_cl_mock import MockGitCL
from webkitpy.common.net.layout_test_results import LayoutTestResults
from webkitpy.layout_tests.try_flag import TryFlag


class TryFlagTest(unittest.TestCase):

    def __init__(self, *args, **kwargs):
        self.linux_build = Build('linux_chromium_rel_ng', 100)
        self.mac_build = Build('mac_chromium_rel_ng', 101)
        self.win_build = Build('win7_chromium_rel_ng', 102)
        super(TryFlagTest, self).__init__(*args, **kwargs)

    def _setup_mock_results(self, buildbot):
        buildbot.set_results(self.linux_build, LayoutTestResults({
            'tests': {
                'something': {
                    'fail-everywhere.html': {
                        'expected': 'FAIL',
                        'actual': 'IMAGE+TEXT',
                        'is_unexpected': True
                    },
                    'fail-win-and-linux.html': {
                        'expected': 'FAIL',
                        'actual': 'IMAGE+TEXT',
                        'is_unexpected': True
                    }
                }
            }
        }))
        buildbot.set_results(self.win_build, LayoutTestResults({
            'tests': {
                'something': {
                    'fail-everywhere.html': {
                        'expected': 'FAIL',
                        'actual': 'IMAGE+TEXT',
                        'is_unexpected': True
                    },
                    'fail-win-and-linux.html': {
                        'expected': 'FAIL',
                        'actual': 'IMAGE+TEXT',
                        'is_unexpected': True
                    }
                }
            }
        }))
        buildbot.set_results(self.mac_build, LayoutTestResults({
            'tests': {
                'something': {
                    'pass-unexpectedly-mac.html': {
                        'expected': 'IMAGE+TEXT',
                        'actual': 'PASS',
                        'is_unexpected': True
                    },
                    'fail-everywhere.html': {
                        'expected': 'FAIL',
                        'actual': 'IMAGE+TEXT',
                        'is_unexpected': True
                    }
                }
            }
        }))

    def test_update(self):
        host = MockHost()
        try_jobs = {
            self.linux_build: TryJobStatus('COMPLETED', 'SUCCESS'),
            self.win_build: TryJobStatus('COMPLETED', 'SUCCESS'),
            self.mac_build: TryJobStatus('COMPLETED', 'SUCCESS')
        }
        self._setup_mock_results(host.buildbot)
        TryFlag(['update'], host, MockGitCL(host, try_jobs)).run()

        def results_url(build):
            return '%s/%s/%s/layout-test-results/results.html' % (
                'https://storage.googleapis.com/chromium-layout-test-archives',
                build.builder_name,
                build.build_number
            )
        self.assertEqual(host.stdout.getvalue(), '\n'.join([
            'Fetching results...',
            '-- Linux: %s' % results_url(self.linux_build),
            '-- Mac: %s' % results_url(self.mac_build),
            '-- Win: %s' % results_url(self.win_build),
            '',
            '### 1 unexpected passes:',
            '',
            'Bug(none) [ Mac ] something/pass-unexpectedly-mac.html [ Pass ]',
            '',
            '### 2 unexpected failures:',
            '',
            'Bug(none) something/fail-everywhere.html [ Failure ]',
            'Bug(none) [ Linux Win ] something/fail-win-and-linux.html [ Failure ]',
            ''
        ]))

    def test_invalid_action(self):
        host = MockHost()
        TryFlag(['invalid'], host, MockGitCL(host)).run()
        self.assertEqual(host.stderr.getvalue(),
                         'specify "trigger" or "update"\n')
