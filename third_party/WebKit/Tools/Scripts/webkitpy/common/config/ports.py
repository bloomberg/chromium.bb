# Copyright (C) 2009, Google Inc. All rights reserved.
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
#
# WebKit's Python module for understanding the various ports

import os
import platform
import sys

from webkitpy.common.system.executive import Executive


class DeprecatedPort(object):
    results_directory = "/tmp/layout-test-results"

    # Subclasses must override
    port_flag_name = None

    # FIXME: This is only used by BotInfo.
    def name(self):
        return self.__class__

    def flag(self):
        if self.port_flag_name:
            return "--port=%s" % self.port_flag_name
        return None

    # We might need to pass scm into this function for scm.checkout_root
    def script_path(self, script_name):
        return os.path.join("Tools", "Scripts", script_name)

    def script_shell_command(self, script_name):
        script_path = self.script_path(script_name)
        return Executive.shell_command_for_script(script_path)

    @staticmethod
    def port(port_name):
        ports = {
            "chromium": ChromiumPort,
            "chromium-android": ChromiumAndroidPort,
            "chromium-xvfb": ChromiumXVFBPort,
        }
        return ports.get(port_name, ChromiumPort)()

    def makeArgs(self):
        # FIXME: This shouldn't use a static Executive().
        args = '--makeargs="-j%s"' % Executive().cpu_count()
        if os.environ.has_key('MAKEFLAGS'):
            args = '--makeargs="%s"' % os.environ['MAKEFLAGS']
        return args

    def check_webkit_style_command(self):
        return self.script_shell_command("check-webkit-style")

    def run_webkit_unit_tests_command(self):
        return None

    def run_webkit_tests_command(self):
        return self.script_shell_command("run-webkit-tests")

    def run_python_unittests_command(self):
        return self.script_shell_command("test-webkitpy")

    def run_perl_unittests_command(self):
        return self.script_shell_command("test-webkitperl")

    def run_bindings_tests_command(self):
        return self.script_shell_command("run-bindings-tests")


class ChromiumPort(DeprecatedPort):
    port_flag_name = "chromium"

    def run_webkit_tests_command(self):
        # Note: This could be run-webkit-tests now.
        command = self.script_shell_command("new-run-webkit-tests")
        command.append("--chromium")
        command.append("--skip-failing-tests")
        return command

    def run_webkit_unit_tests_command(self):
        return self.script_shell_command("run-chromium-webkit-unit-tests")


class ChromiumAndroidPort(ChromiumPort):
    port_flag_name = "chromium-android"


class ChromiumXVFBPort(ChromiumPort):
    port_flag_name = "chromium-xvfb"

    def run_webkit_tests_command(self):
        return ["xvfb-run"] + super(ChromiumXVFBPort, self).run_webkit_tests_command()
