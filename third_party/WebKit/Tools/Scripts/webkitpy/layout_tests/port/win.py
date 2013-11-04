# Copyright (C) 2010 Google Inc. All rights reserved.
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

"""Windows implementation of the Port interface."""

import os
import logging
import shlex

from webkitpy.layout_tests.port import base
from webkitpy.layout_tests.servers import crash_service


_log = logging.getLogger(__name__)


class WinPort(base.Port):
    port_name = 'win'

    # FIXME: Figure out how to unify this with base.TestConfiguration.all_systems()?
    SUPPORTED_VERSIONS = ('xp', 'win7')

    FALLBACK_PATHS = { 'win7': [ 'win' ]}
    FALLBACK_PATHS['xp'] = ['win-xp'] + FALLBACK_PATHS['win7']

    DEFAULT_BUILD_DIRECTORIES = ('build', 'out')

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        if port_name.endswith('win'):
            assert host.platform.is_win()
            # We don't maintain separate baselines for vista, so we pretend it is win7.
            if host.platform.os_version in ('vista', '7sp0', '7sp1', 'future'):
                version = 'win7'
            else:
                version = host.platform.os_version
            port_name = port_name + '-' + version
        return port_name

    def __init__(self, host, port_name, **kwargs):
        super(WinPort, self).__init__(host, port_name, **kwargs)
        self._version = port_name[port_name.index('win-') + len('win-'):]
        assert self._version in self.SUPPORTED_VERSIONS, "%s is not in %s" % (self._version, self.SUPPORTED_VERSIONS)
        self._crash_service = None
        self._cdb_available = None

    def additional_drt_flag(self):
        flags = super(WinPort, self).additional_drt_flag()
        flags += ['--enable-crash-reporter', '--crash-dumps-dir=%s' % self.crash_dumps_directory()]
        return flags

    def setup_test_run(self):
        super(WinPort, self).setup_test_run()

        assert not self._crash_service, 'Already running a crash service'
        service = crash_service.CrashService(self, self.crash_dumps_directory())
        service.start()
        self._crash_service = service

    def clean_up_test_run(self):
        super(WinPort, self).clean_up_test_run()

        if self._crash_service:
            self._crash_service.stop()
            self._crash_service = None

    def setup_environ_for_server(self, server_name=None):
        env = super(WinPort, self).setup_environ_for_server(server_name)

        # FIXME: lighttpd depends on some environment variable we're not whitelisting.
        # We should add the variable to an explicit whitelist in base.Port.
        # FIXME: This is a temporary hack to get the cr-win bot online until
        # someone from the cr-win port can take a look.
        for key, value in os.environ.items():
            if key not in env:
                env[key] = value

        # Put the cygwin directory first in the path to find cygwin1.dll.
        env["PATH"] = "%s;%s" % (self.path_from_chromium_base("third_party", "cygwin", "bin"), env["PATH"])
        # Configure the cygwin directory so that pywebsocket finds proper
        # python executable to run cgi program.
        env["CYGWIN_PATH"] = self.path_from_chromium_base("third_party", "cygwin", "bin")
        if self.get_option('register_cygwin'):
            setup_mount = self.path_from_chromium_base("third_party", "cygwin", "setup_mount.bat")
            self._executive.run_command([setup_mount])  # Paths are all absolute, so this does not require a cwd.
        return env

    def _modules_to_search_for_symbols(self):
        # FIXME: we should return the path to the ffmpeg equivalents to detect if we have the mp3 and aac codecs installed.
        # See https://bugs.webkit.org/show_bug.cgi?id=89706.
        return []

    def check_build(self, needs_http, printer):
        result = super(WinPort, self).check_build(needs_http, printer)

        result = self._check_cdb_available() and result

        if result:
            _log.error('For complete Windows build requirements, please see:')
            _log.error('')
            _log.error('    http://dev.chromium.org/developers/how-tos/build-instructions-windows')
        return result

    def operating_system(self):
        return 'win'

    def relative_test_filename(self, filename):
        path = filename[len(self.layout_tests_dir()) + 1:]
        return path.replace('\\', '/')

    #
    # PROTECTED ROUTINES
    #

    def _uses_apache(self):
        return False

    def _lighttpd_path(self, *comps):
        return self.path_from_chromium_base('third_party', 'lighttpd', 'win', *comps)

    def _path_to_apache(self):
        return self.path_from_chromium_base('third_party', 'cygwin', 'usr', 'sbin', 'httpd')

    def _path_to_apache_config_file(self):
        return self._filesystem.join(self.layout_tests_dir(), 'http', 'conf', 'cygwin-httpd.conf')

    def _path_to_lighttpd(self):
        return self._lighttpd_path('LightTPD.exe')

    def _path_to_lighttpd_modules(self):
        return self._lighttpd_path('lib')

    def _path_to_lighttpd_php(self):
        return self._lighttpd_path('php5', 'php-cgi.exe')

    def _path_to_driver(self, configuration=None):
        binary_name = '%s.exe' % self.driver_name()
        return self._build_path_with_configuration(configuration, binary_name)

    def _path_to_helper(self):
        binary_name = 'LayoutTestHelper.exe'
        return self._build_path(binary_name)

    def _path_to_crash_service(self):
        binary_name = 'content_shell_crash_service.exe'
        return self._build_path(binary_name)

    def _path_to_image_diff(self):
        binary_name = 'image_diff.exe'
        return self._build_path(binary_name)

    def _path_to_wdiff(self):
        return self.path_from_chromium_base('third_party', 'cygwin', 'bin', 'wdiff.exe')

    def _check_cdb_available(self, logging=True):
        """Checks whether we can use cdb to symbolize minidumps."""
        CDB_LOCATION_TEMPLATES = [
            '%s\\Debugging Tools For Windows',
            '%s\\Debugging Tools For Windows (x86)',
            '%s\\Debugging Tools For Windows (x64)',
            '%s\\Windows Kits\\8.0\\Debuggers\\x86',
            '%s\\Windows Kits\\8.0\\Debuggers\\x64',
        ]

        program_files_directories = ['C:\\Program Files']
        program_files = os.environ.get('ProgramFiles')
        if program_files:
            program_files_directories.append(program_files)
        program_files = os.environ.get('ProgramFiles(x86)')
        if program_files:
            program_files_directories.append(program_files)

        possible_cdb_locations = []
        for template in CDB_LOCATION_TEMPLATES:
            for program_files in program_files_directories:
                possible_cdb_locations.append(template % program_files)

        gyp_defines = os.environ.get('GYP_DEFINES', [])
        if gyp_defines:
            gyp_defines = shlex.split(gyp_defines)
        if 'windows_sdk_path' in gyp_defines:
            possible_cdb_locations.append([
                '%s\\Debuggers\\x86' % gyp_defines['windows_sdk_path'],
                '%s\\Debuggers\\x64' % gyp_defines['windows_sdk_path'],
            ])

        for cdb_path in possible_cdb_locations:
            cdb = self._filesystem.join(cdb_path, 'cdb.exe')
            try:
                _ = self._executive.run_command([cdb, '-version'])
            except:
                pass
            else:
                self._cdb_path = cdb
                return True

        if logging:
            _log.warning("CDB is not installed; can't symbolize minidumps.")
            _log.warning('')
        return False

    def look_for_new_crash_logs(self, crashed_processes, start_time):
        if not crashed_processes:
            return None

        if self._cdb_available is None:
            self._cdb_available = self._check_cdb_available(logging=False)

        if not self._cdb_available:
            return None

        pid_to_minidump = dict()
        for root, dirs, files in self._filesystem.walk(self.crash_dumps_directory()):
            for dmp in [f for f in files if f.endswith('.txt')]:
                dmp_file = self._filesystem.join(root, dmp)
                if self._filesystem.mtime(dmp_file) < start_time:
                    continue
                with self._filesystem.open_text_file_for_reading(dmp_file) as f:
                    crash_keys = dict([l.split(':', 1) for l in f.read().splitlines()])
                    if 'pid' in crash_keys:
                        pid_to_minidump[crash_keys['pid']] = dmp_file[:-3] + 'dmp'

        result = dict()
        for test, process_name, pid in crashed_processes:
            if str(pid) in pid_to_minidump:
                cmd = [self._cdb_path, '-y', self._build_path(), '-c', '.ecxr;k30;q', '-z', pid_to_minidump[str(pid)]]
                try:
                    stack = self._executive.run_command(cmd)
                except:
                    _log.warning('Failed to execute "%s"' % ' '.join(cmd))
                else:
                    result[test] = stack

        return result
