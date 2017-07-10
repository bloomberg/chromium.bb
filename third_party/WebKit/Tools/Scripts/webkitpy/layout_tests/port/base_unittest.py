# Copyright (C) 2010 Google Inc. All rights reserved.
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

import functools
import json
import optparse
import unittest

from webkitpy.common.system.executive import ScriptError
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.output_capture import OutputCapture
from webkitpy.common.system.platform_info_mock import MockPlatformInfo
from webkitpy.common.system.system_host import SystemHost
from webkitpy.common.system.system_host_mock import MockSystemHost
from webkitpy.common.path_finder import PathFinder
from webkitpy.layout_tests.models.test_expectations import TestExpectations
from webkitpy.layout_tests.port.base import Port, VirtualTestSuite
from webkitpy.layout_tests.port.test import add_unit_tests_to_mock_filesystem, LAYOUT_TEST_DIR, TestPort


class PortTest(unittest.TestCase):

    def make_port(self, executive=None, with_tests=False, port_name=None, **kwargs):
        host = MockSystemHost()
        if executive:
            host.executive = executive
        if with_tests:
            add_unit_tests_to_mock_filesystem(host.filesystem)
            return TestPort(host, **kwargs)
        return Port(host, port_name or 'baseport', **kwargs)

    def test_pretty_patch_os_error(self):
        port = self.make_port(executive=MockExecutive(exception=OSError))
        self.assertEqual(port.pretty_patch_text('patch.txt'),
                         port._pretty_patch_error_html)

        # This tests repeated calls to make sure we cache the result.
        self.assertEqual(port.pretty_patch_text('patch.txt'),
                         port._pretty_patch_error_html)

    def test_pretty_patch_script_error(self):
        # FIXME: This is some ugly white-box test hacking ...
        port = self.make_port(executive=MockExecutive(exception=ScriptError))
        port._pretty_patch_available = True
        self.assertEqual(port.pretty_patch_text('patch.txt'),
                         port._pretty_patch_error_html)

        # This tests repeated calls to make sure we cache the result.
        self.assertEqual(port.pretty_patch_text('patch.txt'),
                         port._pretty_patch_error_html)

    def test_setup_test_run(self):
        port = self.make_port()
        # This routine is a no-op. We just test it for coverage.
        port.setup_test_run()

    def test_test_dirs(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(port.layout_tests_dir() + '/canvas/test', '')
        port.host.filesystem.write_text_file(port.layout_tests_dir() + '/css2.1/test', '')
        dirs = port.test_dirs()
        self.assertIn('canvas', dirs)
        self.assertIn('css2.1', dirs)

    def test_skipped_perf_tests(self):
        port = self.make_port()

        def add_text_file(dirname, filename, content='some content'):
            dirname = port.host.filesystem.join(
                PathFinder(port.host.filesystem).perf_tests_dir(), dirname)
            port.host.filesystem.maybe_make_directory(dirname)
            port.host.filesystem.write_text_file(port.host.filesystem.join(dirname, filename), content)

        add_text_file('inspector', 'test1.html')
        add_text_file('inspector', 'unsupported_test1.html')
        add_text_file('inspector', 'test2.html')
        add_text_file('inspector/resources', 'resource_file.html')
        add_text_file('unsupported', 'unsupported_test2.html')
        add_text_file('', 'Skipped', '\n'.join(['Layout', '', 'SunSpider', 'Supported/some-test.html']))
        self.assertEqual(port.skipped_perf_tests(), ['Layout', 'SunSpider', 'Supported/some-test.html'])

    def test_get_option__set(self):
        options, _ = optparse.OptionParser().parse_args([])
        options.foo = 'bar'
        port = self.make_port(options=options)
        self.assertEqual(port.get_option('foo'), 'bar')

    def test_get_option__unset(self):
        port = self.make_port()
        self.assertIsNone(port.get_option('foo'))

    def test_get_option__default(self):
        port = self.make_port()
        self.assertEqual(port.get_option('foo', 'bar'), 'bar')

    def test_additional_platform_directory(self):
        port = self.make_port(port_name='foo')
        port.FALLBACK_PATHS = {'': ['foo']}
        test_file = 'fast/test.html'

        # No additional platform directory
        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [(None, 'fast/test-expected.txt')])
        self.assertEqual(port.baseline_version_dir(), '/mock-checkout/third_party/WebKit/LayoutTests/platform/foo')

        # Simple additional platform directory
        port._options.additional_platform_directory = ['/tmp/local-baselines']
        port.host.filesystem.write_text_file('/tmp/local-baselines/fast/test-expected.txt', 'foo')
        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [('/tmp/local-baselines', 'fast/test-expected.txt')])
        self.assertEqual(port.baseline_version_dir(), '/tmp/local-baselines')

        # Multiple additional platform directories
        port._options.additional_platform_directory = ['/foo', '/tmp/local-baselines']
        self.assertEqual(
            port.expected_baselines(test_file, '.txt'),
            [('/tmp/local-baselines', 'fast/test-expected.txt')])
        self.assertEqual(port.baseline_version_dir(), '/foo')

        # Flag-specific baseline directory
        port._options.additional_platform_directory = []
        port._options.additional_driver_flag = ['--special-flag']
        self.assertEqual(port.baseline_search_path(), [
            '/mock-checkout/third_party/WebKit/LayoutTests/flag-specific/special-flag/platform/foo',
            '/mock-checkout/third_party/WebKit/LayoutTests/flag-specific/special-flag',
            '/mock-checkout/third_party/WebKit/LayoutTests/platform/foo'])

    def test_nonexistant_expectations(self):
        port = self.make_port(port_name='foo')
        port.expectations_files = lambda: ['/mock-checkout/third_party/WebKit/LayoutTests/platform/exists/TestExpectations',
                                           '/mock-checkout/third_party/WebKit/LayoutTests/platform/nonexistant/TestExpectations']
        port.host.filesystem.write_text_file('/mock-checkout/third_party/WebKit/LayoutTests/platform/exists/TestExpectations', '')
        self.assertEqual('\n'.join(port.expectations_dict().keys()),
                         '/mock-checkout/third_party/WebKit/LayoutTests/platform/exists/TestExpectations')

    def test_additional_expectations(self):
        port = self.make_port(port_name='foo')
        port.port_name = 'foo'
        port.host.filesystem.write_text_file(
            '/mock-checkout/third_party/WebKit/LayoutTests/platform/foo/TestExpectations', '')
        port.host.filesystem.write_text_file(
            '/tmp/additional-expectations-1.txt', 'content1\n')
        port.host.filesystem.write_text_file(
            '/tmp/additional-expectations-2.txt', 'content2\n')
        port.host.filesystem.write_text_file(
            '/mock-checkout/third_party/WebKit/LayoutTests/FlagExpectations/special-flag', 'content3')

        self.assertEqual('\n'.join(port.expectations_dict().values()), '')

        port._options.additional_expectations = [
            '/tmp/additional-expectations-1.txt']
        self.assertEqual('\n'.join(port.expectations_dict().values()), 'content1\n')

        port._options.additional_expectations = [
            '/tmp/nonexistent-file', '/tmp/additional-expectations-1.txt']
        self.assertEqual('\n'.join(port.expectations_dict().values()), 'content1\n')

        port._options.additional_expectations = [
            '/tmp/additional-expectations-1.txt', '/tmp/additional-expectations-2.txt']
        self.assertEqual('\n'.join(port.expectations_dict().values()), 'content1\n\ncontent2\n')

        port._options.additional_driver_flag = ['--special-flag']
        self.assertEqual('\n'.join(port.expectations_dict().values()), 'content3\ncontent1\n\ncontent2\n')

    def test_flag_specific_expectations(self):
        port = self.make_port(port_name='foo')
        port.port_name = 'foo'
        port.host.filesystem.write_text_file(
            '/mock-checkout/third_party/WebKit/LayoutTests/FlagExpectations/special-flag-a', 'aa')
        port.host.filesystem.write_text_file(
            '/mock-checkout/third_party/WebKit/LayoutTests/FlagExpectations/special-flag-b', 'bb')
        port.host.filesystem.write_text_file(
            '/mock-checkout/third_party/WebKit/LayoutTests/FlagExpectations/README.txt', 'cc')

        self.assertEqual('\n'.join(port.expectations_dict().values()), '')
        self.assertEqual('\n'.join(port.all_expectations_dict().values()), 'bb\naa')

    def test_additional_env_var(self):
        port = self.make_port(options=optparse.Values({'additional_env_var': ['FOO=BAR', 'BAR=FOO']}))
        self.assertEqual(port.get_option('additional_env_var'), ['FOO=BAR', 'BAR=FOO'])
        environment = port.setup_environ_for_server()
        self.assertTrue(('FOO' in environment) & ('BAR' in environment))
        self.assertEqual(environment['FOO'], 'BAR')
        self.assertEqual(environment['BAR'], 'FOO')

    def test_find_no_paths_specified(self):
        port = self.make_port(with_tests=True)
        tests = port.tests([])
        self.assertNotEqual(len(tests), 0)

    def test_find_one_test(self):
        port = self.make_port(with_tests=True)
        tests = port.tests(['failures/expected/image.html'])
        self.assertEqual(len(tests), 1)

    def test_find_glob(self):
        port = self.make_port(with_tests=True)
        tests = port.tests(['failures/expected/im*'])
        self.assertEqual(len(tests), 2)

    def test_find_with_skipped_directories(self):
        port = self.make_port(with_tests=True)
        tests = port.tests(['userscripts'])
        self.assertNotIn('userscripts/resources/iframe.html', tests)

    def test_find_with_skipped_directories_2(self):
        port = self.make_port(with_tests=True)
        tests = port.tests(['userscripts/resources'])
        self.assertEqual(tests, [])

    @staticmethod
    def _add_manifest_to_mock_file_system(filesystem):
        filesystem.write_text_file(LAYOUT_TEST_DIR + '/external/wpt/MANIFEST.json', json.dumps({
            'items': {
                'testharness': {
                    'dom/ranges/Range-attributes.html': [
                        ['/dom/ranges/Range-attributes.html', {}]
                    ],
                    'dom/ranges/Range-attributes-slow.html': [
                        ['/dom/ranges/Range-attributes.html', {'timeout': 'long'}]
                    ],
                    'console/console-is-a-namespace.any.js': [
                        ['/console/console-is-a-namespace.any.html', {}],
                        ['/console/console-is-a-namespace.any.worker.html', {}],
                    ],
                },
                'manual': {},
                'reftest': {
                    'html/dom/elements/global-attributes/dir_auto-EN-L.html': [
                        [
                            '/html/dom/elements/global-attributes/dir_auto-EN-L.html',
                            [
                                [
                                    '/html/dom/elements/global-attributes/dir_auto-EN-L-ref.html',
                                    '=='
                                ]
                            ],
                            {'timeout': 'long'}
                        ]
                    ],
                },
            }}))
        filesystem.write_text_file(LAYOUT_TEST_DIR + '/external/wpt/dom/ranges/Range-attributes.html', '')
        filesystem.write_text_file(LAYOUT_TEST_DIR + '/external/wpt/console/console-is-a-namespace.any.js', '')
        filesystem.write_text_file(LAYOUT_TEST_DIR + '/external/wpt/common/blank.html', 'foo')

    def test_find_none_if_not_in_manifest(self):
        port = self.make_port(with_tests=True)
        PortTest._add_manifest_to_mock_file_system(port.host.filesystem)
        self.assertNotIn('external/wpt/common/blank.html', port.tests([]))

    def test_find_one_if_in_manifest(self):
        port = self.make_port(with_tests=True)
        PortTest._add_manifest_to_mock_file_system(port.host.filesystem)
        self.assertIn('external/wpt/dom/ranges/Range-attributes.html', port.tests([]))
        self.assertNotIn('external/wpt/console/console-is-a-namespace.any.js', port.tests([]))

        # test.any.js shows up on the filesystem as one file but it effectively becomes two test files:
        # test.any.html and test.any.worker.html. We should support running test.any.js by name and
        # indirectly by specifying a parent directory.
        self.assertEqual(port.tests(['external']),
                         ['external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html',
                          'external/wpt/dom/ranges/Range-attributes.html',
                          'external/wpt/console/console-is-a-namespace.any.worker.html',
                          'external/wpt/console/console-is-a-namespace.any.html'])
        self.assertEqual(port.tests(['external/']),
                         ['external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html',
                          'external/wpt/dom/ranges/Range-attributes.html',
                          'external/wpt/console/console-is-a-namespace.any.worker.html',
                          'external/wpt/console/console-is-a-namespace.any.html'])
        self.assertEqual(port.tests(['external/csswg-test']), [])
        self.assertEqual(port.tests(['external/wpt']),
                         ['external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html',
                          'external/wpt/dom/ranges/Range-attributes.html',
                          'external/wpt/console/console-is-a-namespace.any.worker.html',
                          'external/wpt/console/console-is-a-namespace.any.html'])
        self.assertEqual(port.tests(['external/wpt/']),
                         ['external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html',
                          'external/wpt/dom/ranges/Range-attributes.html',
                          'external/wpt/console/console-is-a-namespace.any.worker.html',
                          'external/wpt/console/console-is-a-namespace.any.html'])
        self.assertEqual(port.tests(['external/wpt/console']),
                         ['external/wpt/console/console-is-a-namespace.any.worker.html',
                          'external/wpt/console/console-is-a-namespace.any.html'])
        self.assertEqual(port.tests(['external/wpt/console/']),
                         ['external/wpt/console/console-is-a-namespace.any.worker.html',
                          'external/wpt/console/console-is-a-namespace.any.html'])
        self.assertEqual(port.tests(['external/wpt/console/console-is-a-namespace.any.js']),
                         ['external/wpt/console/console-is-a-namespace.any.worker.html',
                          'external/wpt/console/console-is-a-namespace.any.html'])
        self.assertEqual(port.tests(['external/wpt/console/console-is-a-namespace.any.html']),
                         ['external/wpt/console/console-is-a-namespace.any.html'])
        self.assertEqual(port.tests(['external/wpt/dom']),
                         ['external/wpt/dom/ranges/Range-attributes.html'])
        self.assertEqual(port.tests(['external/wpt/dom/']),
                         ['external/wpt/dom/ranges/Range-attributes.html'])
        self.assertEqual(port.tests(['external/wpt/dom/ranges/Range-attributes.html']),
                         ['external/wpt/dom/ranges/Range-attributes.html'])

    def test_virtual_wpt_tests_paths(self):
        port = self.make_port(with_tests=True)
        PortTest._add_manifest_to_mock_file_system(port.host.filesystem)
        all_wpt = [
            'virtual/virtual_wpt/external/wpt/console/console-is-a-namespace.any.html',
            'virtual/virtual_wpt/external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html',
            'virtual/virtual_wpt/external/wpt/console/console-is-a-namespace.any.worker.html',
            'virtual/virtual_wpt/external/wpt/dom/ranges/Range-attributes.html',
        ]
        dom_wpt = [
            'virtual/virtual_wpt_dom/external/wpt/dom/ranges/Range-attributes.html',
        ]

        self.assertEqual(port.tests(['virtual/virtual_wpt/external/']), all_wpt)
        self.assertEqual(port.tests(['virtual/virtual_wpt/external/wpt/']), all_wpt)
        self.assertEqual(port.tests(['virtual/virtual_wpt/external/wpt/console']),
                         ['virtual/virtual_wpt/external/wpt/console/console-is-a-namespace.any.html',
                          'virtual/virtual_wpt/external/wpt/console/console-is-a-namespace.any.worker.html'])

        self.assertEqual(port.tests(['virtual/virtual_wpt_dom/external/wpt/dom/']), dom_wpt)
        self.assertEqual(port.tests(['virtual/virtual_wpt_dom/external/wpt/dom/ranges/']), dom_wpt)
        self.assertEqual(port.tests(['virtual/virtual_wpt_dom/external/wpt/dom/ranges/Range-attributes.html']), dom_wpt)

    def test_is_test_file(self):
        port = self.make_port(with_tests=True)
        is_test_file = functools.partial(Port.is_test_file, port, port.host.filesystem)
        self.assertTrue(is_test_file('', 'foo.html'))
        self.assertTrue(is_test_file('', 'foo.svg'))
        self.assertTrue(is_test_file('', 'test-ref-test.html'))
        self.assertTrue(is_test_file('inspector-unit', 'trie.js'))
        self.assertFalse(is_test_file('inspector-unit', 'foo.html'))
        self.assertFalse(is_test_file('inspector', 'devtools.js'))
        self.assertFalse(is_test_file('', 'foo.png'))
        self.assertFalse(is_test_file('', 'foo-expected.html'))
        self.assertFalse(is_test_file('', 'foo-expected.svg'))
        self.assertFalse(is_test_file('', 'foo-expected.xht'))
        self.assertFalse(is_test_file('', 'foo-expected-mismatch.html'))
        self.assertFalse(is_test_file('', 'foo-expected-mismatch.svg'))
        self.assertFalse(is_test_file('', 'foo-expected-mismatch.xhtml'))
        self.assertFalse(is_test_file('', 'foo-ref.html'))
        self.assertFalse(is_test_file('', 'foo-notref.html'))
        self.assertFalse(is_test_file('', 'foo-notref.xht'))
        self.assertFalse(is_test_file('', 'foo-ref.xhtml'))
        self.assertFalse(is_test_file('', 'ref-foo.html'))
        self.assertFalse(is_test_file('', 'notref-foo.xhr'))

    def test_is_test_file_in_wpt(self):
        port = self.make_port(with_tests=True)
        filesystem = port.host.filesystem
        PortTest._add_manifest_to_mock_file_system(filesystem)

        # A file not in MANIFEST.json is not a test even if it has .html suffix.
        self.assertFalse(port.is_test_file(filesystem, LAYOUT_TEST_DIR + '/external/wpt/common', 'blank.html'))

        # .js is not a test in general, but it is if MANIFEST.json contains an
        # entry for it.
        self.assertTrue(port.is_test_file(filesystem, LAYOUT_TEST_DIR + '/external/wpt/console', 'console-is-a-namespace.any.js'))

        # A file in external/wpt, not a sub directory.
        self.assertFalse(port.is_test_file(filesystem, LAYOUT_TEST_DIR + '/external/wpt', 'testharness_runner.html'))
        # A file in external/wpt_automation.
        self.assertTrue(port.is_test_file(filesystem, LAYOUT_TEST_DIR + '/external/wpt_automation', 'foo.html'))

    def test_is_wpt_test(self):
        port = self.make_port(with_tests=True)
        filesystem = port.host.filesystem
        PortTest._add_manifest_to_mock_file_system(filesystem)

        self.assertTrue(port.is_wpt_test('external/wpt/dom/ranges/Range-attributes.html'))
        self.assertTrue(port.is_wpt_test('external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html'))
        self.assertFalse(port.is_wpt_test('dom/domparsing/namespaces-1.html'))
        self.assertFalse(port.is_wpt_test('rutabaga'))

    def test_is_slow_wpt_test(self):
        port = self.make_port(with_tests=True)
        filesystem = port.host.filesystem
        PortTest._add_manifest_to_mock_file_system(filesystem)

        self.assertFalse(port.is_slow_wpt_test('external/wpt/dom/ranges/Range-attributes.html'))
        self.assertFalse(port.is_slow_wpt_test('dom/ranges/Range-attributes.html'))
        self.assertTrue(port.is_slow_wpt_test('external/wpt/dom/ranges/Range-attributes-slow.html'))
        self.assertTrue(port.is_slow_wpt_test('external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html'))

    def test_parse_reftest_list(self):
        port = self.make_port(with_tests=True)
        port.host.filesystem.files['bar/reftest.list'] = '\n'.join(['== test.html test-ref.html',
                                                                    '',
                                                                    '# some comment',
                                                                    '!= test-2.html test-notref.html # more comments',
                                                                    '== test-3.html test-ref.html',
                                                                    '== test-3.html test-ref2.html',
                                                                    '!= test-3.html test-notref.html',
                                                                    'fuzzy(80,500) == test-3 test-ref.html'])

        # Note that we don't support the syntax in the last line; the code should ignore it, rather than crashing.

        reftest_list = Port._parse_reftest_list(port.host.filesystem, 'bar')
        self.assertEqual(reftest_list, {
            'bar/test.html': [('==', 'bar/test-ref.html')],
            'bar/test-2.html': [('!=', 'bar/test-notref.html')],
            'bar/test-3.html': [('==', 'bar/test-ref.html'), ('==', 'bar/test-ref2.html'), ('!=', 'bar/test-notref.html')]})

    def test_reference_files(self):
        port = self.make_port(with_tests=True)
        self.assertEqual(port.reference_files('passes/svgreftest.svg'),
                         [('==', port.layout_tests_dir() + '/passes/svgreftest-expected.svg')])
        self.assertEqual(port.reference_files('passes/xhtreftest.svg'),
                         [('==', port.layout_tests_dir() + '/passes/xhtreftest-expected.html')])
        self.assertEqual(port.reference_files('passes/phpreftest.php'),
                         [('!=', port.layout_tests_dir() + '/passes/phpreftest-expected-mismatch.svg')])

    def test_reference_files_from_manifest(self):
        port = self.make_port(with_tests=True)
        PortTest._add_manifest_to_mock_file_system(port.host.filesystem)

        self.assertEqual(port.reference_files('external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html'),
                         [('==', port.layout_tests_dir() +
                           '/external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L-ref.html')])
        self.assertEqual(port.reference_files('virtual/layout_ng/' +
                                              'external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L.html'),
                         [('==', port.layout_tests_dir() +
                           '/external/wpt/html/dom/elements/global-attributes/dir_auto-EN-L-ref.html')])

    def test_operating_system(self):
        self.assertEqual('mac', self.make_port().operating_system())

    def test_http_server_supports_ipv6(self):
        port = self.make_port()
        self.assertTrue(port.http_server_supports_ipv6())
        port.host.platform.os_name = 'win'
        self.assertFalse(port.http_server_supports_ipv6())

    def test_http_server_requires_http_protocol_options_unsafe(self):
        port = self.make_port(executive=MockExecutive(stderr=(
            "Invalid command 'INTENTIONAL_SYNTAX_ERROR', perhaps misspelled or"
            " defined by a module not included in the server configuration\n")))
        port.path_to_apache = lambda: '/usr/sbin/httpd'
        self.assertTrue(
            port.http_server_requires_http_protocol_options_unsafe())

    def test_http_server_doesnt_require_http_protocol_options_unsafe(self):
        port = self.make_port(executive=MockExecutive(stderr=(
            "Invalid command 'HttpProtocolOptions', perhaps misspelled or"
            " defined by a module not included in the server configuration\n")))
        port.path_to_apache = lambda: '/usr/sbin/httpd'
        self.assertFalse(
            port.http_server_requires_http_protocol_options_unsafe())

    def test_check_httpd_success(self):
        port = self.make_port(executive=MockExecutive())
        port.path_to_apache = lambda: '/usr/sbin/httpd'
        capture = OutputCapture()
        capture.capture_output()
        self.assertTrue(port.check_httpd())
        _, _, logs = capture.restore_output()
        self.assertEqual('', logs)

    def test_httpd_returns_error_code(self):
        port = self.make_port(executive=MockExecutive(exit_code=1))
        port.path_to_apache = lambda: '/usr/sbin/httpd'
        capture = OutputCapture()
        capture.capture_output()
        self.assertFalse(port.check_httpd())
        _, _, logs = capture.restore_output()
        self.assertEqual('httpd seems broken. Cannot run http tests.\n', logs)

    def test_test_exists(self):
        port = self.make_port(with_tests=True)
        self.assertTrue(port.test_exists('passes'))
        self.assertTrue(port.test_exists('passes/text.html'))
        self.assertFalse(port.test_exists('passes/does_not_exist.html'))

        self.assertTrue(port.test_exists('virtual'))
        self.assertFalse(port.test_exists('virtual/does_not_exist.html'))
        self.assertTrue(port.test_exists('virtual/virtual_passes/passes/text.html'))

    def test_test_isfile(self):
        port = self.make_port(with_tests=True)
        self.assertFalse(port.test_isfile('passes'))
        self.assertTrue(port.test_isfile('passes/text.html'))
        self.assertFalse(port.test_isfile('passes/does_not_exist.html'))

        self.assertFalse(port.test_isfile('virtual'))
        self.assertTrue(port.test_isfile('virtual/virtual_passes/passes/text.html'))
        self.assertFalse(port.test_isfile('virtual/does_not_exist.html'))

    def test_test_isdir(self):
        port = self.make_port(with_tests=True)
        self.assertTrue(port.test_isdir('passes'))
        self.assertFalse(port.test_isdir('passes/text.html'))
        self.assertFalse(port.test_isdir('passes/does_not_exist.html'))
        self.assertFalse(port.test_isdir('passes/does_not_exist/'))

        self.assertTrue(port.test_isdir('virtual'))
        self.assertFalse(port.test_isdir('virtual/does_not_exist.html'))
        self.assertFalse(port.test_isdir('virtual/does_not_exist/'))
        self.assertFalse(port.test_isdir('virtual/virtual_passes/passes/text.html'))

    def test_tests(self):
        port = self.make_port(with_tests=True)
        tests = port.tests([])
        self.assertIn('passes/text.html', tests)
        self.assertIn('virtual/virtual_passes/passes/text.html', tests)

        tests = port.tests(['passes'])
        self.assertIn('passes/text.html', tests)
        self.assertIn('passes/virtual_passes/test-virtual-passes.html', tests)
        self.assertNotIn('virtual/virtual_passes/passes/text.html', tests)

        tests = port.tests(['virtual/virtual_passes/'])
        self.assertIn('virtual/virtual_passes/passes/test-virtual-passes.html', tests)
        self.assertIn('virtual/virtual_passes/passes_two/test-virtual-passes.html', tests)

        tests = port.tests(['virtual/virtual_passes/passes'])
        self.assertNotIn('passes/text.html', tests)
        self.assertIn('virtual/virtual_passes/passes/test-virtual-passes.html', tests)
        self.assertNotIn('virtual/virtual_passes/passes_two/test-virtual-passes.html', tests)

        self.assertNotIn('passes/test-virtual-passes.html', tests)
        self.assertNotIn('virtual/virtual_passes/passes/test-virtual-virtual/passes.html', tests)
        self.assertNotIn('virtual/virtual_passes/passes/virtual_passes/passes/test-virtual-passes.html', tests)

    def test_build_path(self):
        # Test for a protected method - pylint: disable=protected-access
        # Test that optional paths are used regardless of whether they exist.
        options = optparse.Values({'configuration': 'Release', 'build_directory': 'xcodebuild'})
        self.assertEqual(self.make_port(options=options)._build_path(), '/mock-checkout/xcodebuild/Release')

        # Test that "out" is used as the default.
        options = optparse.Values({'configuration': 'Release', 'build_directory': None})
        self.assertEqual(self.make_port(options=options)._build_path(), '/mock-checkout/out/Release')

    def test_dont_require_http_server(self):
        port = self.make_port()
        self.assertEqual(port.requires_http_server(), False)

    def test_can_load_actual_virtual_test_suite_file(self):
        port = Port(SystemHost(), 'baseport')

        # If this call returns successfully, we found and loaded the LayoutTests/VirtualTestSuites.
        _ = port.virtual_test_suites()

    def test_good_virtual_test_suite_file(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.layout_tests_dir(), 'VirtualTestSuites'),
            '[{"prefix": "bar", "base": "fast/bar", "args": ["--bar"]}]')

        # If this call returns successfully, we found and loaded the LayoutTests/VirtualTestSuites.
        _ = port.virtual_test_suites()

    def test_duplicate_virtual_test_suite_in_file(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.layout_tests_dir(), 'VirtualTestSuites'),
            '['
            '{"prefix": "bar", "base": "fast/bar", "args": ["--bar"]},'
            '{"prefix": "bar", "base": "fast/bar", "args": ["--bar"]}'
            ']')

        self.assertRaises(ValueError, port.virtual_test_suites)

    def test_virtual_test_suite_file_is_not_json(self):
        port = self.make_port()
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.layout_tests_dir(), 'VirtualTestSuites'),
            '{[{[')
        self.assertRaises(ValueError, port.virtual_test_suites)

    def test_missing_virtual_test_suite_file(self):
        port = self.make_port()
        self.assertRaises(AssertionError, port.virtual_test_suites)

    def test_is_wpt_test(self):
        port = self.make_port()
        self.assertTrue(port.is_wpt_test('external/wpt/foo/bar.html'))
        self.assertTrue(port.is_wpt_test('virtual/a-name/external/wpt/baz/qux.htm'))
        self.assertFalse(port.is_wpt_test('http/wpt/foo.html'))
        self.assertFalse(port.is_wpt_test('virtual/external/wpt/baz/qux.htm'))
        self.assertFalse(port.is_wpt_test('not-virtual/a-name/external/wpt/baz/qux.htm'))

    def test_default_results_directory(self):
        port = self.make_port(options=optparse.Values({'target': 'Default', 'configuration': 'Release'}))
        # By default the results directory is in the build directory: out/<target>.
        self.assertEqual(port.default_results_directory(), '/mock-checkout/out/Default/layout-test-results')

    def test_results_directory(self):
        port = self.make_port(options=optparse.Values({'results_directory': 'some-directory/results'}))
        # A results directory can be given as an option, and it is relative to current working directory.
        self.assertEqual(port.host.filesystem.cwd, '/')
        self.assertEqual(port.results_directory(), '/some-directory/results')

    def _assert_config_file_for_platform(self, port, platform, config_file):
        port.host.platform = MockPlatformInfo(os_name=platform)
        self.assertEqual(port._apache_config_file_name_for_platform(), config_file)  # pylint: disable=protected-access

    def _assert_config_file_for_linux_distribution(self, port, distribution, config_file):
        port.host.platform = MockPlatformInfo(os_name='linux', linux_distribution=distribution)
        self.assertEqual(port._apache_config_file_name_for_platform(), config_file)  # pylint: disable=protected-access

    def test_apache_config_file_name_for_platform(self):
        port = self.make_port()
        port._apache_version = lambda: '2.2'  # pylint: disable=protected-access
        self._assert_config_file_for_platform(port, 'linux', 'apache2-httpd-2.2.conf')
        self._assert_config_file_for_linux_distribution(port, 'arch', 'arch-httpd-2.2.conf')
        self._assert_config_file_for_linux_distribution(port, 'debian', 'debian-httpd-2.2.conf')
        self._assert_config_file_for_linux_distribution(port, 'slackware', 'apache2-httpd-2.2.conf')
        self._assert_config_file_for_linux_distribution(port, 'redhat', 'redhat-httpd-2.2.conf')

        self._assert_config_file_for_platform(port, 'mac', 'apache2-httpd-2.2.conf')
        self._assert_config_file_for_platform(port, 'win32', 'apache2-httpd-2.2.conf')
        self._assert_config_file_for_platform(port, 'barf', 'apache2-httpd-2.2.conf')

    def test_skips_test_in_smoke_tests(self):
        port = self.make_port(with_tests=True)
        port.default_smoke_test_only = lambda: True
        port.host.filesystem.write_text_file(port.path_to_smoke_tests_file(), 'passes/text.html\n')
        self.assertTrue(port.skips_test(
            'failures/expected/image.html',
            generic_expectations=TestExpectations(port, include_overrides=False),
            full_expectations=TestExpectations(port, include_overrides=True)))

    def test_skips_test_no_skip_smoke_tests_file(self):
        port = self.make_port(with_tests=True)
        port.default_smoke_test_only = lambda: True
        self.assertFalse(port.skips_test(
            'failures/expected/image.html',
            generic_expectations=TestExpectations(port, include_overrides=False),
            full_expectations=TestExpectations(port, include_overrides=True)))

    def test_skips_test_port_doesnt_skip_smoke_tests(self):
        port = self.make_port(with_tests=True)
        port.default_smoke_test_only = lambda: False
        self.assertFalse(port.skips_test(
            'failures/expected/image.html',
            generic_expectations=TestExpectations(port, include_overrides=False),
            full_expectations=TestExpectations(port, include_overrides=True)))

    def test_skips_test_skip_in_generic_expectations(self):
        port = self.make_port(with_tests=True)
        port.default_smoke_test_only = lambda: False
        port.host.filesystem.write_text_file(
            port.path_to_generic_test_expectations_file(),
            'Bug(test) failures/expected/image.html [ Skip ]\n')
        self.assertFalse(port.skips_test(
            'failures/expected/image.html',
            generic_expectations=TestExpectations(port, include_overrides=False),
            full_expectations=TestExpectations(port, include_overrides=True)))

    def test_skips_test_skip_in_full_expectations(self):
        port = self.make_port(with_tests=True)
        port.default_smoke_test_only = lambda: False
        port.host.filesystem.write_text_file(
            port.host.filesystem.join(port.layout_tests_dir(), 'NeverFixTests'),
            'Bug(test) failures/expected/image.html [ WontFix ]\n')
        self.assertTrue(port.skips_test(
            'failures/expected/image.html',
            generic_expectations=TestExpectations(port, include_overrides=False),
            full_expectations=TestExpectations(port, include_overrides=True)))


class NaturalCompareTest(unittest.TestCase):

    def setUp(self):
        self._port = TestPort(MockSystemHost())

    def assert_cmp(self, x, y, result):
        self.assertEqual(cmp(self._port._natural_sort_key(x), self._port._natural_sort_key(y)), result)

    def test_natural_compare(self):
        self.assert_cmp('a', 'a', 0)
        self.assert_cmp('ab', 'a', 1)
        self.assert_cmp('a', 'ab', -1)
        self.assert_cmp('', '', 0)
        self.assert_cmp('', 'ab', -1)
        self.assert_cmp('1', '2', -1)
        self.assert_cmp('2', '1', 1)
        self.assert_cmp('1', '10', -1)
        self.assert_cmp('2', '10', -1)
        self.assert_cmp('foo_1.html', 'foo_2.html', -1)
        self.assert_cmp('foo_1.1.html', 'foo_2.html', -1)
        self.assert_cmp('foo_1.html', 'foo_10.html', -1)
        self.assert_cmp('foo_2.html', 'foo_10.html', -1)
        self.assert_cmp('foo_23.html', 'foo_10.html', 1)
        self.assert_cmp('foo_23.html', 'foo_100.html', -1)


class KeyCompareTest(unittest.TestCase):

    def setUp(self):
        self._port = TestPort(MockSystemHost())

    def assert_cmp(self, x, y, result):
        self.assertEqual(cmp(self._port.test_key(x), self._port.test_key(y)), result)

    def test_test_key(self):
        self.assert_cmp('/a', '/a', 0)
        self.assert_cmp('/a', '/b', -1)
        self.assert_cmp('/a2', '/a10', -1)
        self.assert_cmp('/a2/foo', '/a10/foo', -1)
        self.assert_cmp('/a/foo11', '/a/foo2', 1)
        self.assert_cmp('/ab', '/a/a/b', -1)
        self.assert_cmp('/a/a/b', '/ab', 1)
        self.assert_cmp('/foo-bar/baz', '/foo/baz', -1)


class VirtualTestSuiteTest(unittest.TestCase):

    def test_basic(self):
        suite = VirtualTestSuite(prefix='suite', base='base/foo', args=['--args'])
        self.assertEqual(suite.name, 'virtual/suite/base/foo')
        self.assertEqual(suite.base, 'base/foo')
        self.assertEqual(suite.args, ['--args'])
        self.assertEqual(suite.reference_args, suite.args)

    def test_default_reference_args(self):
        suite = VirtualTestSuite(prefix='suite', base='base/foo', args=['--args'], references_use_default_args=True)
        self.assertEqual(suite.args, ['--args'])
        self.assertEqual(suite.reference_args, [])

    def test_non_default_reference_args(self):
        suite = VirtualTestSuite(prefix='suite', base='base/foo', args=['--args'], references_use_default_args=False)
        self.assertEqual(suite.args, ['--args'])
        self.assertEqual(suite.reference_args, suite.args)

    def test_no_slash(self):
        self.assertRaises(AssertionError, VirtualTestSuite, prefix='suite/bar', base='base/foo', args=['--args'])
