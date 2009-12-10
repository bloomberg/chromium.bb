#!/usr/bin/env python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to help start/stop the lighttpd server used by layout tests."""


import logging
import optparse
import os
import shutil
import subprocess
import sys
import tempfile
import time
import urllib

import http_server_base
import path_utils

# So we can import httpd_utils below to make ui_tests happy.
sys.path.append(path_utils.PathFromBase('tools', 'python'))
import google.httpd_utils

def RemoveLogFiles(folder, starts_with):
  files = os.listdir(folder)
  for file in files:
    if file.startswith(starts_with) :
      full_path = os.path.join(folder, file)
      os.remove(full_path)

class Lighttpd(http_server_base.HttpServerBase):
  # Webkit tests
  try:
    _webkit_tests = path_utils.PathFromBase('third_party', 'WebKit',
                                            'LayoutTests', 'http', 'tests')
    _js_test_resource = path_utils.PathFromBase('third_party', 'WebKit',
                                                'LayoutTests', 'fast',
                                                'js', 'resources')
  except path_utils.PathNotFound:
    # If third_party/WebKit/LayoutTests/http/tests does not exist, assume wekit
    # tests are located in webkit/data/layout_tests/LayoutTests/http/tests.
    try:
      _webkit_tests = path_utils.PathFromBase('webkit', 'data', 'layout_tests',
                                              'LayoutTests', 'http', 'tests')
      _js_test_resource = path_utils.PathFromBase('webkit', 'data',
                                                  'layout_tests', 'LayoutTests',
                                                  'fast', 'js', 'resources')
    except path_utils.PathNotFound:
      _webkit_tests = None
      _js_test_resource = None

  # New tests for Chrome
  try:
    _pending_tests = path_utils.PathFromBase('webkit', 'data', 'layout_tests',
                                             'pending', 'http', 'tests')
  except path_utils.PathNotFound:
    _pending_tests = None

  # Path where we can access all of the tests
  _all_tests = path_utils.PathFromBase('webkit', 'data', 'layout_tests')
  # Self generated certificate for SSL server (for client cert get
  # <base-path>\chrome\test\data\ssl\certs\root_ca_cert.crt)
  _pem_file = path_utils.PathFromBase('tools', 'python', 'google',
      'httpd_config', 'httpd2.pem')
  VIRTUALCONFIG = [
    # One mapping where we can get to everything
    {'port': 8081, 'docroot': _all_tests}
  ]

  if _webkit_tests:
    VIRTUALCONFIG.extend(
      # Three mappings (one with SSL enabled) for LayoutTests http tests
      [{'port': 8000, 'docroot': _webkit_tests},
       {'port': 8080, 'docroot': _webkit_tests},
       {'port': 8443, 'docroot': _webkit_tests, 'sslcert': _pem_file}]
    )
  if _pending_tests:
    VIRTUALCONFIG.extend(
      # Three similar mappings (one with SSL enabled) for pending http tests
      [{'port': 9000, 'docroot': _pending_tests},
       {'port': 9080, 'docroot': _pending_tests},
       {'port': 9443, 'docroot': _pending_tests, 'sslcert': _pem_file}]
    )


  def __init__(self, output_dir, background=False, port=None,
               root=None, register_cygwin=None, run_background=None):
    """Args:
      output_dir: the absolute path to the layout test result directory
    """
    self._output_dir = output_dir
    self._process = None
    self._port = port
    self._root = root
    self._register_cygwin = register_cygwin
    self._run_background = run_background
    if self._port:
      self._port = int(self._port)

  def IsRunning(self):
    return self._process != None

  def Start(self):
    if self.IsRunning():
      raise 'Lighttpd already running'

    base_conf_file = path_utils.PathFromBase('webkit',
        'tools','layout_tests','layout_package', 'lighttpd.conf')
    out_conf_file = os.path.join(self._output_dir, 'lighttpd.conf')
    time_str = time.strftime("%d%b%Y-%H%M%S")
    access_file_name = "access.log-" + time_str + ".txt"
    access_log = os.path.join(self._output_dir, access_file_name)
    log_file_name = "error.log-" + time_str + ".txt"
    error_log = os.path.join(self._output_dir, log_file_name)

    # Remove old log files. We only need to keep the last ones.
    RemoveLogFiles(self._output_dir, "access.log-")
    RemoveLogFiles(self._output_dir, "error.log-")

    # Write out the config
    f = file(base_conf_file, 'rb')
    base_conf = f.read()
    f.close()

    f = file(out_conf_file, 'wb')
    f.write(base_conf)

    # Write out our cgi handlers.  Run perl through env so that it processes
    # the #! line and runs perl with the proper command line arguments.
    # Emulate apache's mod_asis with a cat cgi handler.
    f.write(('cgi.assign = ( ".cgi"  => "/usr/bin/env",\n'
             '               ".pl"   => "/usr/bin/env",\n'
             '               ".asis" => "/bin/cat",\n'
             '               ".php"  => "%s" )\n\n') %
                                 path_utils.LigHTTPdPHPPath())

    # Setup log files
    f.write(('server.errorlog = "%s"\n'
             'accesslog.filename = "%s"\n\n') % (error_log, access_log))

    # Setup upload folders. Upload folder is to hold temporary upload files
    # and also POST data. This is used to support XHR layout tests that does
    # POST.
    f.write(('server.upload-dirs = ( "%s" )\n\n') % (self._output_dir))

    # Setup a link to where the js test templates are stored
    f.write(('alias.url = ( "/js-test-resources" => "%s" )\n\n') %
                (self._js_test_resource))

    # dump out of virtual host config at the bottom.
    if self._root:
      if self._port:
        # Have both port and root dir.
        mappings = [{'port': self._port, 'docroot': self._root}]
      else:
        # Have only a root dir - set the ports as for LayoutTests.
        # This is used in ui_tests to run http tests against a browser.
        mappings = [
          # default set of ports as for LayoutTests but with a specified root.
          {'port': 8000, 'docroot': self._root},
          {'port': 8080, 'docroot': self._root},
          {'port': 8443, 'docroot': self._root, 'sslcert': Lighttpd._pem_file}
        ]
    else:
      mappings = self.VIRTUALCONFIG
    for mapping in mappings:
      ssl_setup = ''
      if 'sslcert' in mapping:
        ssl_setup = ('  ssl.engine = "enable"\n'
                     '  ssl.pemfile = "%s"\n' % mapping['sslcert'])

      f.write(('$SERVER["socket"] == "127.0.0.1:%d" {\n'
               '  server.document-root = "%s"\n' +
               ssl_setup +
               '}\n\n') % (mapping['port'], mapping['docroot']))
    f.close()

    executable = path_utils.LigHTTPdExecutablePath()
    module_path = path_utils.LigHTTPdModulePath()
    start_cmd = [ executable,
                  # Newly written config file
                  '-f', path_utils.PathFromBase(self._output_dir,
                                                'lighttpd.conf'),
                  # Where it can find its module dynamic libraries
                  '-m', module_path ]

    if not self._run_background:
      start_cmd.append(# Don't background
                       '-D')

    # Copy liblightcomp.dylib to /tmp/lighttpd/lib to work around the bug that
    # mod_alias.so loads it from the hard coded path.
    if sys.platform == 'darwin':
      tmp_module_path = '/tmp/lighttpd/lib'
      if not os.path.exists(tmp_module_path):
        os.makedirs(tmp_module_path)
      lib_file = 'liblightcomp.dylib'
      shutil.copyfile(os.path.join(module_path, lib_file),
                      os.path.join(tmp_module_path, lib_file))

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

    logging.debug('Starting http server')
    self._process = subprocess.Popen(start_cmd, env=env)

    # Wait for server to start.
    self.mappings = mappings
    server_started = self.WaitForAction(self.IsServerRunningOnAllPorts)

    # Our process terminated already
    if not server_started or self._process.returncode != None:
      raise google.httpd_utils.HttpdNotStarted('Failed to start httpd.')

    logging.debug("Server successfully started")

  # TODO(deanm): Find a nicer way to shutdown cleanly.  Our log files are
  # probably not being flushed, etc... why doesn't our python have os.kill ?
  def Stop(self, force=False):
    if not force and not self.IsRunning():
      return

    httpd_pid = None
    if self._process:
      httpd_pid = self._process.pid
    path_utils.ShutDownHTTPServer(httpd_pid)

    if self._process:
      self._process.wait()
      self._process = None

if '__main__' == __name__:
  # Provide some command line params for starting/stopping the http server
  # manually. Also used in ui_tests to run http layout tests in a browser.
  option_parser = optparse.OptionParser()
  option_parser.add_option('-k', '--server', help='Server action (start|stop)')
  option_parser.add_option('-p', '--port',
      help='Port to listen on (overrides layout test ports)')
  option_parser.add_option('-r', '--root',
      help='Absolute path to DocumentRoot (overrides layout test roots)')
  option_parser.add_option('--register_cygwin', action="store_true",
      dest="register_cygwin", help='Register Cygwin paths (on Win try bots)')
  option_parser.add_option('--run_background', action="store_true",
      dest="run_background", help='Run on background (for running as UI test)')
  options, args = option_parser.parse_args()

  if not options.server:
    print ('Usage: %s --server {start|stop} [--root=root_dir]'
           ' [--port=port_number]' % sys.argv[0])
  else:
    if (options.root is None) and (options.port is not None):
      # specifying root but not port means we want httpd on default set of
      # ports that LayoutTest use, but pointing to a different source of tests.
      # Specifying port but no root does not seem meaningful.
      raise 'Specifying port requires also a root.'
    httpd = Lighttpd(tempfile.gettempdir(),
                     port=options.port,
                     root=options.root,
                     register_cygwin=options.register_cygwin,
                     run_background=options.run_background)
    if 'start' == options.server:
      httpd.Start()
    else:
      httpd.Stop(force=True)
