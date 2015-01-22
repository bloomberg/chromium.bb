# Copyright 2014 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import BaseHTTPServer
import hashlib
import json
import logging
import threading
import urllib2
import urlparse

ALGO = hashlib.sha1


def hash_content(content):
  return ALGO(content).hexdigest()


class IsolateServerHandler(BaseHTTPServer.BaseHTTPRequestHandler):
  """An extremely minimal implementation of the isolate server API v1.0."""

  def _json(self, data):
    """Sends a JSON response."""
    self.send_response(200)
    self.send_header('Content-type', 'application/json')
    self.end_headers()
    json.dump(data, self.wfile)

  def _octet_stream(self, data):
    """Sends a binary response."""
    self.send_response(200)
    self.send_header('Content-type', 'application/octet-stream')
    self.end_headers()
    self.wfile.write(data)

  def _read_body(self):
    """Reads the request body."""
    return self.rfile.read(int(self.headers['Content-Length']))

  def _drop_body(self):
    """Reads the request body."""
    size = int(self.headers['Content-Length'])
    while size:
      chunk = min(4096, size)
      self.rfile.read(chunk)
      size -= chunk

  def do_GET(self):
    logging.info('GET %s', self.path)
    if self.path in ('/on/load', '/on/quit'):
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
    body = self._read_body()
    if self.path == '/content-gs/handshake':
      self._json(
          {
            'access_token': 'a',
            'protocol_version': '1.0',
            'server_app_version': '123-abc',
          })
    elif self.path.startswith('/content-gs/pre-upload/'):
      parts = urlparse.urlparse(self.path)
      namespace = parts.path[len('/content-gs/pre-upload/'):]
      if parts.query != 'token=a':
        raise ValueError('Bad token')
      def process_entry(entry):
        """Converts a {'h', 's', 'i'} to ["<upload url>", "<finalize url>"] or
        None.
        """
        if entry['h'] in self.server.contents.get(namespace, {}):
          return None
        # Don't use finalize url for the mock.
        return [
          '%s/mockimpl/push/%s/%s' % (self.server.url, namespace, entry['h']),
          None,
        ]
      out = [process_entry(i) for i in json.loads(body)]
      logging.info('Returning %s' % out)
      self._json(out)
    else:
      raise NotImplementedError(self.path)

  def do_PUT(self):
    if self.server.discard_content:
      body = '<skipped>'
      self._drop_body()
    else:
      body = self._read_body()
    if self.path.startswith('/mockimpl/push/'):
      namespace, h = self.path[len('/mockimpl/push/'):].split('/', 1)
      self.server.contents.setdefault(namespace, {})[h] = body
      self._octet_stream('')
    else:
      raise NotImplementedError(self.path)

  def log_message(self, fmt, *args):
    logging.info(
        '%s - - [%s] %s', self.address_string(), self.log_date_time_string(),
        fmt % args)


class MockIsolateServer(object):
  def __init__(self):
    self._closed = False
    self._server = BaseHTTPServer.HTTPServer(
        ('127.0.0.1', 0), IsolateServerHandler)
    self._server.contents = {}
    self._server.discard_content = False
    self._server.url = self.url = 'http://localhost:%d' % (
        self._server.server_port)
    self._thread = threading.Thread(target=self._run, name='httpd')
    self._thread.daemon = True
    self._thread.start()
    logging.info('%s', self.url)

  def discard_content(self):
    """Stops saving content in memory. Used to test large files."""
    self._server.discard_content = True

  @property
  def contents(self):
    return self._server.contents

  def add_content(self, namespace, content):
    assert not self._server.discard_content
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
