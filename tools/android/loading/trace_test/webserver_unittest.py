# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import socket
import sys
import unittest

_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..', '..', '..'))
sys.path.append(os.path.join(_SRC_DIR, 'tools', 'android', 'loading'))

import options
from trace_test import webserver_test


OPTIONS = options.OPTIONS


class TracingTrackTestCase(unittest.TestCase):
  def setUp(self):
    OPTIONS.ParseArgs('', extra=[('--noisy', False)])

  def testWebserver(self):
    with webserver_test.TemporaryDirectory() as temp_dir:
      test_html = file(os.path.join(temp_dir, 'test.html'), 'w')
      test_html.write('<!DOCTYPE html><html><head><title>Test</title></head>'
                      '<body><h1>Test Page</h1></body></html>')
      test_html.close()

      server = webserver_test.WebServer(temp_dir, temp_dir)
      server.Start()
      sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      host, port = server.Address().split(':')
      sock.connect((host, int(port)))
      sock.sendall('GET null HTTP/1.1\n\n')
      data = sock.recv(4096)
      self.assertTrue(data.startswith('HTTP/1.0 404 Not Found'))
      sock.close()

      sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      sock.connect((host, int(port)))
      sock.sendall('GET test.html HTTP/1.1\n\n')
      data = sock.recv(4096)
      self.assertTrue('HTTP/1.0 200 OK' in data)

      sock.close()
      self.assertTrue(server.Stop())


if __name__ == '__main__':
  unittest.main()
