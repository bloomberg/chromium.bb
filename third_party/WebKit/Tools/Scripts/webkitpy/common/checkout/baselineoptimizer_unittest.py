# Copyright (C) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import hashlib
import sys
import webkitpy.thirdparty.unittest2 as unittest

from webkitpy.common.checkout.baselineoptimizer import BaselineOptimizer
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.host_mock import MockHost


class TestBaselineOptimizer(BaselineOptimizer):
    def __init__(self, mock_results_by_directory, create_mock_files, baseline_name):
        host = MockHost()
        BaselineOptimizer.__init__(self, host, host.port_factory.all_port_names())
        self._mock_results_by_directory = mock_results_by_directory
        self._filesystem = host.filesystem
        self._port_factory = host.port_factory
        self._created_mock_files = create_mock_files
        self._baseline_name = baseline_name

        self._create_mock_files(mock_results_by_directory)

    # We override this method for testing so we don't have to construct an
    # elaborate mock file system.
    def read_results_by_directory(self, baseline_name):
        if self._created_mock_files:
            return super(TestBaselineOptimizer, self).read_results_by_directory(baseline_name)
        return self._mock_results_by_directory

    def _move_baselines(self, baseline_name, results_by_directory, new_results_by_directory):
        self.new_results_by_directory.append(new_results_by_directory)

        if self._created_mock_files:
            super(TestBaselineOptimizer, self)._move_baselines(baseline_name, results_by_directory, new_results_by_directory)
            return
        self._mock_results_by_directory = new_results_by_directory

    def _create_mock_files(self, results_by_directory):
        root = self._port_factory.get().webkit_base()
        for directory in results_by_directory:
            if 'virtual' in directory:
                virtual_suite = self._port_factory.get().lookup_virtual_suite(self._baseline_name)
                if virtual_suite:
                    baseline_name = self._baseline_name[len(virtual_suite.name) + 1:]
                else:
                    baseline_name = self._baseline_name
            else:
                baseline_name = self._port_factory.get().lookup_virtual_test_base(self._baseline_name)
            path = self._filesystem.join(root, directory, baseline_name)
            self._filesystem.write_text_file(path, results_by_directory[directory])


class BaselineOptimizerTest(unittest.TestCase):
    VIRTUAL_DIRECTORY = 'virtual/softwarecompositing'

    def _appendVirtualSuffix(self, results_by_directory):
        new_results_by_directory = {}
        for directory in results_by_directory:
            new_results_by_directory[directory + '/' + self.VIRTUAL_DIRECTORY] = results_by_directory[directory]
        return new_results_by_directory

    def _assertOneLevelOptimization(self, results_by_directory, expected_new_results_by_directory, baseline_name, create_mock_files=False):
        baseline_optimizer = TestBaselineOptimizer(results_by_directory, create_mock_files, baseline_name)
        self.assertTrue(baseline_optimizer.optimize(baseline_name))
        if type(expected_new_results_by_directory) != list:
            expected_new_results_by_directory = [expected_new_results_by_directory]
        self.assertEqual(baseline_optimizer.new_results_by_directory, expected_new_results_by_directory)

    def _assertOptimization(self, results_by_directory, expected_new_results_by_directory):
        baseline_name = 'mock-baseline.png'
        self._assertOneLevelOptimization(results_by_directory, expected_new_results_by_directory, baseline_name)

        results_by_directory = self._appendVirtualSuffix(results_by_directory)
        expected_new_results_by_directory = self._appendVirtualSuffix(expected_new_results_by_directory)
        baseline_name = self.VIRTUAL_DIRECTORY + '/' + baseline_name
        self._assertOneLevelOptimization(results_by_directory, [expected_new_results_by_directory, expected_new_results_by_directory], baseline_name)

    def test_move_baselines(self):
        host = MockHost()
        host.filesystem.write_binary_file('/mock-checkout/LayoutTests/platform/win/another/test-expected.txt', 'result A')
        host.filesystem.write_binary_file('/mock-checkout/LayoutTests/platform/mac/another/test-expected.txt', 'result A')
        host.filesystem.write_binary_file('/mock-checkout/LayoutTests/another/test-expected.txt', 'result B')
        baseline_optimizer = BaselineOptimizer(host, host.port_factory.all_port_names())
        baseline_optimizer._move_baselines('another/test-expected.txt', {
            'LayoutTests/platform/win': 'aaa',
            'LayoutTests/platform/mac': 'aaa',
            'LayoutTests': 'bbb',
        }, {
            'LayoutTests': 'aaa',
        })
        self.assertEqual(host.filesystem.read_binary_file('/mock-checkout/LayoutTests/another/test-expected.txt'), 'result A')

    def test_linux_redundant_with_win(self):
        self._assertOptimization({
            'LayoutTests/platform/win': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/linux': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        }, {
            'LayoutTests/platform/win': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        })

    def test_covers_mac_win_linux(self):
        self._assertOptimization({
            'LayoutTests/platform/mac': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/win': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/linux': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        }, {
            'LayoutTests': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        })

    def test_overwrites_root(self):
        self._assertOptimization({
            'LayoutTests/platform/mac': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/win': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/linux': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests': '1',
        }, {
            'LayoutTests': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        })

    def test_no_new_common_directory(self):
        self._assertOptimization({
            'LayoutTests/platform/mac': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/linux': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests': '1',
        }, {
            'LayoutTests/platform/mac': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/linux': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests': '1',
        })


    def test_no_common_directory(self):
        self._assertOptimization({
            'LayoutTests/platform/mac': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/chromium': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        }, {
            'LayoutTests/platform/mac': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
            'LayoutTests/platform/chromium': '462d03b9c025db1b0392d7453310dbee5f9a9e74',
        })

    def test_local_optimization(self):
        self._assertOptimization({
            'LayoutTests/platform/mac': '1',
            'LayoutTests/platform/linux': '1',
            'LayoutTests/platform/linux-x86': '1',
        }, {
            'LayoutTests/platform/mac': '1',
            'LayoutTests/platform/linux': '1',
        })

    def test_local_optimization_skipping_a_port_in_the_middle(self):
        self._assertOptimization({
            'LayoutTests/platform/mac-snowleopard': '1',
            'LayoutTests/platform/win': '1',
            'LayoutTests/platform/linux-x86': '1',
        }, {
            'LayoutTests/platform/mac-snowleopard': '1',
            'LayoutTests/platform/win': '1',
        })

    def test_baseline_redundant_with_root(self):
        self._assertOptimization({
            'LayoutTests/platform/mac': '1',
            'LayoutTests/platform/win': '2',
            'LayoutTests': '2',
        }, {
            'LayoutTests/platform/mac': '1',
            'LayoutTests': '2',
        })

    def test_root_baseline_unused(self):
        self._assertOptimization({
            'LayoutTests/platform/mac': '1',
            'LayoutTests/platform/win': '2',
            'LayoutTests': '3',
        }, {
            'LayoutTests/platform/mac': '1',
            'LayoutTests/platform/win': '2',
        })

    def test_root_baseline_unused_and_non_existant(self):
        self._assertOptimization({
            'LayoutTests/platform/mac': '1',
            'LayoutTests/platform/win': '2',
        }, {
            'LayoutTests/platform/mac': '1',
            'LayoutTests/platform/win': '2',
        })

    def test_virtual_root_redundant_with_actual_root(self):
        baseline_name = self.VIRTUAL_DIRECTORY + '/mock-baseline.png'
        hash_of_two = hashlib.sha1('2').hexdigest()
        expected_result = [{'LayoutTests/virtual/softwarecompositing': hash_of_two}, {'LayoutTests': hash_of_two}]
        self._assertOneLevelOptimization({
            'LayoutTests/' + self.VIRTUAL_DIRECTORY: '2',
            'LayoutTests': '2',
        }, expected_result, baseline_name, create_mock_files=True)

    def test_virtual_root_redundant_with_ancestors(self):
        baseline_name = self.VIRTUAL_DIRECTORY + '/mock-baseline.png'
        hash_of_two = hashlib.sha1('2').hexdigest()
        expected_result = [{'LayoutTests/virtual/softwarecompositing': hash_of_two}, {'LayoutTests': hash_of_two}]
        self._assertOneLevelOptimization({
            'LayoutTests/' + self.VIRTUAL_DIRECTORY: '2',
            'LayoutTests/platform/mac': '2',
            'LayoutTests/platform/win': '2',
        }, expected_result, baseline_name, create_mock_files=True)

    def test_virtual_root_not_redundant_with_ancestors(self):
        baseline_name = self.VIRTUAL_DIRECTORY + '/mock-baseline.png'
        hash_of_two = hashlib.sha1('2').hexdigest()
        expected_result = [{'LayoutTests/virtual/softwarecompositing': hash_of_two}, {'LayoutTests/platform/mac': hash_of_two}]
        self._assertOneLevelOptimization({
            'LayoutTests/' + self.VIRTUAL_DIRECTORY: '2',
            'LayoutTests/platform/mac': '2',
        }, expected_result, baseline_name, create_mock_files=True)
