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

import unittest

from webkitpy.common.checkout.baseline_optimizer import BaselineOptimizer
from webkitpy.common.host_mock import MockHost
from webkitpy.common.path_finder import PathFinder


class BaselineOptimizerTest(unittest.TestCase):

    def _assert_optimization(self, results_by_directory, directory_to_new_results, baseline_dirname='', host=None):
        host = host or MockHost()
        fs = host.filesystem
        layout_tests_dir = PathFinder(fs).layout_tests_dir()
        baseline_name = 'mock-baseline-expected.txt'
        fs.write_text_file(
            fs.join(layout_tests_dir, 'VirtualTestSuites'),
            '[{"prefix": "gpu", "base": "fast/canvas", "args": ["--foo"]}]')

        for dirname, contents in results_by_directory.items():
            fs.write_binary_file(fs.join(layout_tests_dir, dirname, baseline_name), contents)

        baseline_optimizer = BaselineOptimizer(host, host.port_factory.get(), host.port_factory.all_port_names())
        self.assertTrue(baseline_optimizer.optimize(fs.join(baseline_dirname, baseline_name)))

        for dirname, contents in directory_to_new_results.items():
            path = fs.join(layout_tests_dir, dirname, baseline_name)
            if contents is not None:
                self.assertEqual(fs.read_binary_file(path), contents)

    def test_linux_redundant_with_win(self):
        self._assert_optimization(
            {
                'platform/win': '1',
                'platform/linux': '1',
            },
            {
                'platform/win': '1',
            })

    def test_covers_mac_win_linux(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/win': '1',
                'platform/linux': '1',
                '': None,
            },
            {
                '': '1',
            })

    def test_overwrites_root(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/win': '1',
                'platform/linux': '1',
                '': '2',
            },
            {
                '': '1',
            })

    def test_no_new_common_directory(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/linux': '1',
                '': '2',
            },
            {
                'platform/mac': '1',
                'platform/linux': '1',
                '': '2',
            })

    def test_local_optimization(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/linux': '1',
                'platform/linux-precise': '1',
            },
            {
                'platform/mac': '1',
                'platform/linux': '1',
            })

    def test_local_optimization_skipping_a_port_in_the_middle(self):
        self._assert_optimization(
            {
                'platform/mac-snowleopard': '1',
                'platform/win': '1',
                'platform/linux': '1',
                'platform/linux-precise': '1',
            },
            {
                'platform/mac-snowleopard': '1',
                'platform/win': '1',
            })

    def test_baseline_redundant_with_root(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/win': '2',
                '': '2',
            },
            {
                'platform/mac': '1',
                '': '2',
            })

    def test_root_baseline_unused(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/win': '2',
                '': '3',
            },
            {
                'platform/mac': '1',
                'platform/win': '2',
            })

    def test_root_baseline_unused_and_non_existant(self):
        self._assert_optimization(
            {
                'platform/mac': '1',
                'platform/win': '2',
            },
            {
                'platform/mac': '1',
                'platform/win': '2',
            })

    def test_virtual_root_redundant_with_actual_root(self):
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': '2',
                'fast/canvas': '2',
            },
            {
                'virtual/gpu/fast/canvas': None,
                'fast/canvas': '2',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_root_redundant_with_ancestors(self):
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': '2',
                'platform/win/fast/canvas': '2',
            },
            {
                'virtual/gpu/fast/canvas': None,
                'fast/canvas': '2',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    def test_virtual_root_not_redundant_with_ancestors(self):
        self._assert_optimization(
            {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': '1',
            },
            {
                'virtual/gpu/fast/canvas': '2',
                'platform/mac/fast/canvas': '1',
            },
            baseline_dirname='virtual/gpu/fast/canvas')

    # Tests for protected methods - pylint: disable=protected-access

    def test_move_baselines(self):
        host = MockHost()
        host.filesystem.write_text_file('/mock-checkout/third_party/WebKit/LayoutTests/VirtualTestSuites', '[]')
        host.filesystem.write_binary_file(
            '/mock-checkout/third_party/WebKit/LayoutTests/platform/win/another/test-expected.txt', 'result A')
        host.filesystem.write_binary_file(
            '/mock-checkout/third_party/WebKit/LayoutTests/platform/mac/another/test-expected.txt', 'result A')
        host.filesystem.write_binary_file('/mock-checkout/third_party/WebKit/LayoutTests/another/test-expected.txt', 'result B')
        baseline_optimizer = BaselineOptimizer(
            host, host.port_factory.get(), host.port_factory.all_port_names())
        baseline_optimizer._move_baselines(
            'another/test-expected.txt',
            {
                '/mock-checkout/third_party/WebKit/LayoutTests/platform/win': 'aaa',
                '/mock-checkout/third_party/WebKit/LayoutTests/platform/mac': 'aaa',
                '/mock-checkout/third_party/WebKit/LayoutTests': 'bbb',
            },
            {
                '/mock-checkout/third_party/WebKit/LayoutTests': 'aaa',
            })
        self.assertEqual(host.filesystem.read_binary_file(
            '/mock-checkout/third_party/WebKit/LayoutTests/another/test-expected.txt'), 'result A')

    def test_move_baselines_skip_git_commands(self):
        host = MockHost()
        host.filesystem.write_text_file('/mock-checkout/third_party/WebKit/LayoutTests/VirtualTestSuites', '[]')
        host.filesystem.write_binary_file(
            '/mock-checkout/third_party/WebKit/LayoutTests/platform/win/another/test-expected.txt', 'result A')
        host.filesystem.write_binary_file(
            '/mock-checkout/third_party/WebKit/LayoutTests/platform/mac/another/test-expected.txt', 'result A')
        host.filesystem.write_binary_file('/mock-checkout/third_party/WebKit/LayoutTests/another/test-expected.txt', 'result B')
        baseline_optimizer = BaselineOptimizer(host, host.port_factory.get(
        ), host.port_factory.all_port_names())
        baseline_optimizer._move_baselines(
            'another/test-expected.txt',
            {
                '/mock-checkout/third_party/WebKit/LayoutTests/platform/win': 'aaa',
                '/mock-checkout/third_party/WebKit/LayoutTests/platform/mac': 'aaa',
                '/mock-checkout/third_party/WebKit/LayoutTests': 'bbb',
            },
            {
                '/mock-checkout/third_party/WebKit/LayoutTests/platform/linux': 'bbb',
                '/mock-checkout/third_party/WebKit/LayoutTests': 'aaa',
            })
        self.assertEqual(
            host.filesystem.read_binary_file(
                '/mock-checkout/third_party/WebKit/LayoutTests/another/test-expected.txt'),
            'result A')
