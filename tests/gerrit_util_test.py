#!/usr/bin/env vpython
# coding=utf-8
# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function
from __future__ import unicode_literals


import base64
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from third_party import mock

import gerrit_util
import gclient_utils
import subprocess2


class CookiesAuthenticatorTest(unittest.TestCase):
  _GITCOOKIES = '\n'.join([
      '\t'.join([
          'chromium.googlesource.com',
          'FALSE',
          '/',
          'TRUE',
          '2147483647',
          'o',
          'git-user.chromium.org=1/chromium-secret',
      ]),
      '\t'.join([
          'chromium-review.googlesource.com',
          'FALSE',
          '/',
          'TRUE',
          '2147483647',
          'o',
          'git-user.chromium.org=1/chromium-secret',
      ]),
      '\t'.join([
          '.example.com',
          'FALSE',
          '/',
          'TRUE',
          '2147483647',
          'o',
          'example-bearer-token',
      ]),
      '\t'.join([
          'another-path.example.com',
          'FALSE',
          '/foo',
          'TRUE',
          '2147483647',
          'o',
          'git-example.com=1/another-path-secret',
      ]),
      '\t'.join([
          'another-key.example.com',
          'FALSE',
          '/',
          'TRUE',
          '2147483647',
          'not-o',
          'git-example.com=1/another-key-secret',
      ]),
      '#' + '\t'.join([
          'chromium-review.googlesource.com',
          'FALSE',
          '/',
          'TRUE',
          '2147483647',
          'o',
          'git-invalid-user.chromium.org=1/invalid-chromium-secret',
      ]),
      'Some unrelated line\t that should not be here',
  ])

  def setUp(self):
    mock.patch('gclient_utils.FileRead', return_value=self._GITCOOKIES).start()
    mock.patch('os.getenv', return_value={}).start()
    mock.patch('os.environ', {'HOME': '$HOME'}).start()
    mock.patch('os.path.exists', return_value=True).start()
    mock.patch(
        'subprocess2.check_output',
        side_effect=[
            subprocess2.CalledProcessError(1, ['cmd'], 'cwd', 'out', 'err')],
    ).start()
    self.addCleanup(mock.patch.stopall)
    self.maxDiff = None

  def testGetNewPasswordUrl(self):
    auth = gerrit_util.CookiesAuthenticator()
    self.assertEqual(
        'https://chromium-review.googlesource.com/new-password',
        auth.get_new_password_url('chromium.googlesource.com'))
    self.assertEqual(
        'https://chrome-internal-review.googlesource.com/new-password',
        auth.get_new_password_url('chrome-internal-review.googlesource.com'))

  def testGetNewPasswordMessage(self):
    auth = gerrit_util.CookiesAuthenticator()
    self.assertIn(
        'https://chromium-review.googlesource.com/new-password',
        auth.get_new_password_message('chromium-review.googlesource.com'))
    self.assertIn(
        'https://chrome-internal-review.googlesource.com/new-password',
        auth.get_new_password_message('chrome-internal.googlesource.com'))

  def testGetGitcookiesPath(self):
    self.assertEqual(
        os.path.join('$HOME', '.gitcookies'),
        gerrit_util.CookiesAuthenticator().get_gitcookies_path())

    subprocess2.check_output.side_effect = ['http.cookiefile']
    self.assertEqual(
        'http.cookiefile',
        gerrit_util.CookiesAuthenticator().get_gitcookies_path())
    subprocess2.check_output.assert_called_with(
        ['git', 'config', '--path', 'http.cookiefile'])

    os.getenv.return_value = 'git-cookies-path'
    self.assertEqual(
        'git-cookies-path',
        gerrit_util.CookiesAuthenticator().get_gitcookies_path())
    os.getenv.assert_called_with('GIT_COOKIES_PATH')

  def testGitcookies(self):
    auth = gerrit_util.CookiesAuthenticator()
    self.assertEqual(auth.gitcookies, {
        'chromium.googlesource.com':
            ('git-user.chromium.org', '1/chromium-secret'),
        'chromium-review.googlesource.com':
            ('git-user.chromium.org', '1/chromium-secret'),
        '.example.com':
            ('', 'example-bearer-token'),
    })

  def testGetAuthHeader(self):
    expected_chromium_header = (
        'Basic Z2l0LXVzZXIuY2hyb21pdW0ub3JnOjEvY2hyb21pdW0tc2VjcmV0')

    auth = gerrit_util.CookiesAuthenticator()
    self.assertEqual(
        expected_chromium_header,
        auth.get_auth_header('chromium.googlesource.com'))
    self.assertEqual(
        expected_chromium_header,
        auth.get_auth_header('chromium-review.googlesource.com'))
    self.assertEqual(
        'Bearer example-bearer-token',
        auth.get_auth_header('some-review.example.com'))

  def testGetAuthEmail(self):
    auth = gerrit_util.CookiesAuthenticator()
    self.assertEqual(
        'user@chromium.org',
        auth.get_auth_email('chromium.googlesource.com'))
    self.assertEqual(
        'user@chromium.org',
        auth.get_auth_email('chromium-review.googlesource.com'))
    self.assertIsNone(auth.get_auth_email('some-review.example.com'))


if __name__ == '__main__':
  unittest.main()
