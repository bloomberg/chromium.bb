#!/usr/bin/env python
# Copyright 2016 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import contextlib
import json
import logging
import os
import sys
import time
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(
    __file__.decode(sys.getfilesystemencoding()))))
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

from depot_tools import auto_stub
from depot_tools import fix_encoding
from third_party import requests

from utils import auth_server


def global_test_setup():
  # Terminate HTTP server in tests 50x faster. Impacts performance though.
  auth_server._HTTPServer.poll_interval = 0.01


def call_rpc(ctx, scopes):
  r = requests.post(
      url='http://127.0.0.1:%d/rpc/LuciLocalAuthService.GetOAuthToken' %
          ctx['rpc_port'],
      data=json.dumps({
        'scopes': scopes,
        'secret': ctx['secret'],
      }),
      headers={'Content-Type': 'application/json'})
  return r.json()


@contextlib.contextmanager
def local_auth_server(token_cb):
  class MockedProvider(object):
    def generate_token(self, scopes):
      return token_cb(scopes)
  s = auth_server.LocalAuthServer()
  try:
    yield s.start(MockedProvider())
  finally:
    s.stop()


class LocalAuthServerTest(auto_stub.TestCase):
  epoch = 12345678

  def setUp(self):
    super(LocalAuthServerTest, self).setUp()
    self.mock_time(0)

  def mock_time(self, delta):
    self.mock(time, 'time', lambda: self.epoch + delta)

  def test_works(self):
    calls = []
    def token_gen(scopes):
      calls.append(scopes)
      return auth_server.AccessToken('tok', time.time() + 300)

    with local_auth_server(token_gen) as ctx:
      # Grab initial token.
      resp = call_rpc(ctx, ['B', 'B', 'A', 'C'])
      self.assertEqual(
          {u'access_token': u'tok', u'expiry': self.epoch + 300}, resp)
      self.assertEqual([('A', 'B', 'C')], calls)
      del calls[:]

      # Reuses cached token until it is close to expiration.
      self.mock_time(200)
      resp = call_rpc(ctx, ['B', 'A', 'C'])
      self.assertEqual(
          {u'access_token': u'tok', u'expiry': self.epoch + 300}, resp)
      self.assertFalse(calls)

      # Expired. Generated new one.
      self.mock_time(300)
      resp = call_rpc(ctx, ['A', 'B', 'C'])
      self.assertEqual(
          {u'access_token': u'tok', u'expiry': self.epoch + 600}, resp)
      self.assertEqual([('A', 'B', 'C')], calls)

  def test_handles_token_errors(self):
    fatal = False
    code = 123
    def token_gen(_scopes):
      raise auth_server.TokenError(code, 'error message', fatal=fatal)

    with local_auth_server(token_gen) as ctx:
      self.assertEqual(
          {u'error_code': 123, u'error_message': u'error message'},
          call_rpc(ctx, ['B', 'B', 'A', 'C']))

      # Non-fatal errors aren't cached.
      code = 456
      self.assertEqual(
          {u'error_code': 456, u'error_message': u'error message'},
          call_rpc(ctx, ['B', 'B', 'A', 'C']))

      # Fatal errors are cached.
      fatal = True
      code = 789
      self.assertEqual(
          {u'error_code': 789, u'error_message': u'error message'},
          call_rpc(ctx, ['B', 'B', 'A', 'C']))

      # Same cached error.
      code = 111
      self.assertEqual(
          {u'error_code': 789, u'error_message': u'error message'},
          call_rpc(ctx, ['B', 'B', 'A', 'C']))

  def test_http_level_errors(self):
    def token_gen(_scopes):
      self.fail('must not be called')

    with local_auth_server(token_gen) as ctx:
      # Wrong URL.
      r = requests.post(
          url='http://127.0.0.1:%d/blah/LuciLocalAuthService.GetOAuthToken' %
              ctx['rpc_port'],
          data=json.dumps({
            'scopes': ['A', 'B', 'C'],
            'secret': ctx['secret'],
          }),
          headers={'Content-Type': 'application/json'})
      self.assertEqual(404, r.status_code)

      # Wrong HTTP method.
      r = requests.get(
          url='http://127.0.0.1:%d/rpc/LuciLocalAuthService.GetOAuthToken' %
              ctx['rpc_port'],
          data=json.dumps({
            'scopes': ['A', 'B', 'C'],
            'secret': ctx['secret'],
          }),
          headers={'Content-Type': 'application/json'})
      self.assertEqual(501, r.status_code)

      # Wrong content type.
      r = requests.post(
          url='http://127.0.0.1:%d/rpc/LuciLocalAuthService.GetOAuthToken' %
              ctx['rpc_port'],
          data=json.dumps({
            'scopes': ['A', 'B', 'C'],
            'secret': ctx['secret'],
          }),
          headers={'Content-Type': 'application/xml'})
      self.assertEqual(400, r.status_code)

      # Bad JSON.
      r = requests.post(
          url='http://127.0.0.1:%d/rpc/LuciLocalAuthService.GetOAuthToken' %
              ctx['rpc_port'],
          data='not a json',
          headers={'Content-Type': 'application/json'})
      self.assertEqual(400, r.status_code)

  def test_validation(self):
    def token_gen(_scopes):
      self.fail('must not be called')

    with local_auth_server(token_gen) as ctx:
      def must_fail(err, body, code=400):
        r = requests.post(
            url='http://127.0.0.1:%d/rpc/LuciLocalAuthService.GetOAuthToken' %
                ctx['rpc_port'],
            data=json.dumps(body),
            headers={'Content-Type': 'application/json'})
        self.assertEqual(code, r.status_code)
        self.assertIn(err, r.text)

      must_fail('"scopes" is required', {})
      must_fail('"scopes" is required', {'scopes': []})
      must_fail('"scopes" must be a list of strings', {'scopes': 'abc'})
      must_fail('"scopes" must be a list of strings', {'scopes': [1]})

      must_fail('"secret" is required', {'scopes': ['a']})
      must_fail('"secret" must be a string', {'scopes': ['a'], 'secret': 123})

      must_fail(
          'Invalid "secret"',
          {'scopes': ['a'], 'secret': 'abc'},
          code=403)


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.CRITICAL)
  global_test_setup()
  unittest.main()
