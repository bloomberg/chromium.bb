# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import BaseHTTPServer
import hashlib
import json
import logging
import threading
import urllib2

ALGO = hashlib.sha1


def hash_content(content):
  return ALGO(content).hexdigest()


class IsolateServerHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  """An extremely minimal implementation of the isolate server API."""
  def _json(self, data):
    self.send_response(200)
    self.send_header('Content-type', 'application/json')
    self.end_headers()
    json.dump(data, self.wfile)

  def _octet_stream(self, data):
    self.send_response(200)
    self.send_header('Content-type', 'application/octet-stream')
    self.end_headers()
    self.wfile.write(data)

  def do_GET(self):
    logging.info('GET %s', self.path)
    if self.path == '/on/quit':
      self._octet_stream('')
    elif self.path == '/auth/api/v1/server/oauth_config':
      self._json(
          {
            'client_id': 'c',
            'client_not_so_secret': 's',
            'primary_url': self.server.url,
          })
    elif self.path == '/auth/api/v1/accounts/self':
      self._json({'identity': 'user:joe', 'xsrf_token': 'foo'})
    elif self.path.startswith('/content-gs/retrieve/'):
      namespace, h = self.path[len('/content-gs/retrieve/'):].split('/', 1)
      self._octet_stream(self.server.contents[namespace][h])
    else:
      raise NotImplementedError(self.path)

  def do_POST(self):
    raise NotImplementedError(self.path)

  def do_PUT(self):
    raise NotImplementedError(self.path)

  def log_message(self, fmt, *args):
    logging.info(
        '%s - - [%s] %s\n', self.address_string(), self.log_date_time_string(),
        fmt % args)


class MockIsolateServer(object):
  def __init__(self):
    self._closed = False
    self._server = BaseHTTPServer.HTTPServer(
        ('127.0.0.1', 0), IsolateServerHandler)
    self._server.contents = {}
    self._server.url = self.url = 'http://%s:%d' % (
        self._server.server_name, self._server.server_port)
    self._thread = threading.Thread(target=self._run, name='httpd')
    self._thread.daemon = True
    self._thread.start()

  def add_content(self, namespace, content):
    h = hash_content(content)
    logging.info('add_content(%s, %s)', namespace, h)
    self._server.contents.setdefault(namespace, {})[h] = content
    return h

  def close(self):
    self.close_start()
    self.close_end()

  def close_start(self):
    assert not self._closed
    self._closed = True
    urllib2.urlopen(self.url + '/on/quit')

  def close_end(self):
    assert self._closed
    self._thread.join()

  def _run(self):
    while not self._closed:
      self._server.handle_request()
