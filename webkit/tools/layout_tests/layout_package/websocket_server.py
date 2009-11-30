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

_WS_LOG_PREFIX = 'pywebsocket.ws.log-'
_WSS_LOG_PREFIX = 'pywebsocket.wss.log-'

_DEFAULT_WS_PORT = 8880
_DEFAULT_WSS_PORT = 9323


def RemoveLogFiles(folder, starts_with):
  files = os.listdir(folder)
  for file in files:
    if file.startswith(starts_with) :
      full_path = os.path.join(folder, file)
      os.remove(full_path)


class PyWebSocketNotStarted(Exception):
  pass


class PyWebSocketNotFound(Exception):
  pass


class PyWebSocket(http_server.Lighttpd):
  def __init__(self, output_dir, port=_DEFAULT_WS_PORT,
               root=None,
               use_tls=False,
               private_key=http_server.Lighttpd._pem_file,
               certificate=http_server.Lighttpd._pem_file,
               register_cygwin=None,
               pidfile=None):
    """Args:
      output_dir: the absolute path to the layout test result directory
    """
    http_server.Lighttpd.__init__(self, output_dir,
                                  port=port,
                                  root=root,
                                  register_cygwin=register_cygwin)
    self._output_dir = output_dir
    self._process = None
    self._port = port
    self._root = root
    self._use_tls = use_tls
    self._private_key = private_key
    self._certificate = certificate
    if self._port:
      self._port = int(self._port)
    if self._use_tls:
      self._server_name = 'PyWebSocket(Secure)'
    else:
      self._server_name = 'PyWebSocket'
    self._pidfile = pidfile

    # Webkit tests
    if self._root:
      self._layout_tests = os.path.abspath(self._root)
      self._web_socket_tests = os.path.abspath(
          os.path.join(self._root, 'websocket', 'tests'))
    else:
      try:
        self._web_socket_tests = path_utils.PathFromBase(
            'third_party', 'WebKit', 'LayoutTests', 'websocket', 'tests')
        self._layout_tests = path_utils.PathFromBase(
            'third_party', 'WebKit', 'LayoutTests')
      except path_utils.PathNotFound:
        self._web_socket_tests = None

  def Start(self):
    if not self._web_socket_tests:
      logging.info('No need to start %s server.' % self._server_name)
      return
    if self.IsRunning():
      raise PyWebSocketNotStarted('%s is already running.' % self._server_name)

    time_str = time.strftime('%d%b%Y-%H%M%S')
    if self._use_tls:
      log_prefix = _WSS_LOG_PREFIX
    else:
      log_prefix = _WS_LOG_PREFIX
    log_file_name = log_prefix + time_str

    # Remove old log files. We only need to keep the last ones.
    RemoveLogFiles(self._output_dir, log_prefix)

    error_log = os.path.join(self._output_dir, log_file_name + "-err.txt")

    output_log = os.path.join(self._output_dir, log_file_name + "-out.txt")
    self._wsout = open(output_log, "w")

    python_interp = sys.executable
    pywebsocket_base = path_utils.PathFromBase(
        'third_party', 'WebKit', 'WebKitTools', 'pywebsocket')
    pywebsocket_script = path_utils.PathFromBase(
        'third_party', 'WebKit', 'WebKitTools', 'pywebsocket',
        'mod_pywebsocket', 'standalone.py')
    start_cmd = [
        python_interp, pywebsocket_script,
        '-p', str(self._port),
        '-d', self._layout_tests,
        '-s', self._web_socket_tests,
        '-l', error_log,
    ]
    if self._use_tls:
      start_cmd.extend(['-t', '-k', self._private_key,
                        '-c', self._certificate])

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

    env['PYTHONPATH'] = (pywebsocket_base + os.path.pathsep +
                         env.get('PYTHONPATH', ''))

    logging.debug('Starting %s server.' % self._server_name)
    self._process = subprocess.Popen(start_cmd, stdout=self._wsout,
                                     env=env)

    # Wait a bit before checking the liveness of the server.
    time.sleep(0.5)

    if self._use_tls:
      url = 'https'
    else:
      url = 'http'
    url = url + '://127.0.0.1:%d/' % self._port
    if not google.httpd_utils.UrlIsAlive(url):
      raise PyWebSocketNotStarted(
          'Failed to start %s server on port %s.' %
              (self._server_name, self._port))

    # Our process terminated already
    if self._process.returncode != None:
      raise PyWebSocketNotStarted(
          'Failed to start %s server.' % self._server_name)
    if self._pidfile:
      f = open(self._pidfile, 'w')
      f.write("%d" % self._process.pid)
      f.close()

  def Stop(self, force=False):
    if not force and not self.IsRunning():
      return

    if self._process:
      pid = self._process.pid
    elif self._pidfile:
      f = open(self._pidfile)
      pid = int(f.read().strip())
      f.close()

    if not pid:
      raise PyWebSocketNotFound(
          'Failed to find %s server pid.' % self._server_name)

    logging.debug('Shutting down %s server %d.' % (self._server_name, pid))
    platform_utils.KillProcess(pid)

    if self._process:
      self._process.wait()
      self._process = None

    # Wait a bit to make sure the ports are free'd up
    time.sleep(2)
    if self._wsout:
      self._wsout.close()
      self._wsout = None


if '__main__' == __name__:
  # Provide some command line params for starting the PyWebSocket server
  # manually.
  option_parser = optparse.OptionParser()
  option_parser.add_option('--server', type='choice',
                           choices=['start', 'stop'], default='start',
                           help='Server action (start|stop)')
  option_parser.add_option('-p', '--port', dest='port',
                           default=None, help='Port to listen on')
  option_parser.add_option('-r', '--root',
                           help='Absolute path to DocumentRoot '
                                '(overrides layout test roots)')
  option_parser.add_option('-t', '--tls', dest='use_tls', action='store_true',
                           default=False, help='use TLS (wss://)')
  option_parser.add_option('-k', '--private_key', dest='private_key',
                           default='', help='TLS private key file.')
  option_parser.add_option('-c', '--certificate', dest='certificate',
                           default='', help='TLS certificate file.')
  option_parser.add_option('--register_cygwin', action="store_true",
                           dest="register_cygwin",
                           help='Register Cygwin paths (on Win try bots)')
  option_parser.add_option('--pidfile', help='path to pid file.')
  options, args = option_parser.parse_args()

  if not options.port:
    if options.use_tls:
      options.port = _DEFAULT_WSS_PORT
    else:
      options.port = _DEFAULT_WS_PORT

  kwds = {'port':options.port,
          'use_tls':options.use_tls}
  if options.root:
    kwds['root'] = options.root
  if options.private_key:
    kwds['private_key'] = options.private_key
  if options.certificate:
    kwds['certificate'] = options.certificate
  kwds['register_cygwin'] = options.register_cygwin
  if options.pidfile:
    kwds['pidfile'] = options.pidfile

  pywebsocket = PyWebSocket(tempfile.gettempdir(), **kwds)

  if 'start' == options.server:
    pywebsocket.Start()
  else:
    pywebsocket.Stop(force=True)
