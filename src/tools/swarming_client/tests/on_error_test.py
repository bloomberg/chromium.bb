#!/usr/bin/env vpython3
# Copyright 2014 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import atexit
import cgi
import getpass
import json
import logging
import os
import platform
import re
import socket
import ssl
import subprocess
import sys
import threading
import unittest

import six
from six.moves import BaseHTTPServer

# Mutates sys.path.
import test_env

# third_party/
from depot_tools import auto_stub
from six.moves import urllib

from utils import on_error


PEM = os.path.join(test_env.TESTS_DIR, 'self_signed.pem')


def _serialize_env():
  return dict((six.ensure_text(k),
               six.ensure_text(v.encode('ascii', 'replace')))
              for k, v in os.environ.items())


class HttpsServer(BaseHTTPServer.HTTPServer):
  def __init__(self, addr, cls, hostname, pem):
    BaseHTTPServer.HTTPServer.__init__(self, addr, cls)
    self.hostname = hostname
    self.pem = pem
    self.socket = ssl.wrap_socket(
        self.socket,
        server_side=True,
        certfile=self.pem)
    self.keep_running = True
    self.requests = []
    self._thread = None

  @property
  def url(self):
    return 'https://%s:%d' % (self.hostname, self.server_address[1])

  def start(self):
    assert not self._thread

    def _server_loop():
      while self.keep_running:
        self.handle_request()

    self._thread = threading.Thread(name='http', target=_server_loop)
    self._thread.daemon = True
    self._thread.start()

    while True:
      # Ensures it is up.
      try:
        urllib.request.urlopen(self.url + '/_warmup').read()
      except IOError:
        continue
      return

  def stop(self):
    self.keep_running = False
    urllib.request.urlopen(self.url + '/_quit').read()
    self._thread.join()
    self._thread = None

  def register_call(self, request):
    if request.path not in ('/_quit', '/_warmup'):
      self.requests.append((request.path, request.parse_POST()))


class Handler(BaseHTTPServer.BaseHTTPRequestHandler):
  def log_message(self, fmt, *args):  # pylint: disable=arguments-differ
    logging.debug(
        '%s - - [%s] %s',
        self.address_string(), self.log_date_time_string(), fmt % args)

  def parse_POST(self):
    ctype, pdict = cgi.parse_header(self.headers['Content-Type'])
    if ctype == 'multipart/form-data':
      return cgi.parse_multipart(self.rfile, pdict)
    if ctype == 'application/x-www-form-urlencoded':
      length = int(self.headers['Content-Length'])
      return urllib.parse.parse_qs(self.rfile.read(length), True)
    if ctype in ('application/json', 'application/json; charset=utf-8'):
      length = int(self.headers['Content-Length'])
      return json.loads(self.rfile.read(length))
    assert False, ctype
    return None

  def do_GET(self):
    self.server.register_call(self)
    self.send_response(200)
    self.send_header('Content-type', 'text/plain')
    self.end_headers()
    self.wfile.write(b'Rock on')

  def do_POST(self):
    self.server.register_call(self)
    self.send_response(200)
    self.send_header('Content-type', 'application/json; charset=utf-8')
    self.end_headers()
    data = {
      'id': '1234',
      'url': 'https://localhost/error/1234',
    }
    self.wfile.write(json.dumps(data).encode())


def start_server():
  """Starts an HTTPS web server and returns the port bound."""
  # A premade passwordless self-signed certificate. It works because older
  # urllib doesn't verify the certificate validity. Disable SSL certificate
  # verification for more recent version.
  create_unverified_https_context = getattr(
      ssl, '_create_unverified_context', None)
  # pylint: disable=using-constant-test
  if create_unverified_https_context:
    ssl._create_default_https_context = create_unverified_https_context
  httpd = HttpsServer(('127.0.0.1', 0), Handler, 'localhost', pem=PEM)
  httpd.start()
  return httpd


class OnErrorBase(auto_stub.TestCase):
  HOSTNAME = socket.getfqdn()

  def setUp(self):
    super(OnErrorBase, self).setUp()
    os.chdir(test_env.TESTS_DIR)
    self._atexit = []
    self.mock(atexit, 'register', self._atexit.append)
    self.mock(on_error, '_HOSTNAME', None)
    self.mock(on_error, '_SERVER', None)
    self.mock(on_error, '_is_in_test', lambda: False)


class OnErrorTest(OnErrorBase):
  def test_report(self):
    url = 'https://localhost/'
    on_error.report_on_exception_exit(url)
    self.assertEqual([on_error._check_for_exception_on_exit], self._atexit)
    self.assertEqual('https://localhost', on_error._SERVER.urlhost)
    self.assertEqual(self.HOSTNAME, on_error._HOSTNAME)
    with self.assertRaises(ValueError):
      on_error.report_on_exception_exit(url)

  def test_no_http(self):
    # http:// url are denied.
    url = 'http://localhost/'
    self.assertIs(False, on_error.report_on_exception_exit(url))
    self.assertEqual([], self._atexit)


class OnErrorServerTest(OnErrorBase):
  def call(self, url, arg, returncode):
    cmd = [sys.executable, '-u', 'main.py', url, arg]
    logging.info('Running: %s', ' '.join(cmd))
    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=os.environ,
        universal_newlines=True,
        cwd=os.path.join(test_env.TESTS_DIR, 'on_error'))
    out = proc.communicate()[0]
    logging.debug('\n%s', out)
    self.assertEqual(returncode, proc.returncode)
    return out

  def one_request(self, httpd):
    self.assertEqual(1, len(httpd.requests))
    resource, params = httpd.requests[0]
    self.assertEqual('/ereporter2/api/v1/on_error', resource)
    self.assertEqual(['r', 'v'], list(params.keys()))
    self.assertEqual('1', params['v'])
    return params['r']

  def test_shell_out_hacked(self):
    # Rerun itself, report an error, ensure the error was reported.
    httpd = start_server()
    out = self.call(httpd.url, 'hacked', 0)
    self.assertEqual([], httpd.requests)
    self.assertEqual('', out)
    httpd.stop()

  def test_shell_out_report(self):
    # Rerun itself, report an error manually, ensure the error was reported.
    httpd = start_server()
    out = self.call(httpd.url, 'report', 0)
    expected = (
        'Sending the report ... done.\n'
        'Report URL: https://localhost/error/1234\n'
        'Oh dang\n')
    self.assertEqual(expected, out)

    actual = self.one_request(httpd)
    self.assertGreater(actual.pop('duration'), 0.000001)
    expected = {
        u'args': [u'main.py', six.text_type(httpd.url), u'report'],
        u'category': u'report',
        u'cwd': os.path.join(test_env.TESTS_DIR, 'on_error'),
        u'env': _serialize_env(),
        u'hostname': six.text_type(socket.getfqdn()),
        u'message': u'Oh dang',
        u'os': six.text_type(sys.platform),
        u'python_version': six.text_type(platform.python_version()),
        u'source': u'main.py',
        u'stack': u'None' if six.PY2 else 'NoneType: None',
        u'user': six.text_type(getpass.getuser()),
        # The version was added dynamically for testing purpose.
        u'version': u'123',
    }
    self.assertEqual(expected, actual)
    httpd.stop()

  def test_shell_out_exception(self):
    # Rerun itself, report an exception manually, ensure the error was reported.
    httpd = start_server()
    out = self.call(httpd.url, 'exception', 0)
    expected = (
        'Sending the crash report ... done.\n'
        'Report URL: https://localhost/error/1234\n'
        'Really\nYou are not my type\n')
    self.assertEqual(expected, out)

    actual = self.one_request(httpd)
    self.assertGreater(actual.pop('duration'), 0.000001)
    # Remove numbers so editing the code doesn't invalidate the expectation.
    actual['stack'] = re.sub(r' \d+', ' 0', actual['stack'])
    expected = {
        u'args': [u'main.py',
                  six.text_type(httpd.url), u'exception'],
        u'cwd': os.path.join(test_env.TESTS_DIR, 'on_error'),
        u'category': u'exception',
        u'env': _serialize_env(),
        u'exception_type': u'TypeError',
        u'hostname': six.text_type(socket.getfqdn()),
        u'message': u'Really\nYou are not my type',
        u'os': six.text_type(sys.platform),
        u'python_version': six.text_type(platform.python_version()),
        u'source': u'main.py',
        u'stack': u'Traceback (most recent call last):\n'
                  u'  File "main.py", line 0, in run_shell_out\n'
                  u'    raise TypeError(\'You are not my type\')\n'
                  u'TypeError: You are not my type',
        u'user': six.text_type(getpass.getuser()),
    }
    self.assertEqual(expected, actual)
    httpd.stop()

  def test_shell_out_exception_no_msg(self):
    # Rerun itself, report an exception manually, ensure the error was reported.
    httpd = start_server()
    out = self.call(httpd.url, 'exception_no_msg', 0)
    expected = (
        'Sending the crash report ... done.\n'
        'Report URL: https://localhost/error/1234\n'
        'You are not my type #2\n')
    self.assertEqual(expected, out)

    actual = self.one_request(httpd)
    self.assertGreater(actual.pop('duration'), 0.000001)
    # Remove numbers so editing the code doesn't invalidate the expectation.
    actual['stack'] = re.sub(r' \d+', ' 0', actual['stack'])
    expected = {
        u'args': [u'main.py',
                  six.text_type(httpd.url), u'exception_no_msg'],
        u'category': u'exception',
        u'cwd': os.path.join(test_env.TESTS_DIR, 'on_error'),
        u'env': _serialize_env(),
        u'exception_type': u'TypeError',
        u'hostname': six.text_type(socket.getfqdn()),
        u'message': u'You are not my type #2',
        u'os': six.text_type(sys.platform),
        u'python_version': six.text_type(platform.python_version()),
        u'source': u'main.py',
        u'stack': u'Traceback (most recent call last):\n'
                  u'  File "main.py", line 0, in run_shell_out\n'
                  u'    raise TypeError(\'You are not my type #2\')\n'
                  u'TypeError: You are not my type #2',
        u'user': six.text_type(getpass.getuser()),
    }
    self.assertEqual(expected, actual)
    httpd.stop()

  def test_shell_out_crash(self):
    # Rerun itself, report an error with a crash, ensure the error was reported.
    httpd = start_server()
    out = self.call(httpd.url, 'crash', 1)
    expected = (
        u'Traceback (most recent call last):\n'
        u'  File "main.py", line 0, in <module>\n'
        u'    sys.exit(run_shell_out(*sys.argv[1:]))\n'
        u'  File "main.py", line 0, in run_shell_out\n'
        u'    raise ValueError(\'Oops\')\n'
        u'ValueError: Oops\n'
        u'Sending the crash report ... done.\n'
        u'Report URL: https://localhost/error/1234\n'
        u'Process exited due to exception\n'
        u'Oops\n')
    # Remove numbers so editing the code doesn't invalidate the expectation.
    self.assertEqual(expected, re.sub(r' \d+', ' 0', out))

    actual = self.one_request(httpd)
    # Remove numbers so editing the code doesn't invalidate the expectation.
    actual['stack'] = re.sub(r' \d+', ' 0', actual['stack'])
    self.assertGreater(actual.pop('duration'), 0.000001)
    expected = {
        u'args': [u'main.py', six.text_type(httpd.url), u'crash'],
        u'category': u'exception',
        u'cwd': os.path.join(test_env.TESTS_DIR, 'on_error'),
        u'env': _serialize_env(),
        u'exception_type': u'ValueError',
        u'hostname': six.text_type(socket.getfqdn()),
        u'message': u'Process exited due to exception\nOops',
        u'os': six.text_type(sys.platform),
        u'python_version': six.text_type(platform.python_version()),
        u'source': u'main.py',
        # The stack trace is stripped off the heading and absolute paths.
        u'stack': u'File "main.py", line 0, in <module>\n'
                  u'  sys.exit(run_shell_out(*sys.argv[1:]))\n'
                  u'File "main.py", line 0, in run_shell_out\n'
                  u'  raise ValueError(\'Oops\')',
        u'user': six.text_type(getpass.getuser()),
    }
    self.assertEqual(expected, actual)
    httpd.stop()

  def test_shell_out_crash_server_down(self):
    # Rerun itself, report an error, ensure the error was reported.
    out = self.call('https://localhost:1', 'crash', 1)
    expected = (
        u'Traceback (most recent call last):\n'
        u'  File "main.py", line 0, in <module>\n'
        u'    sys.exit(run_shell_out(*sys.argv[1:]))\n'
        u'  File "main.py", line 0, in run_shell_out\n'
        u'    raise ValueError(\'Oops\')\n'
        u'ValueError: Oops\n'
        u'Sending the crash report ... failed!\n'
        u'Process exited due to exception\n'
        u'Oops\n')
    # Remove numbers so editing the code doesn't invalidate the expectation.
    self.assertEqual(expected, re.sub(r' \d+', ' 0', out))


if __name__ == '__main__':
  # Ignore _DISABLE_ENVVAR if set.
  os.environ.pop(on_error._DISABLE_ENVVAR, None)
  test_env.main()
