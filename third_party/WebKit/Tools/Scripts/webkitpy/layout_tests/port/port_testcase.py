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

"""Unit testing base class for Port implementations."""

import errno
import logging
import os
import socket
import sys
import time
import webkitpy.thirdparty.unittest2 as unittest

from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.layout_tests.port.base import Port
from webkitpy.layout_tests.port.server_process_mock import MockServerProcess
from webkitpy.layout_tests.servers import http_server_base
from webkitpy.tool.mocktool import MockOptions


# FIXME: get rid of this fixture
class TestWebKitPort(Port):
    port_name = "testwebkitport"

    def __init__(self, port_name=None, symbols_string=None,
                 expectations_file=None, skips_file=None, host=None, config=None,
                 **kwargs):
        port_name = port_name or TestWebKitPort.port_name
        self.symbols_string = symbols_string  # Passing "" disables all staticly-detectable features.
        host = host or MockSystemHost()
        super(TestWebKitPort, self).__init__(host, port_name=port_name, **kwargs)

    def all_test_configurations(self):
        return [self.test_configuration()]

    def _symbols_string(self):
        return self.symbols_string

    def _tests_for_disabled_features(self):
        return ["accessibility", ]


class PortTestCase(unittest.TestCase):
    """Tests that all Port implementations must pass."""
    HTTP_PORTS = (8000, 8080, 8443)
    WEBSOCKET_PORTS = (8880,)

    # Subclasses override this to point to their Port subclass.
    os_name = None
    os_version = None
    port_maker = TestWebKitPort
    port_name = None

    def make_port(self, host=None, port_name=None, options=None, os_name=None, os_version=None, **kwargs):
        host = host or MockSystemHost(os_name=(os_name or self.os_name), os_version=(os_version or self.os_version))
        options = options or MockOptions(configuration='Release')
        port_name = port_name or self.port_name
        port_name = self.port_maker.determine_full_port_name(host, options, port_name)
        port = self.port_maker(host, port_name, options=options, **kwargs)
        port._config.build_directory = lambda configuration: '/mock-build'
        return port

    def make_wdiff_available(self, port):
        port._wdiff_available = True

    def test_default_max_locked_shards(self):
        port = self.make_port()
        port.default_child_processes = lambda: 16
        self.assertEqual(port.default_max_locked_shards(), 1)
        port.default_child_processes = lambda: 2
        self.assertEqual(port.default_max_locked_shards(), 1)

    def test_default_timeout_ms(self):
        self.assertEqual(self.make_port(options=MockOptions(configuration='Release')).default_timeout_ms(), 6000)
        self.assertEqual(self.make_port(options=MockOptions(configuration='Debug')).default_timeout_ms(), 18000)

    def test_default_pixel_tests(self):
        self.assertEqual(self.make_port().default_pixel_tests(), True)

    def test_driver_cmd_line(self):
        port = self.make_port()
        self.assertTrue(len(port.driver_cmd_line()))

        options = MockOptions(additional_drt_flag=['--foo=bar', '--foo=baz'])
        port = self.make_port(options=options)
        cmd_line = port.driver_cmd_line()
        self.assertTrue('--foo=bar' in cmd_line)
        self.assertTrue('--foo=baz' in cmd_line)

    def test_uses_apache(self):
        self.assertTrue(self.make_port()._uses_apache())

    def assert_servers_are_down(self, host, ports):
        for port in ports:
            try:
                test_socket = socket.socket()
                test_socket.connect((host, port))
                self.fail()
            except IOError, e:
                self.assertTrue(e.errno in (errno.ECONNREFUSED, errno.ECONNRESET))
            finally:
                test_socket.close()

    def assert_servers_are_up(self, host, ports):
        for port in ports:
            try:
                test_socket = socket.socket()
                test_socket.connect((host, port))
            except IOError, e:
                self.fail('failed to connect to %s:%d' % (host, port))
            finally:
                test_socket.close()

    def test_diff_image__missing_both(self):
        port = self.make_port()
        self.assertEqual(port.diff_image(None, None), (None, None))
        self.assertEqual(port.diff_image(None, ''), (None, None))
        self.assertEqual(port.diff_image('', None), (None, None))

        self.assertEqual(port.diff_image('', ''), (None, None))

    def test_diff_image__missing_actual(self):
        port = self.make_port()
        self.assertEqual(port.diff_image(None, 'foo'), ('foo', None))
        self.assertEqual(port.diff_image('', 'foo'), ('foo', None))

    def test_diff_image__missing_expected(self):
        port = self.make_port()
        self.assertEqual(port.diff_image('foo', None), ('foo', None))
        self.assertEqual(port.diff_image('foo', ''), ('foo', None))

    def test_diff_image(self):
        port = self.make_port()
        self.proc = None

        def make_proc(port, nm, cmd, env):
            self.proc = MockServerProcess(port, nm, cmd, env, lines=['diff: 100% failed\n', 'diff: 100% failed\n'])
            return self.proc

        port._server_process_constructor = make_proc
        port.setup_test_run()
        self.assertEqual(port.diff_image('foo', 'bar'), ('', 100.0, None))

        port.clean_up_test_run()
        self.assertTrue(self.proc.stopped)
        self.assertEqual(port._image_differ, None)

    def test_check_wdiff(self):
        port = self.make_port()
        port.check_wdiff()

    def test_wdiff_text_fails(self):
        host = MockSystemHost(os_name=self.os_name, os_version=self.os_version)
        host.executive = MockExecutive(should_throw=True)
        port = self.make_port(host=host)
        port._executive = host.executive  # AndroidPortTest.make_port sets its own executive, so reset that as well.

        # This should raise a ScriptError that gets caught and turned into the
        # error text, and also mark wdiff as not available.
        self.make_wdiff_available(port)
        self.assertTrue(port.wdiff_available())
        diff_txt = port.wdiff_text("/tmp/foo.html", "/tmp/bar.html")
        self.assertEqual(diff_txt, port._wdiff_error_html)
        self.assertFalse(port.wdiff_available())

    def test_test_configuration(self):
        port = self.make_port()
        self.assertTrue(port.test_configuration())

    def test_all_test_configurations(self):
        port = self.make_port()
        self.assertTrue(len(port.all_test_configurations()) > 0)
        self.assertTrue(port.test_configuration() in port.all_test_configurations(), "%s not in %s" % (port.test_configuration(), port.all_test_configurations()))

    def test_get_crash_log(self):
        port = self.make_port()
        self.assertEqual(port._get_crash_log(None, None, None, None, newer_than=None),
           (None,
            'crash log for <unknown process name> (pid <unknown>):\n'
            'STDOUT: <empty>\n'
            'STDERR: <empty>\n'))

        self.assertEqual(port._get_crash_log('foo', 1234, 'out bar\nout baz', 'err bar\nerr baz\n', newer_than=None),
            ('err bar\nerr baz\n',
             'crash log for foo (pid 1234):\n'
             'STDOUT: out bar\n'
             'STDOUT: out baz\n'
             'STDERR: err bar\n'
             'STDERR: err baz\n'))

        self.assertEqual(port._get_crash_log('foo', 1234, 'foo\xa6bar', 'foo\xa6bar', newer_than=None),
            ('foo\xa6bar',
             u'crash log for foo (pid 1234):\n'
             u'STDOUT: foo\ufffdbar\n'
             u'STDERR: foo\ufffdbar\n'))

        self.assertEqual(port._get_crash_log('foo', 1234, 'foo\xa6bar', 'foo\xa6bar', newer_than=1.0),
            ('foo\xa6bar',
             u'crash log for foo (pid 1234):\n'
             u'STDOUT: foo\ufffdbar\n'
             u'STDERR: foo\ufffdbar\n'))

    def assert_build_path(self, options, dirs, expected_path):
        port = self.make_port(options=options)
        for directory in dirs:
            port.host.filesystem.maybe_make_directory(directory)
        self.assertEqual(port._build_path(), expected_path)

    def test_expectations_ordering(self):
        port = self.make_port()
        for path in port.expectations_files():
            port._filesystem.write_text_file(path, '')
        ordered_dict = port.expectations_dict()
        self.assertEqual(port.path_to_generic_test_expectations_file(), ordered_dict.keys()[0])

        options = MockOptions(additional_expectations=['/tmp/foo', '/tmp/bar'])
        port = self.make_port(options=options)
        for path in port.expectations_files():
            port._filesystem.write_text_file(path, '')
        port._filesystem.write_text_file('/tmp/foo', 'foo')
        port._filesystem.write_text_file('/tmp/bar', 'bar')
        ordered_dict = port.expectations_dict()
        self.assertEqual(ordered_dict.keys()[-2:], options.additional_expectations)  # pylint: disable=E1101
        self.assertEqual(ordered_dict.values()[-2:], ['foo', 'bar'])

    def test_skipped_directories_for_symbols(self):
        # This first test confirms that the commonly found symbols result in the expected skipped directories.
        symbols_string = " ".join(["fooSymbol"])
        expected_directories = set([
            "webaudio/codec-tests/mp3",
            "webaudio/codec-tests/aac",
        ])

        result_directories = set(TestWebKitPort(symbols_string=symbols_string)._skipped_tests_for_unsupported_features(test_list=['webaudio/codec-tests/mp3/foo.html']))
        self.assertEqual(result_directories, expected_directories)

        # Test that the nm string parsing actually works:
        symbols_string = """
000000000124f498 s __ZZN7WebCore13ff_mp3_decoder12replaceChildEPS0_S1_E19__PRETTY_FUNCTION__
000000000124f500 s __ZZN7WebCore13ff_mp3_decoder13addChildAboveEPS0_S1_E19__PRETTY_FUNCTION__
000000000124f670 s __ZZN7WebCore13ff_mp3_decoder13addChildBelowEPS0_S1_E19__PRETTY_FUNCTION__
"""
        # Note 'compositing' is not in the list of skipped directories (hence the parsing of GraphicsLayer worked):
        expected_directories = set([
            "webaudio/codec-tests/aac",
        ])
        result_directories = set(TestWebKitPort(symbols_string=symbols_string)._skipped_tests_for_unsupported_features(test_list=['webaudio/codec-tests/mp3/foo.html']))
        self.assertEqual(result_directories, expected_directories)

    def test_expectations_files(self):
        port = TestWebKitPort()

        def platform_dirs(port):
            return [port.host.filesystem.basename(port.host.filesystem.dirname(f)) for f in port.expectations_files()]

        self.assertEqual(platform_dirs(port), ['LayoutTests', 'testwebkitport'])

        port = TestWebKitPort(port_name="testwebkitport-version")
        self.assertEqual(platform_dirs(port), ['LayoutTests', 'testwebkitport', 'testwebkitport-version'])

        port = TestWebKitPort(port_name="testwebkitport-version",
                              options=MockOptions(additional_platform_directory=["internal-testwebkitport"]))
        self.assertEqual(platform_dirs(port), ['LayoutTests', 'testwebkitport', 'testwebkitport-version', 'internal-testwebkitport'])

    def test_test_expectations(self):
        # Check that we read the expectations file
        host = MockSystemHost()
        host.filesystem.write_text_file('/mock-checkout/LayoutTests/platform/testwebkitport/TestExpectations',
            'BUG_TESTEXPECTATIONS SKIP : fast/html/article-element.html = FAIL\n')
        port = TestWebKitPort(host=host)
        self.assertEqual(''.join(port.expectations_dict().values()), 'BUG_TESTEXPECTATIONS SKIP : fast/html/article-element.html = FAIL\n')

    def _assert_config_file_for_platform(self, port, platform, config_file):
        self.assertEqual(port._apache_config_file_name_for_platform(platform), config_file)

    def test_linux_distro_detection(self):
        port = TestWebKitPort()
        self.assertFalse(port._is_redhat_based())
        self.assertFalse(port._is_debian_based())

        port._filesystem = MockFileSystem({'/etc/redhat-release': ''})
        self.assertTrue(port._is_redhat_based())
        self.assertFalse(port._is_debian_based())

        port._filesystem = MockFileSystem({'/etc/debian_version': ''})
        self.assertFalse(port._is_redhat_based())
        self.assertTrue(port._is_debian_based())

    def test_apache_config_file_name_for_platform(self):
        port = TestWebKitPort()
        self._assert_config_file_for_platform(port, 'cygwin', 'cygwin-httpd.conf')

        self._assert_config_file_for_platform(port, 'linux2', 'apache2-httpd.conf')
        self._assert_config_file_for_platform(port, 'linux3', 'apache2-httpd.conf')

        port._is_redhat_based = lambda: True
        port._apache_version = lambda: '2.2'
        self._assert_config_file_for_platform(port, 'linux2', 'fedora-httpd-2.2.conf')

        port = TestWebKitPort()
        port._is_debian_based = lambda: True
        self._assert_config_file_for_platform(port, 'linux2', 'apache2-debian-httpd.conf')

        self._assert_config_file_for_platform(port, 'mac', 'apache2-httpd.conf')
        self._assert_config_file_for_platform(port, 'win32', 'apache2-httpd.conf')  # win32 isn't a supported sys.platform.  AppleWin/WinCairo/WinCE ports all use cygwin.
        self._assert_config_file_for_platform(port, 'barf', 'apache2-httpd.conf')

    def test_path_to_apache_config_file(self):
        port = TestWebKitPort()

        saved_environ = os.environ.copy()
        try:
            os.environ['WEBKIT_HTTP_SERVER_CONF_PATH'] = '/path/to/httpd.conf'
            self.assertRaises(IOError, port._path_to_apache_config_file)
            port._filesystem.write_text_file('/existing/httpd.conf', 'Hello, world!')
            os.environ['WEBKIT_HTTP_SERVER_CONF_PATH'] = '/existing/httpd.conf'
            self.assertEqual(port._path_to_apache_config_file(), '/existing/httpd.conf')
        finally:
            os.environ = saved_environ.copy()

        # Mock out _apache_config_file_name_for_platform to ignore the passed sys.platform value.
        port._apache_config_file_name_for_platform = lambda platform: 'httpd.conf'
        self.assertEqual(port._path_to_apache_config_file(), '/mock-checkout/LayoutTests/http/conf/httpd.conf')

        # Check that even if we mock out _apache_config_file_name, the environment variable takes precedence.
        saved_environ = os.environ.copy()
        try:
            os.environ['WEBKIT_HTTP_SERVER_CONF_PATH'] = '/existing/httpd.conf'
            self.assertEqual(port._path_to_apache_config_file(), '/existing/httpd.conf')
        finally:
            os.environ = saved_environ.copy()

    def test_additional_platform_directory(self):
        port = self.make_port(options=MockOptions(additional_platform_directory=['/tmp/foo']))
        self.assertEqual(port.baseline_search_path()[0], '/tmp/foo')
