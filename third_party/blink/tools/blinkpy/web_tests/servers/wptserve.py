# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Start and stop the WPTserve servers as they're used by the layout tests."""

import datetime
import logging

from blinkpy.common.path_finder import PathFinder
from blinkpy.web_tests.servers import server_base


_log = logging.getLogger(__name__)


class WPTServe(server_base.ServerBase):

    def __init__(self, port_obj, output_dir):
        super(WPTServe, self).__init__(port_obj, output_dir)
        # These ports must match wpt_support/wpt.config.json
        http_port, http_alt_port, https_port = (8001, 8081, 8444)
        ws_port, wss_port = (9001, 9444)
        self._name = 'wptserve'
        self._log_prefixes = ('access_log', 'error_log')
        self._mappings = [{'port': http_port},
                          {'port': http_alt_port},
                          {'port': https_port, 'sslcert': True},
                          {'port': ws_port},
                          {'port': wss_port, 'sslcert': True}]

        # TODO(burnik): We can probably avoid PID files for WPT in the future.
        fs = self._filesystem
        self._pid_file = fs.join(self._runtime_path, '%s.pid' % self._name)

        finder = PathFinder(fs)
        path_to_pywebsocket = finder.path_from_chromium_base('third_party', 'pywebsocket', 'src')
        path_to_wpt_support = finder.path_from_blink_tools('blinkpy', 'third_party', 'wpt')
        path_to_wpt_root = fs.join(path_to_wpt_support, 'wpt')
        path_to_wpt_config = fs.join(path_to_wpt_support, 'wpt.config.json')
        path_to_wpt_tests = fs.abspath(fs.join(self._port_obj.layout_tests_dir(), 'external', 'wpt'))
        path_to_ws_handlers = fs.join(path_to_wpt_tests, 'websockets', 'handlers')
        wpt_script = fs.join(path_to_wpt_root, 'wpt')
        start_cmd = [self._port_obj.host.executable,
                     '-u', wpt_script, 'serve',
                     '--config', path_to_wpt_config,
                     '--doc_root', path_to_wpt_tests]

        # TODO(burnik): Merge with default start_cmd once we roll in websockets.
        if self._port_obj.host.filesystem.exists(path_to_ws_handlers):
            start_cmd += ['--ws_doc_root', path_to_ws_handlers]

        # TODO(burnik): We should stop setting the CWD once WPT can be run without it.
        self._cwd = path_to_wpt_root
        self._env = port_obj.host.environ.copy()
        self._env.update({'PYTHONPATH': path_to_pywebsocket})
        self._start_cmd = start_cmd

        expiration_date = datetime.date(2025, 1, 4)
        if datetime.date.today() > expiration_date - datetime.timedelta(30):
            logging.getLogger(__name__).error(
                'Pre-generated keys and certificates are going to be expired at %s.'
                ' Please re-generate them by following steps in %s/README.chromium.',
                expiration_date.strftime('%b %d %Y'), path_to_wpt_support)

    def _stop_running_server(self):
        if not self._wait_for_action(self._check_and_kill):
            # This is mostly for POSIX systems. We send SIGINT in
            # _check_and_kill() and here we use SIGKILL.
            self._executive.kill_process(self._pid)

        if self._filesystem.exists(self._pid_file):
            self._filesystem.remove(self._pid_file)

    def _check_and_kill(self):
        """Tries to kill wptserve.

        Returns True if it appears to be not running. Or, if it appears to be
        running, tries to kill the process and returns False.
        """
        if not (self._pid and self._process):
            _log.warning('No process object or PID. wptserve has not started.')
            return True

        # Polls the process in case it has died; otherwise, the process might be
        # defunct and check_running_pid can still succeed.
        if self._process.poll() or not self._executive.check_running_pid(self._pid):
            _log.debug('pid %d is not running', self._pid)
            return True

        _log.debug('pid %d is running, killing it', self._pid)

        # kill_process() kills the whole process tree on Windows, but not on
        # POSIX, so we send SIGINT instead to allow wptserve to exit gracefully.
        if self._platform.is_win():
            self._executive.kill_process(self._pid)
        else:
            self._executive.interrupt(self._pid)

        return False
