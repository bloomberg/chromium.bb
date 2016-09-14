#!/usr/bin/env python
# Copyright 2016 The LUCI Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0
# that can be found in the LICENSE file.

import contextlib
import json
import logging
import os
import socket
import sys
import tempfile
import time
import unittest

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(
    __file__.decode(sys.getfilesystemencoding()))))
sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, 'third_party'))

from depot_tools import auto_stub
from depot_tools import fix_encoding
from third_party import requests

from utils import authenticators
from utils import auth_server
from utils import net
from utils import oauth
import net_utils


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


@contextlib.contextmanager
def luci_context_server(token_cb, secret=None, rpc_port=None):
  class MockedProvider(object):
    def generate_token(self, scopes):
      return token_cb(scopes)
  s = auth_server.LocalAuthServer()
  try:
    luci_context = s.start(MockedProvider())
    local_auth = {'local_auth': {
        'secret': secret if secret else luci_context['secret'],
        'rpc_port': rpc_port if rpc_port else luci_context['rpc_port']}}
    fd, luci_context_path = tempfile.mkstemp(
        suffix='.json', prefix='luci_context.')
    with os.fdopen(fd, 'w') as f:
      json.dump(local_auth, f, indent=2, sort_keys=True)
    yield luci_context_path
  finally:
    s.stop()
    try:
      os.remove(luci_context_path)
    except OSError:
      pass


class LocalAuthHttpServiceTest(auto_stub.TestCase):
  """Tests for LocalAuthServer and LuciContextAuthenticator."""
  epoch = 12345678

  def setUp(self):
    super(LocalAuthHttpServiceTest, self).setUp()
    self.mock_time(0)

  def mock_time(self, delta):
    self.mock(time, 'time', lambda: self.epoch + delta)

  @staticmethod
  def mocked_http_service(
      url='http://example.com',
      config=None,
      perform_request=None):

    class MockedRequestEngine(object):
      def perform_request(self, request):
        return perform_request(request) if perform_request else None
      @classmethod
      def timeout_exception_classes(cls):
        return ()
      @classmethod
      def parse_request_exception(cls, exc):
        del exc  # Unused argument
        return None, None

    return net.HttpService(
        url,
        authenticator=authenticators.LuciContextAuthenticator(config),
        engine=MockedRequestEngine())

  def test_works(self):
    service_url = 'http://example.com'
    request_url = '/some_request'
    response = 'True'
    token = 'notasecret'

    def token_gen(scopes):
      self.assertEqual(1, len(scopes))
      self.assertEqual(oauth.OAUTH_SCOPES, scopes[0])
      return auth_server.AccessToken(token, time.time() + 300)

    def handle_request(request):
      self.assertTrue(
          request.get_full_url().startswith(service_url + request_url))
      self.assertEqual('', request.body)
      self.assertEqual(u'Bearer %s' % token,
                       request.headers['Authorization'])
      return net_utils.make_fake_response(response, request.get_full_url())

    with luci_context_server(token_gen) as luci_context:
      config = oauth.make_oauth_config(luci_context_json=luci_context)
      service = self.mocked_http_service(perform_request=handle_request,
                                         config=config)
      self.assertEqual(service.request(request_url, data={}).read(), response)

  def test_bad_secret(self):
    service_url = 'http://example.com'
    request_url = '/some_request'
    response = 'False'

    def token_gen(scopes):
      del scopes  # Unused argument
      self.fail('must not be called')

    def handle_request(request):
      self.assertTrue(
          request.get_full_url().startswith(service_url + request_url))
      self.assertEqual('', request.body)
      self.assertIsNone(request.headers.get('Authorization'))
      return net_utils.make_fake_response(response, request.get_full_url())

    with luci_context_server(token_gen, secret='invalid') as luci_context:
      config = oauth.make_oauth_config(luci_context_json=luci_context)
      service = self.mocked_http_service(perform_request=handle_request,
                                         config=config)
      self.assertEqual(service.request(request_url, data={}).read(), response)

  def test_bad_port(self):
    request_url = '/some_request'

    def token_gen(scopes):
      del scopes  # Unused argument
      self.fail('must not be called')

    def handle_request(request):
      del request  # Unused argument
      self.fail('must not be called')

    with luci_context_server(token_gen, rpc_port=22) as luci_context:
      config = oauth.make_oauth_config(luci_context_json=luci_context)
      service = self.mocked_http_service(perform_request=handle_request,
                                         config=config)
      with self.assertRaises(socket.error):
        self.assertRaises(service.request(request_url, data={}).read())

  def test_expired_token(self):
    service_url = 'http://example.com'
    request_url = '/some_request'
    response = 'False'
    token = 'notasecret'

    def token_gen(scopes):
      self.assertEqual(1, len(scopes))
      self.assertEqual(oauth.OAUTH_SCOPES, scopes[0])
      return auth_server.AccessToken(token, time.time())

    def handle_request(request):
      self.assertTrue(
          request.get_full_url().startswith(service_url + request_url))
      self.assertEqual('', request.body)
      self.assertIsNone(request.headers.get('Authorization'))
      return net_utils.make_fake_response(response, request.get_full_url())

    with luci_context_server(token_gen) as luci_context:
      config = oauth.make_oauth_config(luci_context_json=luci_context)
      service = self.mocked_http_service(perform_request=handle_request,
                                         config=config)
      self.assertEqual(service.request(request_url, data={}).read(), response)


if __name__ == '__main__':
  fix_encoding.fix_encoding()
  logging.basicConfig(
      level=logging.DEBUG if '-v' in sys.argv else logging.CRITICAL)
  global_test_setup()
  unittest.main()
