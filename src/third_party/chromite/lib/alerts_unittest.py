# -*- coding: utf-8 -*-
# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the alerts.py module."""

from __future__ import print_function

import json
import mock
import os
import smtplib
import socket
import urllib

from chromite.lib import alerts
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import osutils


class SmtpServerTest(cros_test_lib.MockTestCase):
  """Tests for Smtp server."""

  # pylint: disable=protected-access

  def setUp(self):
    self.smtp_mock = self.PatchObject(smtplib, 'SMTP')

  def testBasic(self):
    """Basic sanity check."""
    msg = alerts.CreateEmail('fake subject', 'fake@localhost', 'fake message')
    server = alerts.SmtpServer(('localhost', 1))
    ret = server.Send(msg)
    self.assertTrue(ret)
    self.assertEqual(self.smtp_mock.call_count, 1)

  def testRetryException(self):
    """Verify we try sending multiple times & don't abort socket.error."""
    self.smtp_mock.side_effect = socket.error('test fail')
    msg = alerts.CreateEmail('fake subject', 'fake@localhost', 'fake message')
    server = alerts.SmtpServer(('localhost', 1))
    ret = server.Send(msg)
    self.assertFalse(ret)
    self.assertEqual(self.smtp_mock.call_count, 4)


class GmailServerTest(cros_test_lib.MockTempDirTestCase):
  """Tests for Gmail server."""

  FAKE_TOKEN_JSON = {
      'client_id': 'fake_client_id',
      'client_secret': 'fake_client_secret',
      'email': 'fake_email@fake.com',
      'refresh_token': 'fake_token',
      'scope': 'https://fake_scope/auth/fake.modify'
  }
  FAKE_CACHE = {
      '_module': 'oauth2client.client',
      'token_expiry': '2014-04-28T19:30:42Z',
      'access_token': 'fake_access_token',
      'token_uri': 'https://accounts.google.com/o/oauth2/token',
      'invalid': False,
      'token_response': {
          'access_token': 'fake_access_token_2',
          'token_type': 'Bearer',
          'expires_in': 3600
      },
      'client_id': 'fake_client_id',
      'id_token': None,
      'client_secret': 'fake_client_secret',
      'revoke_uri': None,
      '_class': 'OAuth2Credentials',
      'refresh_token': 'fake_refresh_token',
      'user_agent': None,
  }

  def setUp(self):
    self.PatchObject(alerts, 'apiclient_build')
    self.token_cache_file = os.path.join(self.tempdir, 'fake_cache')
    self.token_json_file = os.path.join(self.tempdir, 'fake_json')

  def testValidCache(self):
    """Test valid cache."""
    osutils.WriteFile(self.token_cache_file, json.dumps(self.FAKE_CACHE))
    msg = alerts.CreateEmail('fake subject', 'fake@localhost', 'fake msg')
    server = alerts.GmailServer(token_cache_file=self.token_cache_file)
    ret = server.Send(msg)
    self.assertTrue(ret)

  def testCacheNotExistsTokenExists(self):
    """Test cache not exists, token exists"""
    osutils.WriteFile(self.token_json_file, json.dumps(self.FAKE_TOKEN_JSON))
    msg = alerts.CreateEmail('fake subject', 'fake@localhost', 'fake msg')
    server = alerts.GmailServer(token_cache_file=self.token_cache_file,
                                token_json_file=self.token_json_file)
    ret = server.Send(msg)
    self.assertTrue(ret)
    # Cache file should be auto-generated.
    self.assertExists(self.token_cache_file)

  def testCacheNotExistsTokenNotExists(self):
    """Test cache not exists, token not exists."""
    msg = alerts.CreateEmail('fake subject', 'fake@localhost', 'fake msg')
    server = alerts.GmailServer(token_cache_file=self.token_cache_file,
                                token_json_file=self.token_json_file)
    ret = server.Send(msg)
    self.assertFalse(ret)

  def testCacheInvalidTokenExists(self):
    """Test cache exists but invalid, token exists."""
    invalid_cache = self.FAKE_CACHE.copy()
    invalid_cache['invalid'] = True
    osutils.WriteFile(self.token_cache_file, json.dumps(invalid_cache))
    osutils.WriteFile(self.token_json_file, json.dumps(self.FAKE_TOKEN_JSON))
    msg = alerts.CreateEmail('fake subject', 'fake@localhost', 'fake msg')
    server = alerts.GmailServer(token_cache_file=self.token_cache_file,
                                token_json_file=self.token_json_file)
    ret = server.Send(msg)
    self.assertTrue(ret)
    valid_cache = json.loads(osutils.ReadFile(self.token_cache_file))
    self.assertFalse(valid_cache['invalid'])

  def testCacheInvalidTokenNotExists(self):
    """Test cache exists but invalid, token not exists."""
    invalid_cache = self.FAKE_CACHE.copy()
    invalid_cache['invalid'] = True
    osutils.WriteFile(self.token_cache_file, json.dumps(invalid_cache))
    msg = alerts.CreateEmail('fake subject', 'fake@localhost', 'fake msg')
    server = alerts.GmailServer(token_cache_file=self.token_cache_file,
                                token_json_file=self.token_json_file)
    ret = server.Send(msg)
    self.assertFalse(ret)
    invalid_cache = json.loads(osutils.ReadFile(self.token_cache_file))
    self.assertTrue(invalid_cache['invalid'])


class SendEmailTest(cros_test_lib.MockTestCase):
  """Tests for SendEmail."""

  def testSmtp(self):
    """Smtp sanity check."""
    send_mock = self.PatchObject(alerts.SmtpServer, 'Send')
    alerts.SendEmail('mail', 'root@localhost')
    self.assertEqual(send_mock.call_count, 1)

  def testGmail(self):
    """Gmail sanity check."""
    send_mock = self.PatchObject(alerts.GmailServer, 'Send')
    alerts.SendEmail('mail', 'root@localhost',
                     server=alerts.GmailServer(token_cache_file='fakefile'))
    self.assertEqual(send_mock.call_count, 1)


class SendEmailLogTest(cros_test_lib.MockTestCase):
  """Tests for SendEmailLog()."""

  def testSmtp(self):
    """Smtp sanity check."""
    send_mock = self.PatchObject(alerts.SmtpServer, 'Send')
    alerts.SendEmailLog('mail', 'root@localhost')
    self.assertEqual(send_mock.call_count, 1)

  def testGmail(self):
    """Gmail sanity check."""
    send_mock = self.PatchObject(alerts.GmailServer, 'Send')
    alerts.SendEmailLog('mail', 'root@localhost',
                        server=alerts.GmailServer(token_cache_file='fakefile'))
    self.assertEqual(send_mock.call_count, 1)


class GetGardenerEmailAddressesTest(cros_test_lib.MockTestCase):
  """Tests functions related to retrieving the gardener's email address."""

  def _SetEmails(self, emails):
    gardener_json = ('{"updated_unix_timestamp":1547254144,'
                     '"emails":[%s]}' % emails)
    response = mock.MagicMock(json=gardener_json, getcode=lambda: 200,
                              read=lambda: gardener_json)
    self.PatchObject(urllib, 'urlopen', autospec=True,
                     side_effect=[response])

  def testParsingGardenerEmails(self):
    self._SetEmails('"gardener@google.com"')
    self.assertEqual(alerts.GetGardenerEmailAddresses(),
                     ['gardener@google.com'])

    # Test multiple gardeners.
    self._SetEmails('"gardener@google.com", "gardener2@chromium.org"')
    self.assertEqual(alerts.GetGardenerEmailAddresses(),
                     ['gardener@google.com', 'gardener2@chromium.org'])


class GetHealthAlertRecipientsTest(cros_test_lib.MockTestCase):
  """Tests for GetHealthAlertRecipients."""

  def SetRecipients(self, recipients):
    self.builder_run.config.health_alert_recipients = recipients

  def setUp(self):
    self.builder_run = mock.MagicMock()

  def testSingleRecipient(self):
    """Test GetHealthAlertRecipients returns a non-gardener recipient."""
    expected = ['jeff@google.com']
    self.SetRecipients(expected)
    actual = alerts.GetHealthAlertRecipients(self.builder_run)
    self.assertEqual(actual, expected)

  def testGardenerRecipient(self):
    expected = ['gardener@chromium.org']
    self.PatchObject(alerts, 'GetGardenerEmailAddresses', return_value=expected)
    self.SetRecipients([constants.CHROME_GARDENER])
    actual = alerts.GetHealthAlertRecipients(self.builder_run)
    self.assertEqual(actual, expected)


class SendHealthAlertTest(cros_test_lib.MockTestCase):
  """Tests for SendHealthAlert."""

  def setUp(self):
    self.builder_run = mock.MagicMock()
    self.builder_run.InEmailReportingEnvironment = lambda: True

    self.recipients = ['jeff@google.com']
    self.send_email = self.PatchObject(alerts, 'SendEmail')
    self.get_health_alert_recipients = self.PatchObject(
        alerts, 'GetHealthAlertRecipients', return_value=self.recipients)

  def testBasic(self):
    """Test that alert functions are called with the correct args."""
    subject = 'PyTorch > TensorFlow'
    body = 'You heard it here, Jeff'
    alerts.SendHealthAlert(self.builder_run, subject, body)
    self.assertEqual(
        self.send_email.call_args_list,
        [mock.call(subject, self.recipients,
                   server=mock.ANY, message=body, extra_fields=None)])


def main(_argv):
  # No need to make unittests sleep.
  alerts.SmtpServer.SMTP_RETRY_DELAY = 0

  cros_test_lib.main(module=__name__)
