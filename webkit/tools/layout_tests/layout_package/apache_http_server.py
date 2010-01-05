# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A class to start/stop the apache http server used by layout tests."""

import logging
import optparse
import os
import re
import subprocess
import sys

import http_server_base
import path_utils
import platform_utils

class LayoutTestApacheHttpd(http_server_base.HttpServerBase):
  def __init__(self, output_dir):
    """Args:
      output_dir: the absolute path to the layout test result directory
    """
    self._output_dir = output_dir
    self._httpd_proc = None
    path_utils.MaybeMakeDirectory(output_dir)

    self.mappings = [
      {'port': 8000},
      {'port': 8080},
      {'port': 8081},
      {'port': 8443, 'sslcert': True}
    ]

    # The upstream .conf file assumed the existence of /tmp/WebKit for placing
    # apache files like the lock file there.
    self._runtime_path = os.path.join("/tmp", "WebKit")
    path_utils.MaybeMakeDirectory(self._runtime_path)

    # The PID returned when Apache is started goes away (due to dropping
    # privileges?). The proper controlling PID is written to a file in the
    # apache runtime directory.
    self._pid_file = os.path.join(self._runtime_path, 'httpd.pid')

    test_dir = path_utils.PathFromBase('third_party', 'WebKit',
        'LayoutTests')
    js_test_resources_dir = self._CygwinSafeJoin(test_dir, "fast", "js",
        "resources")
    mime_types_path = self._CygwinSafeJoin(test_dir, "http", "conf",
        "mime.types")
    cert_file = self._CygwinSafeJoin(test_dir, "http", "conf",
        "webkit-httpd.pem")
    access_log = self._CygwinSafeJoin(output_dir, "access_log.txt")
    error_log = self._CygwinSafeJoin(output_dir, "error_log.txt")
    document_root = self._CygwinSafeJoin(test_dir, "http", "tests")

    executable = platform_utils.ApacheExecutablePath()
    if self._IsCygwin():
      executable = self._GetCygwinPath(executable)

    cmd = [ executable,
        '-f', self._GetApacheConfigFilePath(test_dir, output_dir),
        '-C', "\'DocumentRoot %s\'" % document_root,
        '-c', "\'Alias /js-test-resources %s\'" % js_test_resources_dir,
        '-C', "\'Listen %s\'" % "127.0.0.1:8000",
        '-C', "\'Listen %s\'" % "127.0.0.1:8081",
        '-c', "\'TypesConfig \"%s\"\'" % mime_types_path,
        '-c', "\'CustomLog \"%s\" common\'" % access_log,
        '-c', "\'ErrorLog \"%s\"\'" % error_log,
        '-C', "\'User \"%s\"\'" % os.environ.get("USERNAME",
                                                 os.environ.get("USER", "")),
      ]

    if self._IsCygwin():
      cygbin = path_utils.PathFromBase('third_party', 'cygwin', 'bin')
      # Not entirely sure why, but from cygwin we need to run the httpd command
      # through bash.
      self._start_cmd = [
          os.path.join(cygbin, 'bash.exe'),
          '-c',
          'PATH=%s %s' % (self._GetCygwinPath(cygbin), " ".join(cmd)),
        ]
    else:
      # TODO(ojan): When we get cygwin using Apache 2, use set the cert file
      # for cygwin as well.
      cmd.extend(['-c', "\'SSLCertificateFile %s\'" % cert_file])
      # Join the string here so that Cygwin/Windows and Mac/Linux can use the
      # same code. Otherwise, we could remove the single quotes above and
      # keep cmd as a sequence.
      self._start_cmd = " ".join(cmd)

  def _IsCygwin(self):
    return sys.platform in ("win32", "cygwin")

  def _CygwinSafeJoin(self, *parts):
    """Returns a platform appropriate path."""
    path = os.path.join(*parts)
    if self._IsCygwin():
      return self._GetCygwinPath(path)
    return path

  def _GetCygwinPath(self, path):
    """Convert a Windows path to a cygwin path.

    The cygpath utility insists on converting paths that it thinks are Cygwin
    root paths to what it thinks the correct roots are.  So paths such as
    "C:\b\slave\webkit-release\build\third_party\cygwin\bin" are converted to
    plain "/usr/bin".  To avoid this, we do the conversion manually.

    The path is expected to be an absolute path, on any drive.
    """
    drive_regexp = re.compile(r'([a-z]):[/\\]', re.IGNORECASE)
    def LowerDrive(matchobj):
      return '/cygdrive/%s/' % matchobj.group(1).lower()
    path = drive_regexp.sub(LowerDrive, path)
    return path.replace('\\', '/')

  def _GetApacheConfigFilePath(self, test_dir, output_dir):
    """Returns the path to the apache config file to use.
    Args:
      test_dir: absolute path to the LayoutTests directory.
      output_dir: absolute path to the layout test results directory.
    """
    httpd_config = platform_utils.ApacheConfigFilePath()
    httpd_config_copy = os.path.join(output_dir, "httpd.conf")
    httpd_conf = open(httpd_config).read()
    if self._IsCygwin():
      # This is a gross hack, but it lets us use the upstream .conf file and
      # our checked in cygwin.
      # This tells the server the root directory to look in for .so modules.
      # It will use this path plus the relative paths to the .so files listed
      # in the .conf file. We have apache/cygwin checked into our tree so
      # people don't have to install it into their cygwin.
      cygusr = path_utils.PathFromBase('third_party', 'cygwin', 'usr')
      httpd_conf = httpd_conf.replace('ServerRoot "/usr"',
          'ServerRoot "%s"' % self._GetCygwinPath(cygusr))

    # TODO(ojan): Instead of writing an extra file, checkin a conf file
    # upstream. Or, even better, upstream/delete all our chrome http tests so we
    # don't need this special-cased DocumentRoot and then just use the upstream
    # conf file.
    chrome_document_root = path_utils.PathFromBase('webkit', 'data',
        'layout_tests')
    if self._IsCygwin():
      chrome_document_root = self._GetCygwinPath(chrome_document_root)
    httpd_conf = (httpd_conf +
        self._GetVirtualHostConfig(chrome_document_root, 8081))

    f = open(httpd_config_copy, 'wb')
    f.write(httpd_conf)
    f.close()

    if self._IsCygwin():
      return self._GetCygwinPath(httpd_config_copy)
    return httpd_config_copy

  def _GetVirtualHostConfig(self, document_root, port, ssl=False):
    """Returns a <VirtualHost> directive block for an httpd.conf file.  It will
    listen to 127.0.0.1 on each of the given port.
    """
    return '\n'.join(('<VirtualHost 127.0.0.1:%s>' % port,
                      'DocumentRoot %s' % document_root,
                      ssl and 'SSLEngine On' or '',
                      '</VirtualHost>', ''))

  def _StartHttpdProcess(self):
    """Starts the httpd process and returns whether there were errors."""
    # Use shell=True because we join the arguments into a string for the sake
    # of Window/Cygwin and it needs quoting that breaks shell=False.
    self._httpd_proc = subprocess.Popen(self._start_cmd, stderr=subprocess.PIPE,
        shell=True)
    err = self._httpd_proc.stderr.read()
    if len(err):
      logging.debug(err)
      return False
    return True

  def Start(self):
    """Starts the apache http server."""
    # Stop any currently running servers.
    self.Stop()

    logging.debug("Starting apache http server")
    server_started = self.WaitForAction(self._StartHttpdProcess)
    if server_started:
      logging.debug("Apache started. Testing ports")
      server_started = self.WaitForAction(self.IsServerRunningOnAllPorts)

    if server_started:
      logging.debug("Server successfully started")
    else:
      raise Exception('Failed to start http server')

  def Stop(self):
    """Stops the apache http server."""
    logging.debug("Shutting down any running http servers")
    httpd_pid = None
    if os.path.exists(self._pid_file):
      httpd_pid = int(open(self._pid_file).readline())
    path_utils.ShutDownHTTPServer(httpd_pid)
