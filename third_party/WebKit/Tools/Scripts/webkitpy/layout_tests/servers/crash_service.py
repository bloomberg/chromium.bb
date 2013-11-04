# Copyright (C) 2013 Google Inc. All rights reserved.
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

"""A class to help start/stop the crash service used by layout tests."""

import logging
import os
import time

from webkitpy.layout_tests.servers import http_server_base


_log = logging.getLogger(__name__)


class CrashService(http_server_base.HttpServerBase):

    def __init__(self, port_obj, crash_dumps_dir):
        """Args:
          crash_dumps_dir: the absolute path to the directory where to store crash dumps
        """
        # Webkit tests
        http_server_base.HttpServerBase.__init__(self, port_obj)
        self._name = 'CrashService'
        self._crash_dumps_dir = crash_dumps_dir

        self._pid_file = self._filesystem.join(self._runtime_path, '%s.pid' % self._name)

    def _spawn_process(self):
        start_cmd = [self._port_obj._path_to_crash_service(),
                     '--dumps-dir=%s' % self._crash_dumps_dir,
                     '--no-window']
        _log.debug('Starting crash service, cmd = "%s"' % " ".join(start_cmd))
        process = self._executive.popen(start_cmd, shell=False, stderr=self._executive.PIPE)
        pid = process.pid
        self._filesystem.write_text_file(self._pid_file, str(pid))
        return pid

    def _stop_running_server(self):
        # FIXME: It would be nice if we had a cleaner way of killing this process.
        # Currently we throw away the process object created in _spawn_process,
        # since there doesn't appear to be any way to kill the server any more
        # cleanly using it than just killing the pid, and we need to support
        # killing a pid directly anyway for run-webkit-httpd and run-webkit-websocketserver.
        self._wait_for_action(self._check_and_kill)
        if self._filesystem.exists(self._pid_file):
            self._filesystem.remove(self._pid_file)

    def _check_and_kill(self):
        if self._executive.check_running_pid(self._pid):
            host = self._port_obj.host
            if host.platform.is_win() and not host.platform.is_cygwin():
                # FIXME: https://bugs.webkit.org/show_bug.cgi?id=106838
                # We need to kill all of the child processes as well as the
                # parent, so we can't use executive.kill_process().
                #
                # If this is actually working, we should figure out a clean API.
                self._executive.run_command(["taskkill.exe", "/f", "/t", "/pid", self._pid], error_handler=self._executive.ignore_error)
            else:
                self._executive.kill_process(self._pid)
            return False
        return True
