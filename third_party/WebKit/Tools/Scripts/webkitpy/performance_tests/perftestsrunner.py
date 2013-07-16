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

"""Run perf tests in perf mode."""

import os
import logging
import optparse
import subprocess
import tempfile

from webkitpy.common.host import Host

_log = logging.getLogger(__name__)


class PerfTestsRunner(object):

    def __init__(self, args=None, port=None):
        self._options, self._args = PerfTestsRunner._parse_args(args)
        if port:
            self._port = port
            self._host = self._port.host
        else:
            self._host = Host()
            self._port = self._host.port_factory.get(options=self._options)

    @staticmethod
    def _parse_args(args=None):
        def _expand_path(option, opt_str, value, parser):
            path = os.path.expandvars(os.path.expanduser(value))
            setattr(parser.values, option.dest, path)
        perf_option_list = [
            optparse.make_option("--browser", default='release',
                help="Specify browser being tested (e.g. release, android-content-shell)"),
            optparse.make_option('--debug', action='store_const', const='debug', dest="browser",
                help='Set the configuration to Debug (alias for --browser=debug)'),
            optparse.make_option('--release', action='store_const', const='release', dest="browser",
                help='Set the configuration to Release (alias for --browser=release)'),
            optparse.make_option("--android",
                action="store_const", const='android', dest="browser",
                help='Alias for --browser=android',),
            optparse.make_option("--profiler", action="store",
                help="Output per-test profile information, using the specified profiler (e.g. perf, iprofiler)."),
            optparse.make_option("--additional-flag", action="append",
                default=[], help="Additional command line flag to pass to content shell"
                     "Specify multiple times to add multiple flags."),
            ]
        return optparse.OptionParser(option_list=(perf_option_list)).parse_args(args)

    def run(self):
        telemetry_cmd = os.path.join(self._port.webkit_base(), '..', '..', 'tools', 'perf', 'run_measurement')
        assert os.path.exists(telemetry_cmd), telemetry_cmd + ' does not exist'

        cmd = [telemetry_cmd, '--browser=' + self._options.browser, 'blink_perf']

        if self._options.profiler:
            cmd.append('--profiler-dir=' + tempfile.mkdtemp())
            cmd.append('--profiler-tool=' + self._options.profiler)

        if self._options.additional_flag:
            cmd.append('--extra-browser-args="%s"' % ' '.join(self._options.additional_flag))

        perf_tests_dir = os.path.join('third_party', 'WebKit', 'PerformanceTests')
        if not self._args:
            test_paths = [perf_tests_dir]
        else:
            test_paths = [os.path.join(perf_tests_dir, p) for p in self._args]
        cmd.extend(test_paths)

        return subprocess.call(cmd)
