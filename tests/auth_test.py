#!/usr/bin/env python
# Copyright (c) 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit Tests for auth.py"""

import calendar
import datetime
import json
import os
import unittest
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from third_party import mock

import auth
import subprocess2


NOW = datetime.datetime(2019, 10, 17, 12, 30, 59, 0)
VALID_EXPIRY = NOW + datetime.timedelta(seconds=31)


class AuthenticatorTest(unittest.TestCase):
  def setUp(self):
    mock.patch('subprocess2.check_call').start()
    mock.patch('subprocess2.check_call_out').start()
    mock.patch('auth.datetime_now', return_value=NOW).start()
    self.addCleanup(mock.patch.stopall)

  def testHasCachedCredentials_NotLoggedIn(self):
    subprocess2.check_call_out.side_effect = [
        subprocess2.CalledProcessError(1, ['cmd'], 'cwd', 'stdout', 'stderr')]
    self.assertFalse(auth.Authenticator().has_cached_credentials())

  def testHasCachedCredentials_LoggedIn(self):
    subprocess2.check_call_out.return_value = (
        json.dumps({'token': 'token', 'expiry': 12345678}), '')
    self.assertTrue(auth.Authenticator().has_cached_credentials())

  def testGetAccessToken_NotLoggedIn(self):
    subprocess2.check_call_out.side_effect = [
        subprocess2.CalledProcessError(1, ['cmd'], 'cwd', 'stdout', 'stderr')]
    self.assertRaises(
        auth.LoginRequiredError, auth.Authenticator().get_access_token)

  def testGetAccessToken_CachedToken(self):
    authenticator = auth.Authenticator()
    authenticator._access_token = auth.AccessToken('token', None)
    self.assertEqual(
        auth.AccessToken('token', None), authenticator.get_access_token())
    self.assertEqual(0, len(subprocess2.check_call_out.mock_calls))

  def testGetAccesstoken_LoggedIn(self):
    expiry = calendar.timegm(VALID_EXPIRY.timetuple())
    subprocess2.check_call_out.return_value = (
        json.dumps({'token': 'token', 'expiry': expiry}), '')
    self.assertEqual(
        auth.AccessToken('token', VALID_EXPIRY),
        auth.Authenticator().get_access_token())
    subprocess2.check_call_out.assert_called_with(
        ['luci-auth',
         'token',
         '-scopes', auth.OAUTH_SCOPE_EMAIL,
         '-json-output', '-'],
        stdout=subprocess2.PIPE, stderr=subprocess2.PIPE)

  def testGetAccessToken_DifferentScope(self):
    expiry = calendar.timegm(VALID_EXPIRY.timetuple())
    subprocess2.check_call_out.return_value = (
        json.dumps({'token': 'token', 'expiry': expiry}), '')
    self.assertEqual(
        auth.AccessToken('token', VALID_EXPIRY),
        auth.Authenticator('custom scopes').get_access_token())
    subprocess2.check_call_out.assert_called_with(
        ['luci-auth', 'token', '-scopes', 'custom scopes', '-json-output', '-'],
        stdout=subprocess2.PIPE, stderr=subprocess2.PIPE)

  def testAuthorize(self):
    http = mock.Mock()
    http_request = http.request
    http_request.__name__ = '__name__'

    authenticator = auth.Authenticator()
    authenticator._access_token = auth.AccessToken('token', None)

    authorized = authenticator.authorize(http)
    authorized.request(
        'https://example.com', method='POST', body='body',
        headers={'header': 'value'})
    http_request.assert_called_once_with(
        'https://example.com', 'POST', 'body',
        {'header': 'value', 'Authorization': 'Bearer token'}, mock.ANY,
        mock.ANY)


class AccessTokenTest(unittest.TestCase):
  def setUp(self):
    mock.patch('auth.datetime_now', return_value=NOW).start()
    self.addCleanup(mock.patch.stopall)

  def testNeedsRefresh_NoExpiry(self):
    self.assertFalse(auth.AccessToken('token', None).needs_refresh())

  def testNeedsRefresh_Expired(self):
    expired = NOW + datetime.timedelta(seconds=30)
    self.assertTrue(auth.AccessToken('token', expired).needs_refresh())

  def testNeedsRefresh_Valid(self):
    self.assertFalse(auth.AccessToken('token', VALID_EXPIRY).needs_refresh())


if __name__ == '__main__':
  if '-v' in sys.argv:
    logging.basicConfig(level=logging.DEBUG)
  unittest.main()
