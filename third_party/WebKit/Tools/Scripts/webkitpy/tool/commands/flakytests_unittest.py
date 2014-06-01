# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from webkitpy.tool.commands.commandtest import CommandsTest
from webkitpy.tool.mocktool import MockTool, MockOptions

import flakytests


class FakeBotTestExpectations(object):
    def expectation_lines(self, only_ignore_very_flaky=False):
        return []


class FakeBotTestExpectationsFactory(object):
    def expectations_for_port(self, port_name):
        return FakeBotTestExpectations()


class FlakyTestsTest(CommandsTest):
    def test_simple(self):
        command = flakytests.FlakyTests()
        factory = FakeBotTestExpectationsFactory()
        lines = command._collect_expectation_lines(['foo'], factory)
        self.assertEqual(lines, [])

    def test_integration(self):
        command = flakytests.FlakyTests()
        command.expectations_factory = FakeBotTestExpectationsFactory
        options = MockOptions(upload=True)
        expected_stdout = """Updated /mock-checkout/third_party/WebKit/LayoutTests/FlakyTests
LayoutTests/FlakyTests is not changed, not uploading.
"""
        self.assert_execute_outputs(command, options=options, expected_stdout=expected_stdout)
