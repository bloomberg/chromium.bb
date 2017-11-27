#!/usr/bin/env python
# Copyright (c) 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit Tests for auth.py"""

import __builtin__
import datetime
import json
import logging
import os
import unittest
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


from testing_support import auto_stub
from third_party import httplib2
from third_party import mock

import auth


class TestGetLuciContextAccessToken(auto_stub.TestCase):
  mock_env = {'LUCI_CONTEXT': 'default/test/path'}

  def _mock_local_auth(self, account_id, secret, rpc_port):
    self.mock(auth, '_load_luci_context', mock.Mock())
    auth._load_luci_context.return_value = {
      'local_auth': {
        'default_account_id': account_id,
        'secret': secret,
        'rpc_port': rpc_port,
      }
    }

  def _mock_loc_server_resp(self, status, content):
    mock_resp = mock.Mock()
    mock_resp.status = status
    self.mock(httplib2.Http, 'request', mock.Mock())
    httplib2.Http.request.return_value = (mock_resp, content)

  def test_correct_local_auth_format(self):
    self._mock_local_auth('dead', 'beef', 10)
    expiry_time = datetime.datetime.min + datetime.timedelta(hours=1)
    resp_content = {
      'error_code': None,
      'error_message': None,
      'access_token': 'token',
      'expiry': (expiry_time
                 - datetime.datetime.utcfromtimestamp(0)).total_seconds(),
    }
    self._mock_loc_server_resp(200, json.dumps(resp_content))
    token = auth._get_luci_context_access_token(
        self.mock_env, datetime.datetime.min)
    self.assertEquals(token.token, 'token')

  def test_incorrect_port_format(self):
    self._mock_local_auth('foo', 'bar', 'bar')
    with self.assertRaises(auth.LuciContextAuthError):
      auth._get_luci_context_access_token(self.mock_env, datetime.datetime.min)

  def test_no_account_id(self):
    self._mock_local_auth(None, 'bar', 10)
    token = auth._get_luci_context_access_token(
        self.mock_env, datetime.datetime.min)
    self.assertIsNone(token)

  def test_expired_token(self):
    self._mock_local_auth('dead', 'beef', 10)
    resp_content = {
      'error_code': None,
      'error_message': None,
      'access_token': 'token',
      'expiry': 1,
    }
    self._mock_loc_server_resp(200, json.dumps(resp_content))
    with self.assertRaises(auth.LuciContextAuthError):
      auth._get_luci_context_access_token(
          self.mock_env, datetime.datetime.utcfromtimestamp(1))

  def test_incorrect_expiry_format(self):
    self._mock_local_auth('dead', 'beef', 10)
    resp_content = {
      'error_code': None,
      'error_message': None,
      'access_token': 'token',
      'expiry': 'dead',
    }
    self._mock_loc_server_resp(200, json.dumps(resp_content))
    with self.assertRaises(auth.LuciContextAuthError):
      auth._get_luci_context_access_token(self.mock_env, datetime.datetime.min)

  def test_incorrect_response_content_format(self):
    self._mock_local_auth('dead', 'beef', 10)
    self._mock_loc_server_resp(200, '5')
    with self.assertRaises(auth.LuciContextAuthError):
      auth._get_luci_context_access_token(self.mock_env, datetime.datetime.min)


if __name__ == '__main__':
  if '-v' in sys.argv:
    logging.basicConfig(level=logging.DEBUG)
  unittest.main()
