# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.buildbot import Build
from blinkpy.common.net.git_cl import TryJobStatus
from blinkpy.common.net.git_cl_mock import MockGitCL
from blinkpy.common.net.layout_test_results import LayoutTestResults
from blinkpy.common.path_finder import PathFinder
from blinkpy.web_tests.try_flag import TryFlag


class TryFlagTest(unittest.TestCase):

    def __init__(self, *args, **kwargs):
        self.linux_build = Build('linux_chromium_rel_ng', 100)
        self.mac_build = Build('mac_chromium_rel_ng', 101)
        self.win_build = Build('win7_chromium_rel_ng', 102)
        self.mock_try_results = {
            self.linux_build: TryJobStatus('COMPLETED', 'SUCCESS'),
            self.win_build: TryJobStatus('COMPLETED', 'SUCCESS'),
            self.mac_build: TryJobStatus('COMPLETED', 'SUCCESS')
        }
        super(TryFlagTest, self).__init__(*args, **kwargs)

    def _run_trigger_test(self, regenerate):
        host = MockHost()
        git = host.git()
        git_cl = MockGitCL(host)
        finder = PathFinder(host.filesystem)

        flag_file = finder.path_from_layout_tests(
            'additional-driver-flag.setting')
        flag_expectations_file = finder.path_from_layout_tests(
            'FlagExpectations', 'foo')

        cmd = ['trigger', '--flag=--foo']
        if regenerate:
            cmd.append('--regenerate')
        TryFlag(cmd, host, git_cl).run()

        expected_added_paths = {flag_file}
        expected_commits = [['Flag try job: force --foo for run_web_tests.py.']]

        if regenerate:
            expected_added_paths.add(flag_expectations_file)
            expected_commits.append(
                ['Flag try job: clear expectations for --foo.'])

        self.assertEqual(git.added_paths, expected_added_paths)
        self.assertEqual(git.local_commits(), expected_commits)

        self.assertEqual(git_cl.calls, [
            ['git', 'cl', 'upload', '--bypass-hooks', '-f',
             '-m', 'Flag try job for --foo.'],
            ['git', 'cl', 'try', '-B', 'luci.chromium.try',
             '-b', 'linux_chromium_rel_ng'],
            ['git', 'cl', 'try', '-B', 'master.tryserver.chromium.mac',
             '-b', 'mac_chromium_rel_ng'],
            ['git', 'cl', 'try', '-B', 'master.tryserver.chromium.win',
             '-b', 'win7_chromium_rel_ng']
        ])

    def test_trigger(self):
        self._run_trigger_test(regenerate=False)
        self._run_trigger_test(regenerate=True)

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
        filesystem = host.filesystem
        finder = PathFinder(filesystem)

        flag_expectations_file = finder.path_from_layout_tests(
            'FlagExpectations', 'foo')
        filesystem.write_text_file(
            flag_expectations_file,
            'something/pass-unexpectedly-mac.html [ Fail ]')

        self._setup_mock_results(host.buildbot)
        cmd = ['update', '--flag=--foo']
        TryFlag(cmd, host, MockGitCL(host, self.mock_try_results)).run()

        def results_url(build):
            return '%s/%s/%s/layout-test-results/results.html' % (
                'https://test-results.appspot.com/data/layout_results',
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

    def test_update_irrelevant_unexpected_pass(self):
        host = MockHost()
        filesystem = host.filesystem
        finder = PathFinder(filesystem)
        flag_expectations_file = finder.path_from_layout_tests(
            'FlagExpectations', 'foo')
        self._setup_mock_results(host.buildbot)
        cmd = ['update', '--flag=--foo']

        # Unexpected passes that don't have flag-specific failure expectations
        # should not be reported.
        filesystem.write_text_file(flag_expectations_file, '')
        TryFlag(cmd, host, MockGitCL(host, self.mock_try_results)).run()
        self.assertTrue('### 0 unexpected passes' in host.stdout.getvalue())

    def test_invalid_action(self):
        host = MockHost()
        cmd = ['invalid', '--flag=--foo']
        TryFlag(cmd, host, MockGitCL(host)).run()
        self.assertEqual(host.stderr.getvalue(),
                         'specify "trigger" or "update"\n')
