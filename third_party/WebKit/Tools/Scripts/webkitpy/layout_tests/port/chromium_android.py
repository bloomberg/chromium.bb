# Copyright (C) 2012 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
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

import copy
import logging
import os
import re
import sys
import subprocess
import threading
import time

from webkitpy.layout_tests.port import chromium
from webkitpy.layout_tests.port import chromium_linux
from webkitpy.layout_tests.port import driver
from webkitpy.layout_tests.port import factory
from webkitpy.layout_tests.port import server_process
from webkitpy.common.system.profiler import SingleFileOutputProfiler

_log = logging.getLogger(__name__)

# The root directory for test resources, which has the same structure as the
# source root directory of Chromium.
# This path is defined in Chromium's base/test/test_support_android.cc.
DEVICE_SOURCE_ROOT_DIR = '/data/local/tmp/'

# The layout tests directory on device, which has two usages:
# 1. as a virtual path in file urls that will be bridged to HTTP.
# 2. pointing to some files that are pushed to the device for tests that
# don't work on file-over-http (e.g. blob protocol tests).
DEVICE_WEBKIT_BASE_DIR = DEVICE_SOURCE_ROOT_DIR + 'third_party/WebKit/'
DEVICE_LAYOUT_TESTS_DIR = DEVICE_WEBKIT_BASE_DIR + 'LayoutTests/'

SCALING_GOVERNORS_PATTERN = "/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor"
KPTR_RESTRICT_PATH = "/proc/sys/kernel/kptr_restrict"

# All the test cases are still served to the test runner through file protocol,
# but we use a file-to-http feature to bridge the file request to host's http
# server to get the real test files and corresponding resources.
# See webkit/support/platform_support_android.cc for the other side of this bridge.
PERF_TEST_PATH_PREFIX = '/all-perf-tests'
LAYOUT_TEST_PATH_PREFIX = '/all-tests'

# All ports the Android forwarder to forward.
# 8000, 8080 and 8443 are for http/https tests.
# 8880 and 9323 are for websocket tests
# (see http_server.py, apache_http_server.py and websocket_server.py).
FORWARD_PORTS = '8000 8080 8443 8880 9323'

MS_TRUETYPE_FONTS_DIR = '/usr/share/fonts/truetype/msttcorefonts/'
MS_TRUETYPE_FONTS_PACKAGE = 'ttf-mscorefonts-installer'

# Timeout in seconds to wait for starting/stopping the driver.
DRIVER_START_STOP_TIMEOUT_SECS = 10

HOST_FONT_FILES = [
    [[MS_TRUETYPE_FONTS_DIR], 'Arial.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Arial_Bold.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Arial_Bold_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Arial_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Comic_Sans_MS.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Comic_Sans_MS_Bold.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Courier_New.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Courier_New_Bold.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Courier_New_Bold_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Courier_New_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Georgia.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Georgia_Bold.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Georgia_Bold_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Georgia_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Impact.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Trebuchet_MS.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Trebuchet_MS_Bold.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Trebuchet_MS_Bold_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Trebuchet_MS_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Times_New_Roman.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Times_New_Roman_Bold.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Times_New_Roman_Bold_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Times_New_Roman_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Verdana.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Verdana_Bold.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Verdana_Bold_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    [[MS_TRUETYPE_FONTS_DIR], 'Verdana_Italic.ttf', MS_TRUETYPE_FONTS_PACKAGE],
    # The Microsoft font EULA
    [['/usr/share/doc/ttf-mscorefonts-installer/'], 'READ_ME!.gz', MS_TRUETYPE_FONTS_PACKAGE],
    # Other fonts: Arabic, CJK, Indic, Thai, etc.
    [['/usr/share/fonts/truetype/ttf-dejavu/'], 'DejaVuSans.ttf', 'ttf-dejavu'],
    [['/usr/share/fonts/truetype/kochi/'], 'kochi-mincho.ttf', 'ttf-kochi-mincho'],
    [['/usr/share/fonts/truetype/ttf-indic-fonts-core/'], 'lohit_hi.ttf', 'ttf-indic-fonts-core'],
    [['/usr/share/fonts/truetype/ttf-indic-fonts-core/'], 'lohit_ta.ttf', 'ttf-indic-fonts-core'],
    [['/usr/share/fonts/truetype/ttf-indic-fonts-core/'], 'MuktiNarrow.ttf', 'ttf-indic-fonts-core'],
    [['/usr/share/fonts/truetype/thai/', '/usr/share/fonts/truetype/tlwg/'], 'Garuda.ttf', 'fonts-tlwg-garuda'],
    [['/usr/share/fonts/truetype/ttf-indic-fonts-core/', '/usr/share/fonts/truetype/ttf-punjabi-fonts/'], 'lohit_pa.ttf', 'ttf-indic-fonts-core'],
]

# Test resources that need to be accessed as files directly.
# Each item can be the relative path of a directory or a file.
TEST_RESOURCES_TO_PUSH = [
    # Blob tests need to access files directly.
    'editing/pasteboard/resources',
    'fast/files/resources',
    'http/tests/local/resources',
    'http/tests/local/formdata/resources',
    # User style URLs are accessed as local files in webkit_support.
    'http/tests/security/resources/cssStyle.css',
    # Media tests need to access audio/video as files.
    'media/content',
    'compositing/resources/video.mp4',
]

# List of test resources from webkit platform. These resources will be copied to the external storage.
WEBKIT_PLATFORM_RESOURCES_TO_PUSH = [
    'third_party/hyphen/hyph_en_US.dic',
]

MD5SUM_DEVICE_FILE_NAME = 'md5sum_bin'
MD5SUM_DEVICE_PATH = '/data/local/tmp/' + MD5SUM_DEVICE_FILE_NAME

# Shared pieces of information for the two supported test runners.
class SharedDriverDetails(object):
    def device_cache_directory(self):
        return self.device_directory() + 'cache/'
    def device_fonts_directory(self):
        return self.device_directory() + 'fonts/'
    def device_forwarder_path(self):
        return self.device_directory() + 'forwarder'
    def device_fifo_directory(self):
        return '/data/data/' + self.package_name() + '/files/'

# Information required when running layout tests using DumpRenderTree as the test runner.
class DumpRenderTreeDriverDetails(SharedDriverDetails):
    def apk_name(self):
        return 'DumpRenderTree_apk/DumpRenderTree-debug.apk'
    def package_name(self):
        return 'org.chromium.native_test'
    def activity_name(self):
        return self.package_name() + '/.ChromeNativeTestActivity'
    def library_name(self):
        return 'libDumpRenderTree.so'
    def additional_resources(self):
        return ['DumpRenderTree.pak', 'DumpRenderTree_resources']
    def command_line_file(self):
        return '/data/local/tmp/chrome-native-tests-command-line'
    def additional_command_line_flags(self):
        return ['--create-stdin-fifo', '--separate-stderr-fifo', '--disable-impl-side-painting']
    def device_directory(self):
        return DEVICE_SOURCE_ROOT_DIR + 'drt/'

# Information required when running layout tests using content_shell as the test runner.
class ContentShellDriverDetails(SharedDriverDetails):
    def apk_name(self):
        return 'apks/ContentShell.apk'
    def package_name(self):
        return 'org.chromium.content_shell_apk'
    def activity_name(self):
        return self.package_name() + '/.ContentShellActivity'
    def library_name(self):
        return 'libcontent_shell_content_view.so'
    def additional_resources(self):
        return ['content_resources.pak', 'shell_resources.pak']
    def command_line_file(self):
        return '/data/local/tmp/content-shell-command-line'
    def additional_command_line_flags(self):
        return []
    def device_directory(self):
        return DEVICE_SOURCE_ROOT_DIR + 'content_shell/'

# The AndroidCommands class encapsulates commands to communicate with an attached device.
class AndroidCommands(object):
    _adb_command_path = None
    _adb_command_path_options = []

    def __init__(self, executive, device_serial):
        self._executive = executive
        self._device_serial = device_serial

    # Local public methods.

    def file_exists(self, full_path):
        assert full_path.startswith('/')
        return self.run(['shell', 'ls', full_path]).strip() == full_path

    def push(self, host_path, device_path, ignore_error=False):
        return self.run(['push', host_path, device_path], ignore_error=ignore_error)

    def pull(self, device_path, host_path, ignore_error=False):
        return self.run(['pull', device_path, host_path], ignore_error=ignore_error)

    def restart_as_root(self):
        output = self.run(['root'])
        if 'adbd is already running as root' in output:
            return

        elif not 'restarting adbd as root' in output:
            self._log_error('Unrecognized output from adb root: %s' % output)

        self.run(['wait-for-device'])

    def run(self, command, ignore_error=False):
        self._log_debug('Run adb command: ' + str(command))
        if ignore_error:
            error_handler = self._executive.ignore_error
        else:
            error_handler = None

        result = self._executive.run_command(self.adb_command() + command,
                error_handler=error_handler)

        # We limit the length to avoid outputting too verbose commands, such as "adb logcat".
        self._log_debug('Run adb result: ' + result[:80])
        return result

    def get_serial(self):
        return self._device_serial

    def adb_command(self):
        return [AndroidCommands.adb_command_path(self._executive), '-s', self._device_serial]

    @staticmethod
    def set_adb_command_path_options(paths):
        AndroidCommands._adb_command_path_options = paths

    @staticmethod
    def adb_command_path(executive):
        if AndroidCommands._adb_command_path:
            return AndroidCommands._adb_command_path

        assert AndroidCommands._adb_command_path_options, 'No commands paths have been set to look for the "adb" command.'

        command_path = None
        command_version = None
        for path_option in AndroidCommands._adb_command_path_options:
            path_version = AndroidCommands._determine_adb_version(path_option, executive)
            if not path_version:
                continue
            if command_version != None and path_version < command_version:
                continue

            command_path = path_option
            command_version = path_version

        assert command_path, 'Unable to locate the "adb" command. Are you using an Android checkout of Chromium?'

        AndroidCommands._adb_command_path = command_path
        return command_path

    @staticmethod
    def get_devices(executive):
        re_device = re.compile('^([a-zA-Z0-9_:.-]+)\tdevice$', re.MULTILINE)
        result = executive.run_command([AndroidCommands.adb_command_path(executive), 'devices'],
                error_handler=executive.ignore_error)
        devices = re_device.findall(result)
        if not devices:
            raise AssertionError('No devices attached. Result of "adb devices": %s' % result)

        return devices

    # Local private methods.

    def _log_error(self, message):
        _log.error('[%s] %s' % (self._device_serial, message))

    def _log_debug(self, message):
        _log.debug('[%s] %s' % (self._device_serial, message))

    @staticmethod
    def _determine_adb_version(adb_command_path, executive):
        re_version = re.compile('^.*version ([\d\.]+)$')
        try:
            output = executive.run_command([adb_command_path, 'version'], error_handler=executive.ignore_error)
        except OSError:
            return None

        result = re_version.match(output)
        if not output or not result:
            return None

        return [int(n) for n in result.group(1).split('.')]

class ChromiumAndroidPort(chromium.ChromiumPort):
    port_name = 'chromium-android'

    # Avoid initializing the adb path [worker count]+1 times by storing it as a static member.
    _adb_path = None

    SUPPORTED_VERSIONS = ('android')

    FALLBACK_PATHS = { 'android': [ 'chromium-android' ] + chromium_linux.ChromiumLinuxPort.latest_platform_fallback_path() }

    def __init__(self, host, port_name, **kwargs):
        super(ChromiumAndroidPort, self).__init__(host, port_name, **kwargs)

        self._operating_system = 'android'
        self._version = 'icecreamsandwich'

        self._host_port = factory.PortFactory(host).get('chromium', **kwargs)
        self._server_process_constructor = self._android_server_process_constructor

        if self.driver_name() == 'content_shell':
            self._driver_details = ContentShellDriverDetails()
        else:
            self._driver_details = DumpRenderTreeDriverDetails()

        if hasattr(self._options, 'adb_device') and len(self._options.adb_device):
            self._devices = [self._options.adb_device]
        else:
            self._devices = []

        AndroidCommands.set_adb_command_path_options(['adb',
            self.path_from_chromium_base('third_party', 'android_tools', 'sdk', 'platform-tools', 'adb')])

    # Local public methods.
    def get_device_serial(self, worker_number):
        if not self._devices:
            self._devices = AndroidCommands.get_devices(self._executive)
        if worker_number >= len(self._devices):
            raise AssertionError('Worker number exceeds available number of devices')
        return self._devices[worker_number]

    def path_to_forwarder(self):
        return self._build_path('forwarder')

    def path_to_md5sum(self):
        return self._build_path(MD5SUM_DEVICE_FILE_NAME)

    # Overridden public methods.
    def buildbot_archives_baselines(self):
        return False

    def additional_drt_flag(self):
        # Chromium for Android always uses the hardware GPU path.
        return ['--encode-binary', '--enable-hardware-gpu',
                '--force-compositing-mode',
                '--enable-accelerated-fixed-position']

    def default_timeout_ms(self):
        # Android platform has less computing power than desktop platforms.
        # Using 10 seconds allows us to pass most slow tests which are not
        # marked as slow tests on desktop platforms.
        return 10 * 1000

    def driver_stop_timeout(self):
        # The driver doesn't respond to closing stdin, so we might as well stop the driver immediately.
        return 0.0

    def driver_name(self):
        if self.get_option('driver_name'):
            return self.get_option('driver_name')
        if self.get_option('content_shell'):
            return self.CONTENT_SHELL_NAME
        return 'DumpRenderTree'

    def default_child_processes(self):
        if self._devices:
            return len(self._devices)

        return len(AndroidCommands.get_devices(self._executive))

    def default_baseline_search_path(self):
        return map(self._webkit_baseline_path, self.FALLBACK_PATHS['android'])

    def check_wdiff(self, logging=True):
        return self._host_port.check_wdiff(logging)

    def check_build(self, needs_http):
        result = super(ChromiumAndroidPort, self).check_build(needs_http)
        result = self._check_file_exists(self.path_to_md5sum(), 'md5sum utility') and result
        result = self._check_file_exists(self.path_to_forwarder(), 'forwarder utility') and result
        if not result:
            _log.error('For complete Android build requirements, please see:')
            _log.error('')
            _log.error('    http://code.google.com/p/chromium/wiki/AndroidBuildInstructions')

        return result

    def check_sys_deps(self, needs_http):
        for (font_dirs, font_file, package) in HOST_FONT_FILES:
            exists = False
            for font_dir in font_dirs:
                font_path = font_dir + font_file
                if self._check_file_exists(font_path, '', logging=False):
                    exists = True
                    break
            if not exists:
                _log.error('You are missing %s under %s. Try installing %s. See build instructions.' % (font_file, font_dirs, package))
                return False
        return True

    def requires_http_server(self):
        """Chromium Android runs tests on devices, and uses the HTTP server to
        serve the actual layout tests to the test driver."""
        return True

    def start_http_server(self, additional_dirs=None, number_of_servers=0):
        if not additional_dirs:
            additional_dirs = {}
        additional_dirs[PERF_TEST_PATH_PREFIX] = self.perf_tests_dir()
        additional_dirs[LAYOUT_TEST_PATH_PREFIX] = self.layout_tests_dir()
        super(ChromiumAndroidPort, self).start_http_server(additional_dirs, number_of_servers)

    def create_driver(self, worker_number, no_timeout=False):
        return ChromiumAndroidDriver(self, worker_number, pixel_tests=self.get_option('pixel_tests'), driver_details=self._driver_details,
                                     # Force no timeout to avoid test driver timeouts before NRWT.
                                     no_timeout=True)

    def driver_cmd_line(self):
        # Override to return the actual test driver's command line.
        return self.create_driver(0)._android_driver_cmd_line(self.get_option('pixel_tests'), [])

    # Overridden protected methods.

    def _port_specific_expectations_files(self):
        # LayoutTests/platform/chromium-android/TestExpectations should contain only the rules to
        # skip tests for the features not supported or not testable on Android.
        # Other rules should be in LayoutTests/TestExpectations.
        android_expectations_file = self.path_from_webkit_base('LayoutTests', 'platform', 'chromium-android', 'TestExpectations')
        return super(ChromiumAndroidPort, self)._port_specific_expectations_files() + [android_expectations_file]


    def _build_path(self, *comps):
        return self._host_port._build_path(*comps)

    def _build_path_with_configuration(self, configuration, *comps):
        return self._host_port._build_path_with_configuration(configuration, *comps)

    def _path_to_apache(self):
        return self._host_port._path_to_apache()

    def _path_to_apache_config_file(self):
        return self._host_port._path_to_apache_config_file()

    def _path_to_driver(self, configuration=None):
        return self._build_path_with_configuration(configuration, self._driver_details.apk_name())

    def _path_to_helper(self):
        return None

    def _path_to_image_diff(self):
        return self._host_port._path_to_image_diff()

    def _path_to_lighttpd(self):
        return self._host_port._path_to_lighttpd()

    def _path_to_lighttpd_modules(self):
        return self._host_port._path_to_lighttpd_modules()

    def _path_to_lighttpd_php(self):
        return self._host_port._path_to_lighttpd_php()

    def _path_to_wdiff(self):
        return self._host_port._path_to_wdiff()

    def _shut_down_http_server(self, pid):
        return self._host_port._shut_down_http_server(pid)

    def _driver_class(self):
        return ChromiumAndroidDriver

    # Local private methods.

    @staticmethod
    def _android_server_process_constructor(port, server_name, cmd_line, env=None):
        return server_process.ServerProcess(port, server_name, cmd_line, env,
                                            universal_newlines=True, treat_no_data_as_crash=True)


class AndroidPerf(SingleFileOutputProfiler):
    _cached_perf_host_path = None
    _have_searched_for_perf_host = False

    def __init__(self, host, executable_path, output_dir, android_commands, symfs_path, kallsyms_path, identifier=None):
        super(AndroidPerf, self).__init__(host, executable_path, output_dir, "data", identifier)
        self._android_commands = android_commands
        self._perf_process = None
        self._symfs_path = symfs_path
        self._kallsyms_path = kallsyms_path

    def check_configuration(self):
        # Check that perf is installed
        if not self._android_commands.file_exists('/system/bin/perf'):
            print "Cannot find /system/bin/perf on device %s" % self._android_commands.get_serial()
            return False

        # Check that the device is a userdebug build (or at least has the necessary libraries).
        if self._android_commands.run(['shell', 'getprop', 'ro.build.type']).strip() != 'userdebug':
            print "Device %s is not flashed with a userdebug build of Android" % self._android_commands.get_serial()
            return False

        # FIXME: Check that the binary actually is perf-able (has stackframe pointers)?
        # objdump -s a function and make sure it modifies the fp?
        # Instruct users to rebuild after export GYP_DEFINES="profiling=1 $GYP_DEFINES"
        return True

    def print_setup_instructions(self):
        print """
perf on android requires a 'userdebug' build of Android, see:
http://source.android.com/source/building-devices.html"

The perf command can be built from:
https://android.googlesource.com/platform/external/linux-tools-perf/
and requires libefl, libebl, libdw, and libdwfl available in:
https://android.googlesource.com/platform/external/elfutils/

The test driver must be built with profiling=1, make sure you've done:
export GYP_DEFINES="profiling=1 $GYP_DEFINES"
update-webkit --chromium-android
build-webkit --chromium-android

Googlers should read:
http://goto.google.com/cr-android-perf-howto
"""

    def attach_to_pid(self, pid):
        assert(pid)
        assert(self._perf_process == None)
        # FIXME: This can't be a fixed timeout!
        cmd = self._android_commands.adb_command() + ['shell', 'perf', 'record', '-g', '-p', pid, 'sleep', 30]
        self._perf_process = self._host.executive.popen(cmd)

    def _perf_version_string(self, perf_path):
        try:
            return self._host.executive.run_command([perf_path, '--version'])
        except:
            return None

    def _find_perfhost_binary(self):
        perfhost_version = self._perf_version_string('perfhost_linux')
        if perfhost_version:
            return 'perfhost_linux'
        perf_version = self._perf_version_string('perf')
        if perf_version:
            return 'perf'
        return None

    def _perfhost_path(self):
        if self._have_searched_for_perf_host:
            return self._cached_perf_host_path
        self._have_searched_for_perf_host = True
        self._cached_perf_host_path = self._find_perfhost_binary()
        return self._cached_perf_host_path

    def _first_ten_lines_of_profile(self, perf_output):
        match = re.search("^#[^\n]*\n((?: [^\n]*\n){1,10})", perf_output, re.MULTILINE)
        return match.group(1) if match else None

    def profile_after_exit(self):
        perf_exitcode = self._perf_process.wait()
        if perf_exitcode != 0:
            print "Perf failed (exit code: %i), can't process results." % perf_exitcode
            return

        self._android_commands.pull('/data/perf.data', self._output_path)

        perfhost_path = self._perfhost_path()
        perfhost_report_command = [
            'report',
            '--input', self._output_path,
            '--symfs', self._symfs_path,
            '--kallsyms', self._kallsyms_path,
        ]
        if perfhost_path:
            perfhost_args = [perfhost_path] + perfhost_report_command + ['--call-graph', 'none']
            perf_output = self._host.executive.run_command(perfhost_args)
            # We could save off the full -g report to a file if users found that useful.
            print self._first_ten_lines_of_profile(perf_output)
        else:
            print """
Failed to find perfhost_linux binary, can't process samples from the device.

perfhost_linux can be built from:
https://android.googlesource.com/platform/external/linux-tools-perf/
also, modern versions of perf (available from apt-get install goobuntu-kernel-tools-common)
may also be able to process the perf.data files from the device.

Googlers should read:
http://goto.google.com/cr-android-perf-howto
for instructions on installing pre-built copies of perfhost_linux
http://crbug.com/165250 discusses making these pre-built binaries externally available.
"""

        perfhost_display_patch = perfhost_path if perfhost_path else 'perfhost_linux'
        print "To view the full profile, run:"
        print ' '.join([perfhost_display_patch] + perfhost_report_command)


class ChromiumAndroidDriver(driver.Driver):
    def __init__(self, port, worker_number, pixel_tests, driver_details, no_timeout=False):
        super(ChromiumAndroidDriver, self).__init__(port, worker_number, pixel_tests, no_timeout)
        self._in_fifo_path = driver_details.device_fifo_directory() + 'stdin.fifo'
        self._out_fifo_path = driver_details.device_fifo_directory() + 'test.fifo'
        self._err_fifo_path = driver_details.device_fifo_directory() + 'stderr.fifo'
        self._read_stdout_process = None
        self._read_stderr_process = None
        self._forwarder_process = None
        self._has_setup = False
        self._original_governors = {}
        self._original_kptr_restrict = None

        self._android_commands = AndroidCommands(port._executive, port.get_device_serial(worker_number))
        self._driver_details = driver_details

        # FIXME: If we taught ProfileFactory about "target" devices we could
        # just use the logic in Driver instead of duplicating it here.
        if self._port.get_option("profile"):
            # FIXME: This should be done once, instead of per-driver!
            symfs_path = self._find_or_create_symfs()
            kallsyms_path = self._update_kallsyms_cache(symfs_path)
            # FIXME: We should pass this some sort of "Bridge" object abstraction around ADB instead of a path/device pair.
            self._profiler = AndroidPerf(self._port.host, self._port._path_to_driver(), self._port.results_directory(),
                self._android_commands, symfs_path, kallsyms_path)
            # FIXME: This is a layering violation and should be moved to Port.check_sys_deps
            # once we have an abstraction around an adb_path/device_serial pair to make it
            # easy to make these class methods on AndroidPerf.
            if not self._profiler.check_configuration():
                self._profiler.print_setup_instructions()
                sys.exit(1)
        else:
            self._profiler = None

    def __del__(self):
        self._teardown_performance()
        super(ChromiumAndroidDriver, self).__del__()

    def _update_kallsyms_cache(self, output_dir):
        kallsyms_name = "%s-kallsyms" % self._android_commands.get_serial()
        kallsyms_cache_path = self._port.host.filesystem.join(output_dir, kallsyms_name)

        self._android_commands.restart_as_root()

        saved_kptr_restrict = self._android_commands.run(['shell', 'cat', KPTR_RESTRICT_PATH]).strip()
        self._android_commands.run(['shell', 'echo', '0', '>', KPTR_RESTRICT_PATH])

        print "Updating kallsyms file (%s) from device" % kallsyms_cache_path
        self._android_commands.pull("/proc/kallsyms", kallsyms_cache_path)

        self._android_commands.run(['shell', 'echo', saved_kptr_restrict, '>', KPTR_RESTRICT_PATH])

        return kallsyms_cache_path

    def _find_or_create_symfs(self):
        environment = self._port.host.copy_current_environment()
        env = environment.to_dictionary()
        fs = self._port.host.filesystem

        if 'ANDROID_SYMFS' in env:
            symfs_path = env['ANDROID_SYMFS']
        else:
            symfs_path = fs.join(self._port.results_directory(), 'symfs')
            print "ANDROID_SYMFS not set, using %s" % symfs_path

        # find the installed path, and the path of the symboled built library
        # FIXME: We should get the install path from the device!
        symfs_library_path = fs.join(symfs_path, "data/app-lib/%s-1/%s" % (self._driver_details.package_name(), self._driver_details.library_name()))
        built_library_path = self._port._build_path('lib', self._driver_details.library_name())
        assert(fs.exists(built_library_path))

        # FIXME: Ideally we'd check the sha1's first and make a soft-link instead of copying (since we probably never care about windows).
        print "Updating symfs libary (%s) from built copy (%s)" % (symfs_library_path, built_library_path)
        fs.maybe_make_directory(fs.dirname(symfs_library_path))
        fs.copyfile(built_library_path, symfs_library_path)

        return symfs_path

    def _setup_md5sum_and_push_data_if_needed(self):
        self._md5sum_path = self._port.path_to_md5sum()
        if not self._android_commands.file_exists(MD5SUM_DEVICE_PATH):
            if not self._android_commands.push(self._md5sum_path, MD5SUM_DEVICE_PATH):
                raise AssertionError('Could not push md5sum to device')

        self._push_executable()
        self._push_fonts()
        self._push_test_resources()
        self._push_platform_resources()

    def _setup_test(self):
        if self._has_setup:
            return

        self._android_commands.restart_as_root()
        self._setup_md5sum_and_push_data_if_needed()
        self._has_setup = True
        self._setup_performance()

        # Required by webkit_support::GetWebKitRootDirFilePath().
        # Other directories will be created automatically by adb push.
        self._android_commands.run(['shell', 'mkdir', '-p', DEVICE_SOURCE_ROOT_DIR + 'chrome'])

        # Allow the test driver to get full read and write access to the directory.
        # The native code needs the permission to write temporary files and create pipes here.
        self._android_commands.run(['shell', 'mkdir', '-p', self._driver_details.device_directory()])
        self._android_commands.run(['shell', 'chmod', '777', self._driver_details.device_directory()])

        # Delete the disk cache if any to ensure a clean test run.
        # This is like what's done in ChromiumPort.setup_test_run but on the device.
        self._android_commands.run(['shell', 'rm', '-r', self._driver_details.device_cache_directory()])

    def _log_error(self, message):
        _log.error('[%s] %s' % (self._android_commands.get_serial(), message))

    def _log_debug(self, message):
        _log.debug('[%s] %s' % (self._android_commands.get_serial(), message))

    def _abort(self, message):
        raise AssertionError('[%s] %s' % (self._android_commands.get_serial(), message))

    @staticmethod
    def _extract_hashes_from_md5sum_output(md5sum_output):
        assert md5sum_output
        return [line.split('  ')[0] for line in md5sum_output]

    def _push_file_if_needed(self, host_file, device_file):
        assert os.path.exists(host_file)
        device_hashes = self._extract_hashes_from_md5sum_output(
                self._port.host.executive.popen(self._android_commands.adb_command() + ['shell', MD5SUM_DEVICE_PATH, device_file],
                                                stdout=subprocess.PIPE).stdout)
        host_hashes = self._extract_hashes_from_md5sum_output(
                self._port.host.executive.popen(args=['%s_host' % self._md5sum_path, host_file],
                                                stdout=subprocess.PIPE).stdout)
        if host_hashes and device_hashes == host_hashes:
            return

        self._android_commands.push(host_file, device_file)

    def _push_executable(self):
        self._push_file_if_needed(self._port.path_to_forwarder(), self._driver_details.device_forwarder_path())
        for resource in self._driver_details.additional_resources():
            self._push_file_if_needed(self._port._build_path(resource), self._driver_details.device_directory() + resource)

        self._push_file_if_needed(self._port._build_path('android_main_fonts.xml'), self._driver_details.device_directory() + 'android_main_fonts.xml')
        self._push_file_if_needed(self._port._build_path('android_fallback_fonts.xml'), self._driver_details.device_directory() + 'android_fallback_fonts.xml')

        self._android_commands.run(['uninstall', self._driver_details.package_name()])
        driver_host_path = self._port._path_to_driver()
        install_result = self._android_commands.run(['install', driver_host_path])
        if install_result.find('Success') == -1:
            self._abort('Failed to install %s onto device: %s' % (driver_host_path, install_result))

    def _push_fonts(self):
        self._log_debug('Pushing fonts')
        path_to_ahem_font = self._port._build_path('AHEM____.TTF')
        self._push_file_if_needed(path_to_ahem_font, self._driver_details.device_fonts_directory() + 'AHEM____.TTF')
        for (host_dirs, font_file, package) in HOST_FONT_FILES:
            for host_dir in host_dirs:
                host_font_path = host_dir + font_file
                if self._port._check_file_exists(host_font_path, '', logging=False):
                    self._push_file_if_needed(host_font_path, self._driver_details.device_fonts_directory() + font_file)

    def _push_test_resources(self):
        self._log_debug('Pushing test resources')
        for resource in TEST_RESOURCES_TO_PUSH:
            self._push_file_if_needed(self._port.layout_tests_dir() + '/' + resource, DEVICE_LAYOUT_TESTS_DIR + resource)

    def _push_platform_resources(self):
        self._log_debug('Pushing platform resources')
        external_storage = self._port._filesystem.join(self._android_commands.run(['shell', 'echo $EXTERNAL_STORAGE']).strip(), 'Source', 'WebKit', 'chromium')
        for resource in WEBKIT_PLATFORM_RESOURCES_TO_PUSH:
            self._push_file_if_needed(self._port._chromium_base_dir(self._port._filesystem) + '/' + resource, external_storage + '/' + resource)

    def _get_last_stacktrace(self):
        tombstones = self._android_commands.run(['shell', 'ls', '-n', '/data/tombstones'])
        if not tombstones or tombstones.startswith('/data/tombstones: No such file or directory'):
            self._log_error('The driver crashed, but no tombstone found!')
            return ''
        tombstones = tombstones.rstrip().split('\n')
        last_tombstone = tombstones[0].split()
        for tombstone in tombstones[1:]:
            # Format of fields:
            # 0          1      2      3     4          5     6
            # permission uid    gid    size  date       time  filename
            # -rw------- 1000   1000   45859 2011-04-13 06:00 tombstone_00
            fields = tombstone.split()
            if (fields[4] + fields[5] >= last_tombstone[4] + last_tombstone[5]):
                last_tombstone = fields
            else:
                break

        # Use Android tool vendor/google/tools/stack to convert the raw
        # stack trace into a human readable format, if needed.
        # It takes a long time, so don't do it here.
        return '%s\n%s' % (' '.join(last_tombstone),
                           self._android_commands.run(['shell', 'cat', '/data/tombstones/' + last_tombstone[6]]))

    def _get_logcat(self):
        return self._android_commands.run(['logcat', '-d', '-v', 'threadtime'])

    def _setup_performance(self):
        # Disable CPU scaling and drop ram cache to reduce noise in tests
        if not self._original_governors:
            governor_files = self._android_commands.run(['shell', 'ls', SCALING_GOVERNORS_PATTERN])
            if governor_files.find('No such file or directory') == -1:
                for file in governor_files.split():
                    self._original_governors[file] = self._android_commands.run(['shell', 'cat', file]).strip()
                    self._android_commands.run(['shell', 'echo', 'performance', '>', file])

    def _teardown_performance(self):
        for file, original_content in self._original_governors.items():
            self._android_commands.run(['shell', 'echo', original_content, '>', file])
        self._original_governors = {}

    def _get_crash_log(self, stdout, stderr, newer_than):
        if not stdout:
            stdout = ''
        stdout += '********* [%s] Logcat:\n%s' % (self._android_commands.get_serial(), self._get_logcat())
        if not stderr:
            stderr = ''
        stderr += '********* [%s] Tombstone file:\n%s' % (self._android_commands.get_serial(), self._get_last_stacktrace())
        return super(ChromiumAndroidDriver, self)._get_crash_log(stdout, stderr, newer_than)

    def cmd_line(self, pixel_tests, per_test_args):
        # The returned command line is used to start _server_process. In our case, it's an interactive 'adb shell'.
        # The command line passed to the driver process is returned by _driver_cmd_line() instead.
        return self._android_commands.adb_command() + ['shell']

    def _android_driver_cmd_line(self, pixel_tests, per_test_args):
        return driver.Driver.cmd_line(self, pixel_tests, per_test_args) + self._driver_details.additional_command_line_flags()

    @staticmethod
    def _loop_with_timeout(condition, timeout_secs):
        deadline = time.time() + timeout_secs
        while time.time() < deadline:
            if condition():
                return True
        return False

    def _all_pipes_created(self):
        return (self._android_commands.file_exists(self._in_fifo_path) and
                self._android_commands.file_exists(self._out_fifo_path) and
                self._android_commands.file_exists(self._err_fifo_path))

    def _remove_all_pipes(self):
        for file in [self._in_fifo_path, self._out_fifo_path, self._err_fifo_path]:
            self._android_commands.run(['shell', 'rm', file])

        return (not self._android_commands.file_exists(self._in_fifo_path) and
                not self._android_commands.file_exists(self._out_fifo_path) and
                not self._android_commands.file_exists(self._err_fifo_path))

    def run_test(self, driver_input, stop_when_done):
        base = self._port.lookup_virtual_test_base(driver_input.test_name)
        if base:
            driver_input = copy.copy(driver_input)
            driver_input.args = self._port.lookup_virtual_test_args(driver_input.test_name)
            driver_input.test_name = base
        return super(ChromiumAndroidDriver, self).run_test(driver_input, stop_when_done)

    def start(self, pixel_tests, per_test_args):
        # We override the default start() so that we can call _android_driver_cmd_line()
        # instead of cmd_line().
        new_cmd_line = self._android_driver_cmd_line(pixel_tests, per_test_args)
        if new_cmd_line != self._current_cmd_line:
            self.stop()
        super(ChromiumAndroidDriver, self).start(pixel_tests, per_test_args)

    def _start(self, pixel_tests, per_test_args):
        self._setup_test()

        for retries in range(3):
            if self._start_once(pixel_tests, per_test_args):
                return
            self._log_error('Failed to start the content_shell application. Retries=%d. Log:%s' % (retries, self._get_logcat()))
            self.stop()
            time.sleep(2)
        self._abort('Failed to start the content_shell application multiple times. Giving up.')

    def _start_once(self, pixel_tests, per_test_args):
        super(ChromiumAndroidDriver, self)._start(pixel_tests, per_test_args)

        self._log_debug('Starting forwarder')
        self._forwarder_process = self._port._server_process_constructor(
            self._port, 'Forwarder', self._android_commands.adb_command() + ['shell', '%s -D %s' % (self._driver_details.device_forwarder_path(), FORWARD_PORTS)])
        self._forwarder_process.start()

        self._android_commands.run(['logcat', '-c'])
        self._android_commands.run(['shell', 'echo'] + self._android_driver_cmd_line(pixel_tests, per_test_args) + ['>', self._driver_details.command_line_file()])
        start_result = self._android_commands.run(['shell', 'am', 'start', '-e', 'RunInSubThread', '-n', self._driver_details.activity_name()])
        if start_result.find('Exception') != -1:
            self._log_error('Failed to start the content_shell application. Exception:\n' + start_result)
            return False

        if not ChromiumAndroidDriver._loop_with_timeout(self._all_pipes_created, DRIVER_START_STOP_TIMEOUT_SECS):
            return False

        # Read back the shell prompt to ensure adb shell ready.
        deadline = time.time() + DRIVER_START_STOP_TIMEOUT_SECS
        self._server_process.start()
        self._read_prompt(deadline)
        self._log_debug('Interactive shell started')

        # Start a process to read from the stdout fifo of the test driver and print to stdout.
        self._log_debug('Redirecting stdout to ' + self._out_fifo_path)
        self._read_stdout_process = self._port._server_process_constructor(
            self._port, 'ReadStdout', self._android_commands.adb_command() + ['shell', 'cat', self._out_fifo_path])
        self._read_stdout_process.start()

        # Start a process to read from the stderr fifo of the test driver and print to stdout.
        self._log_debug('Redirecting stderr to ' + self._err_fifo_path)
        self._read_stderr_process = self._port._server_process_constructor(
            self._port, 'ReadStderr', self._android_commands.adb_command() + ['shell', 'cat', self._err_fifo_path])
        self._read_stderr_process.start()

        self._log_debug('Redirecting stdin to ' + self._in_fifo_path)
        self._server_process.write('cat >%s\n' % self._in_fifo_path)

        # Combine the stdout and stderr pipes into self._server_process.
        self._server_process.replace_outputs(self._read_stdout_process._proc.stdout, self._read_stderr_process._proc.stdout)

        def deadlock_detector(processes, normal_startup_event):
            if not ChromiumAndroidDriver._loop_with_timeout(lambda: normal_startup_event.is_set(), DRIVER_START_STOP_TIMEOUT_SECS):
                # If normal_startup_event is not set in time, the main thread must be blocked at
                # reading/writing the fifo. Kill the fifo reading/writing processes to let the
                # main thread escape from the deadlocked state. After that, the main thread will
                # treat this as a crash.
                self._log_error('Deadlock detected. Processes killed.')
                for i in processes:
                    i.kill()

        # Start a thread to kill the pipe reading/writing processes on deadlock of the fifos during startup.
        normal_startup_event = threading.Event()
        threading.Thread(name='DeadlockDetector', target=deadlock_detector,
                         args=([self._server_process, self._read_stdout_process, self._read_stderr_process], normal_startup_event)).start()

        output = ''
        line = self._server_process.read_stdout_line(deadline)
        while not self._server_process.timed_out and not self.has_crashed() and line.rstrip() != '#READY':
            output += line
            line = self._server_process.read_stdout_line(deadline)

        if self._server_process.timed_out and not self.has_crashed():
            # The test driver crashed during startup, or when the deadlock detector hit
            # a deadlock and killed the fifo reading/writing processes.
            _log.error('Failed to start the test driver: \n%s' % output)
            return False

        # Inform the deadlock detector that the startup is successful without deadlock.
        normal_startup_event.set()
        return True

    def _pid_from_android_ps_output(self, ps_output, package_name):
        # ps output seems to be fixed width, we only care about the name and the pid
        # u0_a72    21630 125   947920 59364 ffffffff 400beee4 S org.chromium.native_test
        for line in ps_output.split('\n'):
            if line.find(self._driver_details.package_name()) != -1:
                match = re.match(r'\S+\s+(\d+)', line)
                return int(match.group(1))

    def _pid_on_target(self):
        # FIXME: There must be a better way to do this than grepping ps output!
        ps_output = self._android_commands.run(['shell', 'ps'])
        return self._pid_from_android_ps_output(ps_output, self._driver_details.package_name())

    def stop(self):
        self._android_commands.run(['shell', 'am', 'force-stop', self._driver_details.package_name()])

        if self._read_stdout_process:
            self._read_stdout_process.kill()
            self._read_stdout_process = None

        if self._read_stderr_process:
            self._read_stderr_process.kill()
            self._read_stderr_process = None

        super(ChromiumAndroidDriver, self).stop()

        if self._forwarder_process:
            self._forwarder_process.kill()
            self._forwarder_process = None

        if self._has_setup:
            if not ChromiumAndroidDriver._loop_with_timeout(self._remove_all_pipes, DRIVER_START_STOP_TIMEOUT_SECS):
                raise AssertionError('Failed to remove fifo files. May be locked.')

    def _command_from_driver_input(self, driver_input):
        command = super(ChromiumAndroidDriver, self)._command_from_driver_input(driver_input)
        if command.startswith('/'):
            fs = self._port._filesystem
            # FIXME: what happens if command lies outside of the layout_tests_dir on the host?
            relative_test_filename = fs.relpath(command, fs.dirname(self._port.layout_tests_dir()))
            command = DEVICE_WEBKIT_BASE_DIR + relative_test_filename
        return command

    def _read_prompt(self, deadline):
        last_char = ''
        while True:
            current_char = self._server_process.read_stdout(deadline, 1)
            if current_char == ' ':
                if last_char in ('#', '$'):
                    return
            last_char = current_char
