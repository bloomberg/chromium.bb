# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from blinkpy.common.host_mock import MockHost
from blinkpy.common.net.bb_agent import BBAgent
from blinkpy.common.net.results_fetcher import Build
from blinkpy.common.system.executive_mock import MockExecutive


class BBAgentTest(unittest.TestCase):
    def setUp(self):
        self._host = MockHost()
        self._host.executive = MockExecutive(output='')
        self._bb_agent = BBAgent(self._host)

    def test_get_latest_build(self):
        self._bb_agent.get_latest_finished_build('linux-blink-rel')
        self.assertEqual(self._host.executive.calls[-1],
                         [self._bb_agent.bb_bin_path, 'ls', '-1', '-json',
                          '-status', 'ended', 'chromium/ci/linux-blink-rel'])

    def test_get_latest_try_build(self):
        self._bb_agent.get_latest_finished_build('linux-blink-rel',
                                                 try_build=True)
        self.assertEqual(self._host.executive.calls[-1],
                         [self._bb_agent.bb_bin_path, 'ls', '-1', '-json',
                          '-status', 'ended', 'chromium/try/linux-blink-rel'])

    def test_get_build_results(self):
        host = MockHost()
        host.executive = MockExecutive(
            output='{"number": 422, "id": "abcd"}')

        bb_agent = BBAgent(host)
        build = bb_agent.get_latest_finished_build('linux-blink-rel')
        self.assertEqual(build, Build('linux-blink-rel', 422, 'abcd'))
        bb_agent.get_build_test_results(build, 'blink_web_tests')
        self.assertEqual(host.executive.calls[-1],
                         [bb_agent.bb_bin_path, 'log', '-nocolor',
                          build.build_id, 'blink_web_tests', 'json.output'])
