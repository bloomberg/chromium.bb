# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for net.py."""

from __future__ import print_function

import sys
import unittest

import httplib2

from chromite.lib import auth
from chromite.lib import cros_logging as logging
from chromite.lib.luci import net
from chromite.lib.luci.test_support import test_case, test_env

test_env.setup_test_env()


class NetTest(test_case.TestCase):
  """Tests for Net."""

  def setUp(self):
    super(NetTest, self).setUp()

    def GetAccessToken(service_account_json): # pylint: disable=unused-argument
      return 'token'

    self.mock(auth, 'GetAccessToken', GetAccessToken)

    self.mock(logging, 'warning', lambda *_args: None)
    self.mock(logging, 'error', lambda *_args: None)

  def mock_httplib2(self, calls):

    def mocked(http, **kwargs):
      if not calls:
        self.fail('Unexpected httplib2 call: %s' % kwargs)
      expected, response = calls.pop(0)
      defaults = {
          'headers': {},
          'method': 'GET',
          'body': None,
      }
      defaults.update(expected)
      self.assertEqual(defaults, kwargs)
      self.assertIsInstance(http, httplib2.Http)
      if isinstance(response, Exception):
        raise response
      return response

    self.mock(net, 'httprequest', mocked)
    return calls

  def test_request_works(self):
    self.mock_httplib2([
        ({
            'headers': {
                'Accept': 'text/plain',
                'Authorization': 'Bearer token'
            },
            'method': 'POST',
            'body': 'post body',
            'uri': 'http://localhost/123?a=%3D&b=%26',
        }, (httplib2.Response({}), 'response body')),
    ])
    response = net.request(
        url='http://localhost/123',
        method='POST',
        payload='post body',
        params={
            'a': '=',
            'b': '&'
        },
        headers={'Accept': 'text/plain'},
        include_auth=True,
        deadline=123,
        max_attempts=5)
    self.assertEqual('response body', response)

  def test_retries_transient_errors(self):
    self.mock_httplib2([
        ({
            'uri': 'http://localhost/123'
        }, httplib2.HttpLib2Error()),
        ({
            'uri': 'http://localhost/123'
        }, (httplib2.Response({
            'status': 408,
            'reason': 'client timeout',
        }), 'client timeout')),
        ({
            'uri': 'http://localhost/123'
        }, (httplib2.Response({
            'status': 500,
            'reason': 'server error',
        }), 'server error')),
        ({
            'uri': 'http://localhost/123'
        }, (httplib2.Response({}), 'response body')),
    ])
    response = net.request('http://localhost/123', max_attempts=4)
    self.assertEqual('response body', response)

  def test_gives_up_retrying(self):
    self.mock_httplib2([
        ({
            'uri': 'http://localhost/123'
        }, (httplib2.Response({
            'status': 500,
            'reason': 'server error'
        }), 'server error')),
        ({
            'uri': 'http://localhost/123'
        }, (httplib2.Response({
            'status': 500,
            'reason': 'server error'
        }), 'server error')),
        ({
            'uri': 'http://localhost/123'
        }, (httplib2.Response({}), 'response body')),
    ])
    with self.assertRaises(net.Error):
      net.request('http://localhost/123', max_attempts=2)

  def test_404(self):
    self.mock_httplib2([
        ({
            'uri': 'http://localhost/123'
        }, (httplib2.Response({
            'status': 404,
            'reason': 'Not found'
        }), 'Not found')),
    ])
    with self.assertRaises(net.NotFoundError):
      net.request('http://localhost/123')

  def test_crappy_cloud_endpoints_404_is_retried(self):
    self.mock_httplib2([
        ({
            'uri': 'http://localhost/_ah/api/blah'
        }, (httplib2.Response({
            'status': 404,
            'reason': 'Not found'
        }), 'Not found')),
        ({
            'uri': 'http://localhost/_ah/api/blah'
        }, (httplib2.Response({}), 'response body')),
    ])
    response = net.request('http://localhost/_ah/api/blah')
    self.assertEqual('response body', response)

  def test_legitimate_cloud_endpoints_404_is_not_retried(self):
    self.mock_httplib2([
        ({
            'uri': 'http://localhost/_ah/api/blah'
        }, (httplib2.Response({
            'status': 404,
            'reason': 'Not found',
            'Content-Type': 'application/json'
        }), '{}')),
    ])
    with self.assertRaises(net.NotFoundError):
      net.request('http://localhost/_ah/api/blah')

  def test_401(self):
    self.mock_httplib2([
        ({
            'uri': 'http://localhost/123'
        }, (httplib2.Response({
            'status': 401,
            'reason': 'Auth error'
        }), 'Auth error')),
    ])
    with self.assertRaises(net.AuthError):
      net.request('http://localhost/123')

  def test_403(self):
    self.mock_httplib2([
        ({
            'uri': 'http://localhost/123'
        }, (httplib2.Response({
            'status': 403,
            'reason': 'Auth error'
        }), 'Auth error')),
    ])
    with self.assertRaises(net.AuthError):
      net.request('http://localhost/123')


if __name__ == '__main__':
  if '-v' in sys.argv:
    unittest.TestCase.maxDiff = None
  unittest.main()
