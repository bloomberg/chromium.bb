# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import flakytests

from webkitpy.common.checkout.scm.scm_mock import MockSCM
from webkitpy.layout_tests.layout_package import bot_test_expectations
from webkitpy.tool.commands.commandtest import CommandsTest
from webkitpy.tool.mocktool import MockTool, MockOptions
from webkitpy.layout_tests.port import builders

class FakeBotTestExpectations(object):
    def expectation_lines(self, only_ignore_very_flaky=False):
        return []


class FakeBotTestExpectationsFactory(object):
    FAILURE_MAP = {"A": "AUDIO", "C": "CRASH", "F": "TEXT", "I": "IMAGE", "O": "MISSING",
                   "N": "NO DATA", "P": "PASS", "T": "TIMEOUT", "Y": "NOTRUN", "X": "SKIP",
                   "Z": "IMAGE+TEXT", "K": "LEAK"}

    def _expectations_from_test_data(self, builder, test_data):
        test_data[bot_test_expectations.ResultsJSON.FAILURE_MAP_KEY] = self.FAILURE_MAP
        json_dict = {
            builder: test_data,
        }
        results = bot_test_expectations.ResultsJSON(builder, json_dict)
        return bot_test_expectations.BotTestExpectations(results, builders._exact_matches[builder]["specifiers"])

    def expectations_for_builder(self, builder):
        if builder == 'foo-builder':
            return self._expectations_from_test_data(builder, {
                'tests': {
                    'pass.html': {'results': [[2, 'FFFP']], 'expected': 'PASS'},
                }
            })

        if builder == 'bar-builder':
            return self._expectations_from_test_data(builder, {
                'tests': {
                    'pass.html': {'results': [[2, 'TTTP']], 'expected': 'PASS'},
                }
            })

        return FakeBotTestExpectations()


class ChangedExpectationsMockSCM(MockSCM):
    def changed_files(self):
        return ['LayoutTests/FlakyTests']


class FlakyTestsTest(CommandsTest):
    def test_merge_lines(self):
        command = flakytests.FlakyTests()
        factory = FakeBotTestExpectationsFactory()

        old_builders = builders._exact_matches
        builders._exact_matches = {
            "foo-builder": {"port_name": "dummy-port", "specifiers": ['Linux', 'Release']},
            "bar-builder": {"port_name": "dummy-port", "specifiers": ['Mac', 'Debug']},
        }

        try:
            lines = command._collect_expectation_lines(['foo-builder', 'bar-builder'], factory)
            self.assertEqual(len(lines), 1)
            self.assertEqual(lines[0].expectations, ['TEXT', 'TIMEOUT', 'PASS'])
            self.assertEqual(lines[0].specifiers, ['Mac', 'Linux'])
        finally:
            builders._exact_matches = old_builders


    def test_integration(self):
        command = flakytests.FlakyTests()
        tool = MockTool()
        command.expectations_factory = FakeBotTestExpectationsFactory
        options = MockOptions(upload=True)
        expected_stdout = """Updated /mock-checkout/third_party/WebKit/LayoutTests/FlakyTests
LayoutTests/FlakyTests is not changed, not uploading.
"""
        self.assert_execute_outputs(command, options=options, tool=tool, expected_stdout=expected_stdout)

        port = tool.port_factory.get()
        self.assertEqual(tool.filesystem.read_text_file(tool.filesystem.join(port.layout_tests_dir(), 'FlakyTests')), command.FLAKY_TEST_CONTENTS % '')

    def test_integration_uploads(self):
        command = flakytests.FlakyTests()
        tool = MockTool()
        tool.scm = ChangedExpectationsMockSCM
        command.expectations_factory = FakeBotTestExpectationsFactory
        reviewer = 'foo@chromium.org'
        options = MockOptions(upload=True, reviewers=reviewer)
        expected_stdout = """Updated /mock-checkout/third_party/WebKit/LayoutTests/FlakyTests
"""
        self.assert_execute_outputs(command, options=options, tool=tool, expected_stdout=expected_stdout)
        self.assertEqual(tool.executive.calls,
            [
                ['git', 'commit', '-m', command.COMMIT_MESSAGE % reviewer, '/mock-checkout/third_party/WebKit/LayoutTests/FlakyTests'],
                ['git', 'cl', 'upload', '--send-mail', '-f', '--cc', 'ojan@chromium.org,dpranke@chromium.org,eseidel@chromium.org'],
            ])

        port = tool.port_factory.get()
        self.assertEqual(tool.filesystem.read_text_file(tool.filesystem.join(port.layout_tests_dir(), 'FlakyTests')), command.FLAKY_TEST_CONTENTS % '')
