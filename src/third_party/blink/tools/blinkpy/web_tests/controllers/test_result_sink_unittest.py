# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import json
import mock
import re
import requests
import sys
import unittest
from six.moves.urllib.parse import urlparse

from blinkpy.common.host_mock import MockHost
from blinkpy.common.path_finder import RELATIVE_WEB_TESTS
from blinkpy.web_tests.controllers.test_result_sink import CreateTestResultSink
from blinkpy.web_tests.controllers.test_result_sink import TestResultSink
from blinkpy.web_tests.models import test_results
from blinkpy.web_tests.models.typ_types import ResultType
from blinkpy.web_tests.port.test import add_manifest_to_mock_filesystem
from blinkpy.web_tests.port.test import TestPort
from blinkpy.web_tests.port.test import WEB_TEST_DIR


class TestResultSinkTestBase(unittest.TestCase):
    def setUp(self):
        super(TestResultSinkTestBase, self).setUpClass()
        self.port = TestPort(MockHost())

    def luci_context(self, **section_values):
        if not section_values:
            return

        host = self.port.host
        f, fname = host.filesystem.open_text_tempfile()
        json.dump(section_values, f)
        f.close()
        host.environ['LUCI_CONTEXT'] = f.path


class TestCreateTestResultSink(TestResultSinkTestBase):
    def test_without_luci_context(self):
        self.assertIsNone(CreateTestResultSink(self.port))

    def test_without_result_sink_section(self):
        self.luci_context(app={'foo': 'bar'})
        self.assertIsNone(CreateTestResultSink(self.port))

    def test_auth_token(self):
        ctx = {'address': 'localhost:123', 'auth_token': 'secret'}
        self.luci_context(result_sink=ctx)
        rs = CreateTestResultSink(self.port)
        self.assertIsNotNone(rs)
        self.assertEqual(rs._session.headers['Authorization'],
                         'ResultSink ' + ctx['auth_token'])

    def test_with_result_sink_section(self):
        ctx = {'address': 'localhost:123', 'auth_token': 'secret'}
        self.luci_context(result_sink=ctx)
        rs = CreateTestResultSink(self.port)
        self.assertIsNotNone(rs)

        response = requests.Response()
        response.status_code = 200
        with mock.patch.object(rs._session, 'post',
                               return_value=response) as m:
            rs.sink(True, test_results.TestResult('test'), None)
            self.assertTrue(m.called)
            self.assertEqual(
                urlparse(m.call_args[0][0]).netloc, ctx['address'])


class TestResultSinkMessage(TestResultSinkTestBase):
    """Tests ResulkSink.sink."""

    def setUp(self):
        super(TestResultSinkMessage, self).setUp()
        patcher = mock.patch.object(TestResultSink, '_send')
        self.mock_send = patcher.start()
        self.addCleanup(patcher.stop)

        ctx = {'address': 'localhost:123', 'auth_token': 'super-secret'}
        self.luci_context(result_sink=ctx)
        self.rs = CreateTestResultSink(self.port)

    def sink(self, expected, test_result, expectations=None):
        self.rs.sink(expected, test_result, expectations)
        self.assertTrue(self.mock_send.called)
        return self.mock_send.call_args[0][0]['testResults'][0]

    def test_sink(self):
        tr = test_results.TestResult(test_name='test-name')
        tr.total_run_time = 123.456
        tr.type = ResultType.Crash
        sent_data = self.sink(True, tr)

        self.assertEqual(sent_data['testId'], 'test-name')
        self.assertEqual(sent_data['expected'], True)
        self.assertEqual(sent_data['status'], 'CRASH')
        self.assertEqual(sent_data['duration'], '123.456s')

    def test_sink_with_expectations(self):
        class FakeTestExpectation(object):
            def __init__(self):
                self.raw_results = ['Failure']

        class FakeExpectations(object):
            def __init__(self):
                self.system_condition_tags = ['tag1', 'tag2']

            def get_expectations(self, _):
                return FakeTestExpectation()

        # Values should be extracted from expectations.
        tr = test_results.TestResult(test_name='test-name')
        tr.type = ResultType.Crash
        expectations = FakeExpectations()
        expected_tags = [
            {
                'key': 'test_name',
                'value': 'test-name'
            },
            {
                'key': 'web_tests_device_failed',
                'value': 'False'
            },
            {
                'key': 'web_tests_result_type',
                'value': 'CRASH'
            },
            {
                'key': 'raw_typ_expectation',
                'value': 'Failure'
            },
            {
                'key': 'typ_tag',
                'value': 'tag1'
            },
            {
                'key': 'typ_tag',
                'value': 'tag2'
            },
        ]
        sent_data = self.sink(True, tr, expectations)
        self.assertEqual(sent_data['tags'], expected_tags)

    def test_sink_without_expectations(self):
        tr = test_results.TestResult(test_name='test-name')
        tr.type = ResultType.Crash
        expected_tags = [
            {
                'key': 'test_name',
                'value': 'test-name'
            },
            {
                'key': 'web_tests_device_failed',
                'value': 'False'
            },
            {
                'key': 'web_tests_result_type',
                'value': 'CRASH'
            },
        ]
        sent_data = self.sink(True, tr)
        self.assertEqual(sent_data['tags'], expected_tags)

    def test_test_metadata(self):
        tr = test_results.TestResult('')
        base_path = '//' + RELATIVE_WEB_TESTS

        tr.test_name = "test-name"
        self.assertDictEqual(
            self.sink(True, tr)['testMetadata'],
            {
                'name': 'test-name',
                'location': {
                    'repo': 'https://chromium.googlesource.com/chromium/src',
                    'fileName': base_path + 'test-name',
                },
            },
        )

        tr.test_name = "///test-name"
        self.assertDictEqual(
            self.sink(True, tr)['testMetadata'],
            {
                'name': '///test-name',
                'location': {
                    'repo': 'https://chromium.googlesource.com/chromium/src',
                    'fileName': base_path + '///test-name',
                },
            },
        )

    def test_device_failure(self):
        tr = test_results.TestResult(test_name='test-name')
        tr.type = ResultType.Failure
        tr.device_failed = True
        sent_data = self.sink(True, tr)

        # If the device failed, 'expected' and 'status' must be False and 'ABORT'
        self.assertEqual(sent_data['expected'], False)
        self.assertEqual(sent_data['status'], 'ABORT')

    def test_timeout(self):
        tr = test_results.TestResult(test_name='test-name')
        tr.type = ResultType.Timeout
        sent_data = self.sink(True, tr)

        # Timeout is considered as 'ABORT'
        self.assertEqual(sent_data['status'], 'ABORT')

    def test_artifacts(self):
        tr = test_results.TestResult(test_name='test-name')
        tr.artifacts.AddArtifact('test-image.png', '/tmp/test-image.png', True)
        tr.artifacts.AddArtifact('stdout', '/tmp/stdout', True)

        sent_data = self.sink(True, tr)
        self.assertDictEqual(
            sent_data['artifacts'], {
                'test-image.png': {
                    'filePath': '/tmp/test-image.png'
                },
                'stdout': {
                    'filePath': '/tmp/stdout'
                }
            })

    def test_artifacts_with_duplicate_paths(self):
        tr = test_results.TestResult(test_name='test-name')
        tr.artifacts.AddArtifact('artifact', '/tmp/foo', False)
        tr.artifacts.AddArtifact('artifact', '/tmp/bar', False)

        sent_data = self.sink(True, tr)
        self.assertDictEqual(
            sent_data['artifacts'], {
                'artifact': {
                    'filePath': '/tmp/foo'
                },
                'artifact-1': {
                    'filePath': '/tmp/bar'
                }
            })

    def test_summary_html(self):
        tr = test_results.TestResult(test_name='test-name')
        tr.artifacts.AddArtifact('stderr', '/tmp/stderr', False)
        tr.artifacts.AddArtifact('crash_log', '/tmp/crash_log', False)
        tr.artifacts.AddArtifact('command', '/tmp/cmd', False)

        sent_data = self.sink(True, tr)
        p = re.compile(
            '<text-artifact artifact-id="(command|stderr|crash_log)" />')

        self.assertListEqual(
            p.findall(sent_data['summaryHtml']),
            # The artifact tags should be sorted by the artifact names.
            ['command', 'crash_log', 'stderr'],
        )

    def assertFilename(self, test_name, expected_filename):
        sent_data = self.sink(True, test_results.TestResult(test_name))
        self.assertEqual(sent_data['testMetadata']['location']['fileName'],
                         '//' + RELATIVE_WEB_TESTS + expected_filename)

    def test_location_filename(self):
        self.assertFilename('real/test.html', 'real/test.html')

        # TestPort.virtual_test_suites() has a set of hard-coded virtualized
        # tests, and a test name must start with one of the virtual prefixes
        # and base in order for it to be recognized as a virtual test.
        self.assertFilename(
            'virtual/virtual_passes/passes/does_not_exist.html',
            'passes/does_not_exist.html')
        self.port.host.filesystem.write_text_file(
            self.port.host.filesystem.join(WEB_TEST_DIR, 'virtual',
                                           'virtual_passes', 'passes',
                                           'exists.html'),
            'body',
        )
        self.assertFilename('virtual/virtual_passes/passes/exists.html',
                            'virtual/virtual_passes/passes/exists.html')

    def test_wpt_location_filename(self):
        add_manifest_to_mock_filesystem(self.port)
        self.assertFilename(
            'external/wpt/html/parse.html?run_type=uri',
            'external/wpt/html/parse.html',
        )
        self.assertFilename(
            'virtual/virtual_wpt/external/wpt/dom/ranges/Range-attributes.html',
            'external/wpt/dom/ranges/Range-attributes.html',
        )
