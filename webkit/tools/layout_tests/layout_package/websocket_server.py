#!/usr/bin/env python
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to help start/stop the PyWebSocket server used by layout tests."""


import logging
import optparse
import os
import subprocess
import sys
import tempfile
import time

import path_utils
import platform_utils
import http_server

# So we can import httpd_utils below to make ui_tests happy.
sys.path.append(path_utils.PathFromBase('tools', 'python'))
import google.httpd_utils

_LOG_PREFIX = 'pywebsocket.log-'

_DEFAULT_WS_PORT = 8880
_DEFAULT_ROOT = '.'


def RemoveLogFiles(folder, starts_with):
  files = os.listdir(folder)
  for file in files:
    if file.startswith(starts_with) :
      full_path = os.path.join(folder, file)
      os.remove(full_path)

class PyWebSocketNotStarted(Exception):
  pass

class PyWebSocket(http_server.Lighttpd):
  def __init__(self, output_dir, background=False, port=_DEFAULT_WS_PORT,
               root=_DEFAULT_ROOT):
    """Args:
      output_dir: the absolute path to the layout test result directory
    """
    self._output_dir = output_dir
    self._process = None
    self._port = port
    self._root = root
    if self._port:
      self._port = int(self._port)
    # Webkit tests
    try:
      self._webkit_tests = path_utils.PathFromBase(
          'third_party', 'WebKit', 'LayoutTests', 'websocket', 'tests')
    except path_utils.PathNotFound:
      self._webkit_tests = None

  def Start(self):
    if not self._webkit_tests:
      logging.info('No need to start PyWebSocket server.')
      return
    if self.IsRunning():
      raise PyWebSocketNotStarted('PyWebSocket is already running.')

    time_str = time.strftime('%d%b%Y-%H%M%S')
    log_file_name = _LOG_PREFIX + time_str + '.txt'
    error_log = os.path.join(self._output_dir, log_file_name)

    # Remove old log files. We only need to keep the last ones.
    RemoveLogFiles(self._output_dir, _LOG_PREFIX)

    python_interp = sys.executable
    pywebsocket_base = path_utils.PathFromBase('third_party', 'pywebsocket')
    pywebsocket_script = path_utils.PathFromBase(
        'third_party', 'pywebsocket', 'mod_pywebsocket', 'standalone.py')
    start_cmd = [
        python_interp, pywebsocket_script,
        '-p', str(self._port),
        '-d', self._webkit_tests,
    ]

    # Put the cygwin directory first in the path to find cygwin1.dll
    env = os.environ
    if sys.platform in ('cygwin', 'win32'):
      env['PATH'] = '%s;%s' % (
          path_utils.PathFromBase('third_party', 'cygwin', 'bin'),
          env['PATH'])

    if sys.platform == 'win32' and self._register_cygwin:
      setup_mount = path_utils.PathFromBase('third_party', 'cygwin',
          'setup_mount.bat')
      subprocess.Popen(setup_mount).wait()

    env['PYTHONPATH'] = pywebsocket_base + os.path.pathsep + env['PYTHONPATH']

    logging.info('Starting PyWebSocket server.')
    self._process = subprocess.Popen(start_cmd, env=env)

    # Wait a bit before checking the liveness of the server.
    time.sleep(0.5)

    url = 'http://127.0.0.1:%d/' % self._port
    if not google.httpd_utils.UrlIsAlive(url):
      raise PyWebSocketNotStarted(
          'Failed to start PyWebSocket server on port %s.' % self._port)

    # Our process terminated already
    if self._process.returncode != None:
      raise PyWebSocketNotStarted('Failed to start PyWebSocket server.')

  def Stop(self, force=False):
    if not force and not self.IsRunning():
      return

    logging.info('Shutting down PyWebSocket server.')
    platform_utils.KillProcess(self._process.pid)
    self._process = None

    # Wait a bit to make sure the ports are free'd up
    time.sleep(2)


if '__main__' == __name__:
  # Provide some command line params for starting the PyWebSocket server
  # manually.
  option_parser = optparse.OptionParser()
  option_parser.add_option('-p', '--port', dest='port',
                           default=_DEFAULT_WS_PORT, help='Port to listen on')
  option_parser.add_option('-r', '--root', dest='root', default='.',
                           help='Absolute path to DocumentRoot')
  options, args = option_parser.parse_args()

  pywebsocket = PyWebSocket(tempfile.gettempdir(),
                            port=options.port,
                            root=options.root)
  pywebsocket.Start()
