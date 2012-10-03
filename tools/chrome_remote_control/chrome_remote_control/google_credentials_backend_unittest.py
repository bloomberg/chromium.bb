# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from chrome_remote_control import browser_credentials
from chrome_remote_control import google_credentials_backend
from chrome_remote_control import credentials_backend_unittest

class TestGoogleCredentialsBackend(unittest.TestCase):
  def testGoogleAccountsLogin(self):
    browser_cred = browser_credentials.BrowserCredentials()
    gcb = google_credentials_backend.GoogleCredentialsBackend(browser_cred)
    possible_browser = credentials_backend_unittest.getPossibleBrowser()
    self.assertIsNotNone(possible_browser) # pylint: disable=E1101

    browser_cred.SetCredentialsConfigFile(
        credentials_backend_unittest.google_credentials_file)
    try:
      config = browser_cred.GetConfig(
          credentials_backend_unittest.google_credentials)
    except IOError:
      self.skipTest( # pylint: disable=E1101
          'Google Credentials File Not Found: "%s"' %
                    credentials_backend_unittest.google_credentials_file)

    with possible_browser.Create() as browser:
      with browser.ConnectToNthTab(0) as tab:
        username = config['google_username']
        password = config['google_password']

        gcb.GoogleAccountsLogin(username, password, tab,
                                config['google_account_url'])
        credentials_backend_unittest.waitForExist('nav-privacy', tab)
