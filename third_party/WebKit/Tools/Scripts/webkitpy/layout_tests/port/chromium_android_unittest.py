# Copyright (C) 2012 Google Inc. All rights reserved.
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

import optparse
import StringIO
import time
import unittest2 as unittest
import sys

from webkitpy.common.system import executive_mock
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.systemhost_mock import MockSystemHost

from webkitpy.layout_tests.port import chromium_android
from webkitpy.layout_tests.port import chromium_port_testcase
from webkitpy.layout_tests.port import driver
from webkitpy.layout_tests.port import driver_unittest
from webkitpy.tool.mocktool import MockOptions

# Any "adb" commands will be interpret by this class instead of executing actual
# commansd on the file system, which we don't want to do.
class MockAndroidDebugBridge:
    def __init__(self, device_count):
        self._device_count = device_count
        self._last_command = None

    # Local public methods.

    def run_command(self, args):
        self._last_command = ' '.join(args)
        if args[0].startswith('path'):
            if args[0] == 'path1':
                return ''
            if args[0] == 'path2':
                return 'version 1.1'

            return 'version 1.0'

        if args[0] == 'adb':
            if len(args) > 1 and args[1] == 'version':
                return 'version 1.0'
            if len(args) > 1 and args[1] == 'devices':
                return self._get_device_output()
            if len(args) > 3 and args[3] == 'command':
                return 'mockoutput'

        return ''

    def last_command(self):
        return self._last_command

    # Local private methods.

    def _get_device_output(self):
        serials = ['123456789ABCDEF0', '123456789ABCDEF1', '123456789ABCDEF2',
                   '123456789ABCDEF3', '123456789ABCDEF4', '123456789ABCDEF5']
        output = 'List of devices attached\n'
        for serial in serials[:self._device_count]:
          output += '%s\tdevice\n' % serial
        return output


class AndroidCommandsTest(unittest.TestCase):
    def setUp(self):
        chromium_android.AndroidCommands._adb_command_path = None
        chromium_android.AndroidCommands._adb_command_path_options = ['adb']

    def make_executive(self, device_count):
        self._mock_executive = MockAndroidDebugBridge(device_count)
        return MockExecutive2(run_command_fn=self._mock_executive.run_command)

    def make_android_commands(self, device_count, serial):
        return chromium_android.AndroidCommands(self.make_executive(device_count), serial)

    # The "adb" binary with the latest version should be used.
    def serial_test_adb_command_path(self):
        executive = self.make_executive(0)

        chromium_android.AndroidCommands.set_adb_command_path_options(['path1', 'path2', 'path3'])
        self.assertEqual('path2', chromium_android.AndroidCommands.adb_command_path(executive))

    # The get_devices() method should throw if there aren't any devices. Otherwise it returns an array.
    def test_get_devices(self):
        self.assertRaises(AssertionError, chromium_android.AndroidCommands.get_devices, self.make_executive(0))
        self.assertEquals(1, len(chromium_android.AndroidCommands.get_devices(self.make_executive(1))))
        self.assertEquals(5, len(chromium_android.AndroidCommands.get_devices(self.make_executive(5))))

    # The used adb command should include the device's serial number, and get_serial() should reflect this.
    def test_adb_command_and_get_serial(self):
        android_commands = self.make_android_commands(1, '123456789ABCDEF0')
        self.assertEquals(['adb', '-s', '123456789ABCDEF0'], android_commands.adb_command())
        self.assertEquals('123456789ABCDEF0', android_commands.get_serial())

    # Running an adb command should return the command's output.
    def test_run_command(self):
        android_commands = self.make_android_commands(1, '123456789ABCDEF0')

        output = android_commands.run(['command'])
        self.assertEquals('adb -s 123456789ABCDEF0 command', self._mock_executive.last_command())
        self.assertEquals('mockoutput', output)

    # Test that the convenience methods create the expected commands.
    def test_convenience_methods(self):
        android_commands = self.make_android_commands(1, '123456789ABCDEF0')

        android_commands.file_exists('/tombstones')
        self.assertEquals('adb -s 123456789ABCDEF0 shell ls /tombstones', self._mock_executive.last_command())

        android_commands.push('foo', 'bar')
        self.assertEquals('adb -s 123456789ABCDEF0 push foo bar', self._mock_executive.last_command())

        android_commands.pull('bar', 'foo')
        self.assertEquals('adb -s 123456789ABCDEF0 pull bar foo', self._mock_executive.last_command())


class ChromiumAndroidPortTest(chromium_port_testcase.ChromiumPortTestCase):
    port_name = 'chromium-android'
    port_maker = chromium_android.ChromiumAndroidPort

    def make_port(self, **kwargs):
        port = super(ChromiumAndroidPortTest, self).make_port(**kwargs)
        port._mock_adb = MockAndroidDebugBridge(kwargs.get('device_count', 1))
        port._executive = MockExecutive2(run_command_fn=port._mock_adb.run_command)
        return port

    # Test that the right driver details will be set for DumpRenderTree vs. content_shell
    def test_driver_details(self):
        port_dump_render_tree = self.make_port()
        port_content_shell = self.make_port(options=optparse.Values({'driver_name': 'content_shell'}))

        self.assertIsInstance(port_dump_render_tree._driver_details, chromium_android.DumpRenderTreeDriverDetails)
        self.assertIsInstance(port_content_shell._driver_details, chromium_android.ContentShellDriverDetails)

    # Test that the number of child processes to create depends on the devices.
    def test_default_child_processes(self):
        port_default = self.make_port(device_count=5)
        port_fixed_device = self.make_port(device_count=5, options=optparse.Values({'adb_device': '123456789ABCDEF9'}))

        self.assertEquals(5, port_default.default_child_processes())
        self.assertEquals(1, port_fixed_device.default_child_processes())

    # Test that an HTTP server indeed is required by Android (as we serve all tests over them)
    def test_requires_http_server(self):
        self.assertTrue(self.make_port(device_count=1).requires_http_server())

    # Disable this test because Android introduces its own TestExpectations file.
    def test_expectations_files(self):
        pass

    # Test that Chromium Android still uses a port-specific TestExpectations file.
    def test_uses_android_specific_test_expectations(self):
        port = self.make_port()

        android_count = len(port._port_specific_expectations_files())
        chromium_count = len(super(chromium_android.ChromiumAndroidPort, port)._port_specific_expectations_files())

        self.assertEquals(android_count - 1, chromium_count)

    # Tests the default timeouts for Android, which are different than the rest of Chromium.
    def test_default_timeout_ms(self):
        self.assertEqual(self.make_port(options=optparse.Values({'configuration': 'Release'})).default_timeout_ms(), 10000)
        self.assertEqual(self.make_port(options=optparse.Values({'configuration': 'Debug'})).default_timeout_ms(), 10000)


class ChromiumAndroidDriverTest(unittest.TestCase):
    def setUp(self):
        self._mock_adb = MockAndroidDebugBridge(1)
        self._mock_executive = MockExecutive2(run_command_fn=self._mock_adb.run_command)
        self._port = chromium_android.ChromiumAndroidPort(MockSystemHost(executive=self._mock_executive), 'chromium-android')
        self._driver = chromium_android.ChromiumAndroidDriver(self._port, worker_number=0,
            pixel_tests=True, driver_details=chromium_android.ContentShellDriverDetails())

    # The cmd_line() method in the Android port is used for starting a shell, not the test runner.
    def test_cmd_line(self):
        self.assertEquals(['adb', '-s', '123456789ABCDEF0', 'shell'], self._driver.cmd_line(False, []))

    # Test that the Chromium Android port can interpret Android's shell output.
    def test_read_prompt(self):
        self._driver._server_process = driver_unittest.MockServerProcess(lines=['root@android:/ # '])
        self.assertIsNone(self._driver._read_prompt(time.time() + 1))
        self._driver._server_process = driver_unittest.MockServerProcess(lines=['$ '])
        self.assertIsNone(self._driver._read_prompt(time.time() + 1))


class ChromiumAndroidDriverTwoDriverTest(unittest.TestCase):
    # Test two drivers getting the right serial numbers, and that we disregard per-test arguments.
    def test_two_drivers(self):
        mock_adb = MockAndroidDebugBridge(2)
        mock_executive = MockExecutive2(run_command_fn=mock_adb.run_command)

        port = chromium_android.ChromiumAndroidPort(MockSystemHost(executive=mock_executive), 'chromium-android')
        driver0 = chromium_android.ChromiumAndroidDriver(port, worker_number=0, pixel_tests=True,
            driver_details=chromium_android.DumpRenderTreeDriverDetails())
        driver1 = chromium_android.ChromiumAndroidDriver(port, worker_number=1, pixel_tests=True,
            driver_details=chromium_android.DumpRenderTreeDriverDetails())

        self.assertEqual(['adb', '-s', '123456789ABCDEF0', 'shell'], driver0.cmd_line(True, []))
        self.assertEqual(['adb', '-s', '123456789ABCDEF1', 'shell'], driver1.cmd_line(True, ['anything']))


class ChromiumAndroidTwoPortsTest(unittest.TestCase):
    # Test that the driver's command line indeed goes through to the driver.
    def test_options_with_two_ports(self):
        mock_adb = MockAndroidDebugBridge(2)
        mock_executive = MockExecutive2(run_command_fn=mock_adb.run_command)

        port0 = chromium_android.ChromiumAndroidPort(MockSystemHost(executive=mock_executive),
            'chromium-android', options=MockOptions(additional_drt_flag=['--foo=bar']))
        port1 = chromium_android.ChromiumAndroidPort(MockSystemHost(executive=mock_executive),
            'chromium-android', options=MockOptions(driver_name='content_shell'))

        self.assertEqual(1, port0.driver_cmd_line().count('--foo=bar'))
        self.assertEqual(0, port1.driver_cmd_line().count('--create-stdin-fifo'))


class ChromiumAndroidDriverTwoDriversTest(unittest.TestCase):
    # Test two drivers getting the right serial numbers, and that we disregard per-test arguments.
    def test_two_drivers(self):
        mock_adb = MockAndroidDebugBridge(2)
        mock_executive = MockExecutive2(run_command_fn=mock_adb.run_command)

        port = chromium_android.ChromiumAndroidPort(MockSystemHost(executive=mock_executive), 'chromium-android')
        driver0 = chromium_android.ChromiumAndroidDriver(port, worker_number=0, pixel_tests=True,
            driver_details=chromium_android.ContentShellDriverDetails())
        driver1 = chromium_android.ChromiumAndroidDriver(port, worker_number=1, pixel_tests=True,
            driver_details=chromium_android.ContentShellDriverDetails())

        self.assertEqual(['adb', '-s', '123456789ABCDEF0', 'shell'], driver0.cmd_line(True, []))
        self.assertEqual(['adb', '-s', '123456789ABCDEF1', 'shell'], driver1.cmd_line(True, []))


class ChromiumAndroidTwoPortsTest(unittest.TestCase):
    # Test that the driver's command line indeed goes through to the driver.
    def test_options_with_two_ports(self):
        mock_adb = MockAndroidDebugBridge(2)
        mock_executive = MockExecutive2(run_command_fn=mock_adb.run_command)

        port0 = chromium_android.ChromiumAndroidPort(MockSystemHost(executive=mock_executive),
            'chromium-android', options=MockOptions(additional_drt_flag=['--foo=bar']))
        port1 = chromium_android.ChromiumAndroidPort(MockSystemHost(executive=mock_executive),
            'chromium-android', options=MockOptions(driver_name='content_shell'))

        self.assertEqual(1, port0.driver_cmd_line().count('--foo=bar'))
        self.assertEqual(0, port1.driver_cmd_line().count('--create-stdin-fifo'))
