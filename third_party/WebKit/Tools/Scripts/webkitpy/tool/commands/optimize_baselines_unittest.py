# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse

from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.tool.commands.optimize_baselines import OptimizeBaselines
from webkitpy.tool.commands.rebaseline_unittest import BaseTestCase


class TestOptimizeBaselines(BaseTestCase):
    command_constructor = OptimizeBaselines

    def _write_test_file(self, port, path, contents):
        abs_path = self.tool.filesystem.join(port.layout_tests_dir(), path)
        self.tool.filesystem.write_text_file(abs_path, contents)

    def setUp(self):
        super(TestOptimizeBaselines, self).setUp()

    def test_modify_scm(self):
        test_port = self.tool.port_factory.get('test')
        self._write_test_file(test_port, 'another/test.html', "Dummy test contents")
        self._write_test_file(test_port, 'platform/test-mac-mac10.10/another/test-expected.txt', "result A")
        self._write_test_file(test_port, 'another/test-expected.txt', "result A")

        OutputCapture().assert_outputs(self, self.command.execute, args=[
            optparse.Values({'suffixes': 'txt', 'no_modify_scm': False, 'platform': 'test-mac-mac10.10'}),
            ['another/test.html'],
            self.tool,
        ], expected_stdout='{"add": [], "remove-lines": [], "delete": []}\n')

        self.assertFalse(self.tool.filesystem.exists(self.tool.filesystem.join(
            test_port.layout_tests_dir(), 'platform/test-mac-mac10.10/another/test-expected.txt')))
        self.assertTrue(self.tool.filesystem.exists(self.tool.filesystem.join(
            test_port.layout_tests_dir(), 'another/test-expected.txt')))

    def test_no_modify_scm(self):
        test_port = self.tool.port_factory.get('test')
        self._write_test_file(test_port, 'another/test.html', "Dummy test contents")
        self._write_test_file(test_port, 'platform/test-mac-mac10.10/another/test-expected.txt', "result A")
        self._write_test_file(test_port, 'another/test-expected.txt', "result A")

        OutputCapture().assert_outputs(self, self.command.execute, args=[
            optparse.Values({'suffixes': 'txt', 'no_modify_scm': True, 'platform': 'test-mac-mac10.10'}),
            ['another/test.html'],
            self.tool,
        ], expected_stdout=('{"add": [], "remove-lines": [], '
                            '"delete": ["/test.checkout/LayoutTests/platform/test-mac-mac10.10/another/test-expected.txt"]}\n'))

        self.assertFalse(self.tool.filesystem.exists(self.tool.filesystem.join(
            test_port.layout_tests_dir(), 'platform/mac/another/test-expected.txt')))
        self.assertTrue(self.tool.filesystem.exists(self.tool.filesystem.join(
            test_port.layout_tests_dir(), 'another/test-expected.txt')))

    def test_optimize_all_suffixes_by_default(self):
        test_port = self.tool.port_factory.get('test')
        self._write_test_file(test_port, 'another/test.html', "Dummy test contents")
        self._write_test_file(test_port, 'platform/test-mac-mac10.10/another/test-expected.txt', "result A")
        self._write_test_file(test_port, 'platform/test-mac-mac10.10/another/test-expected.png', "result A png")
        self._write_test_file(test_port, 'another/test-expected.txt', "result A")
        self._write_test_file(test_port, 'another/test-expected.png', "result A png")

        try:
            oc = OutputCapture()
            oc.capture_output()
            self.command.execute(
                optparse.Values({'suffixes': 'txt,wav,png', 'no_modify_scm': True, 'platform': 'test-mac-mac10.10'}),
                ['another/test.html'],
                self.tool)
        finally:
            out, _, _ = oc.restore_output()

        self.assertEquals(
            out,
            '{"add": [], "remove-lines": [], '
            '"delete": ["/test.checkout/LayoutTests/platform/test-mac-mac10.10/another/test-expected.txt", '
            '"/test.checkout/LayoutTests/platform/test-mac-mac10.10/another/test-expected.png"]}\n')
        self.assertFalse(
            self.tool.filesystem.exists(self.tool.filesystem.join(
                test_port.layout_tests_dir(), 'platform/mac/another/test-expected.txt')))
        self.assertFalse(
            self.tool.filesystem.exists(self.tool.filesystem.join(
                test_port.layout_tests_dir(), 'platform/mac/another/test-expected.png')))
        self.assertTrue(
            self.tool.filesystem.exists(self.tool.filesystem.join(
                test_port.layout_tests_dir(), 'another/test-expected.txt')))
        self.assertTrue(
            self.tool.filesystem.exists(self.tool.filesystem.join(
                test_port.layout_tests_dir(), 'another/test-expected.png')))
