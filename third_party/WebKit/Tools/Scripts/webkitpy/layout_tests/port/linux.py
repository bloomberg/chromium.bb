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

import cgi
import logging
import re

from webkitpy.common.webkit_finder import WebKitFinder
from webkitpy.layout_tests.models import test_run_results
from webkitpy.layout_tests.port import base
from webkitpy.layout_tests.port import win
from webkitpy.layout_tests.port import config


_log = logging.getLogger(__name__)


class LinuxPort(base.Port):
    port_name = 'linux'

    SUPPORTED_VERSIONS = ('x86', 'x86_64')

    FALLBACK_PATHS = { 'x86_64': [ 'linux' ] + win.WinPort.latest_platform_fallback_path() }
    FALLBACK_PATHS['x86'] = ['linux-x86'] + FALLBACK_PATHS['x86_64']

    DEFAULT_BUILD_DIRECTORIES = ('out',)

    @classmethod
    def _determine_driver_path_statically(cls, host, options):
        config_object = config.Config(host.executive, host.filesystem)
        build_directory = getattr(options, 'build_directory', None)
        finder = WebKitFinder(host.filesystem)
        webkit_base = finder.webkit_base()
        chromium_base = finder.chromium_base()
        driver_name = getattr(options, 'driver_name', None)
        if driver_name is None:
            driver_name = cls.CONTENT_SHELL_NAME
        if hasattr(options, 'configuration') and options.configuration:
            configuration = options.configuration
        else:
            configuration = config_object.default_configuration()
        return cls._static_build_path(host.filesystem, build_directory, chromium_base, configuration, [driver_name])

    @staticmethod
    def _determine_architecture(filesystem, executive, driver_path):
        file_output = ''
        if filesystem.isfile(driver_path):
            # The --dereference flag tells file to follow symlinks
            file_output = executive.run_command(['file', '--brief', '--dereference', driver_path], return_stderr=True)

        if re.match(r'ELF 32-bit LSB\s+executable', file_output):
            return 'x86'
        if re.match(r'ELF 64-bit LSB\s+executable', file_output):
            return 'x86_64'
        if file_output:
            _log.warning('Could not determine architecture from "file" output: %s' % file_output)

        # We don't know what the architecture is; default to 'x86' because
        # maybe we're rebaselining and the binary doesn't actually exist,
        # or something else weird is going on. It's okay to do this because
        # if we actually try to use the binary, check_build() should fail.
        return 'x86_64'

    @classmethod
    def determine_full_port_name(cls, host, options, port_name):
        if port_name.endswith('linux'):
            return port_name + '-' + cls._determine_architecture(host.filesystem, host.executive, cls._determine_driver_path_statically(host, options))
        return port_name

    def __init__(self, host, port_name, **kwargs):
        super(LinuxPort, self).__init__(host, port_name, **kwargs)
        (base, arch) = port_name.rsplit('-', 1)
        assert base == 'linux'
        assert arch in self.SUPPORTED_VERSIONS
        assert port_name in ('linux', 'linux-x86', 'linux-x86_64')
        self._version = 'lucid'  # We only support lucid right now.
        self._architecture = arch
        self._breakpad_tools_available = None

    def additional_drt_flag(self):
        flags = super(LinuxPort, self).additional_drt_flag()
        flags += ['--enable-crash-reporter', '--crash-dumps-dir=%s' % self.crash_dumps_directory()]
        return flags

    def default_baseline_search_path(self):
        port_names = self.FALLBACK_PATHS[self._architecture]
        return map(self._webkit_baseline_path, port_names)

    def _modules_to_search_for_symbols(self):
        return [self._build_path('libffmpegsumo.so')]

    def check_build(self, needs_http, printer):
        result = super(LinuxPort, self).check_build(needs_http, printer)

        self._breakpad_tools_available = self._check_breakpad_tools_available()
        if not self._breakpad_tools_available:
            result = test_run_results.UNEXPECTED_ERROR_EXIT_STATUS

        if result:
            _log.error('For complete Linux build requirements, please see:')
            _log.error('')
            _log.error('    http://code.google.com/p/chromium/wiki/LinuxBuildInstructions')
        return result

    def setup_test_run(self):
        super(LinuxPort, self).setup_test_run()

        if self._filesystem.exists(self.crash_dumps_directory()):
            self._filesystem.rmtree(self.crash_dumps_directory())
        self._filesystem.maybe_make_directory(self.crash_dumps_directory())

    def look_for_new_crash_logs(self, crashed_processes, start_time):
        if not crashed_processes:
            return None

        if self._breakpad_tools_available == None:
            self._breakpad_tools_available = self._check_breakpad_tools_available()
        if not self._breakpad_tools_available:
            return None

        pid_to_minidump = dict()
        for root, dirs, files in self._filesystem.walk(self.crash_dumps_directory()):
            for dmp in [f for f in files if f.endswith('.dmp')]:
                dmp_file = self._filesystem.join(root, dmp)
                if self._filesystem.mtime(dmp_file) < start_time:
                    continue
                with self._filesystem.open_binary_file_for_reading(dmp_file) as f:
                    boundary = f.readline().strip()[2:]
                    data = cgi.parse_multipart(f, {'boundary': boundary})
                    if 'pid' in data:
                        pid_to_minidump[data['pid'][0]] = '\r\n'.join(data['upload_file_minidump'])

        result = dict()
        for test, process_name, pid in crashed_processes:
            if str(pid) in pid_to_minidump:
                self._generate_breakpad_symbols_if_necessary()
                f, temp_name = self._filesystem.open_binary_tempfile('dmp')
                f.write(pid_to_minidump[str(pid)])
                f.close()

                cmd = [self._path_to_minidump_stackwalk(), temp_name, self._path_to_breakpad_symbols()]
                try:
                    stack = self._executive.run_command(cmd, return_stderr=False)
                except:
                    _log.warning('Failed to execute "%s"' % ' '.join(cmd))
                else:
                    result[test] = stack

                self._filesystem.remove(temp_name)

        return result

    def operating_system(self):
        return 'linux'

    def virtual_test_suites(self):
        result = super(LinuxPort, self).virtual_test_suites()
        result.extend([
            base.VirtualTestSuite('linux-subpixel',
                                  'platform/linux/fast/text/subpixel',
                                  ['--enable-webkit-text-subpixel-positioning']),
        ])
        return result

    #
    # PROTECTED METHODS
    #

    def _check_apache_install(self):
        result = self._check_file_exists(self._path_to_apache(), "apache2")
        result = self._check_file_exists(self._path_to_apache_config_file(), "apache2 config file") and result
        if not result:
            _log.error('    Please install using: "sudo apt-get install apache2 libapache2-mod-php5"')
            _log.error('')
        return result

    def _check_lighttpd_install(self):
        result = self._check_file_exists(
            self._path_to_lighttpd(), "LigHTTPd executable")
        result = self._check_file_exists(self._path_to_lighttpd_php(), "PHP CGI executable") and result
        result = self._check_file_exists(self._path_to_lighttpd_modules(), "LigHTTPd modules") and result
        if not result:
            _log.error('    Please install using: "sudo apt-get install lighttpd php5-cgi"')
            _log.error('')
        return result

    def _wdiff_missing_message(self):
        return 'wdiff is not installed; please install using "sudo apt-get install wdiff"'

    def _path_to_apache(self):
        # The Apache binary path can vary depending on OS and distribution
        # See http://wiki.apache.org/httpd/DistrosDefaultLayout
        for path in ["/usr/sbin/httpd", "/usr/sbin/apache2"]:
            if self._filesystem.exists(path):
                return path
        _log.error("Could not find apache. Not installed or unknown path.")
        return None

    def _path_to_lighttpd(self):
        return "/usr/sbin/lighttpd"

    def _path_to_lighttpd_modules(self):
        return "/usr/lib/lighttpd"

    def _path_to_lighttpd_php(self):
        return "/usr/bin/php-cgi"

    def _path_to_driver(self, configuration=None):
        binary_name = self.driver_name()
        return self._build_path_with_configuration(configuration, binary_name)

    def _path_to_helper(self):
        return None

    def _path_to_minidump_stackwalk(self):
        return self._build_path("minidump_stackwalk")

    def _path_to_dump_syms(self):
        return self._build_path("dump_syms")

    def _path_to_breakpad_symbols(self):
        return self._build_path("content_shell.syms")

    def _path_to_generate_breakpad_symbols(self):
        return self.path_from_chromium_base("components", "breakpad", "tools", "generate_breakpad_symbols.py")

    def _check_breakpad_tools_available(self):
        result = self._check_file_exists(self._path_to_dump_syms(), "dump_syms")
        result = self._check_file_exists(self._path_to_minidump_stackwalk(), "minidump_stackwalk") and result
        if not result:
            _log.error("    Could not find breakpad tools, unexpected crashes won't be symbolized")
            _log.error('    Did you build the target all_webkit?')
            _log.error('')
        return result

    def _generate_breakpad_symbols_if_necessary(self):
        if self._filesystem.exists(self._path_to_breakpad_symbols()) and (self._filesystem.mtime(self._path_to_driver()) < self._filesystem.mtime(self._path_to_breakpad_symbols())):
            return

        BINARIES = [
            self._path_to_driver(),
            self._build_path("libTestNetscapePlugIn.so"),
        ]

        _log.debug("Regenerating breakpad symbols")
        self._filesystem.rmtree(self._path_to_breakpad_symbols())

        for binary in BINARIES:
            cmd = [
                self._path_to_generate_breakpad_symbols(),
                '--binary=%s' % binary,
                '--symbols-dir=%s' % self._path_to_breakpad_symbols(),
                '--build-dir=%s' % self._build_path(),
            ]
            try:
                self._executive.run_command(cmd)
            except:
                _log.error('Failed to execute "%s"' % ' '.join(cmd))
