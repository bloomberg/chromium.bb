# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import chrome_remote_control
from chrome_remote_control.credentials_backend import CredentialsBackend

class GoogleCredentialsBackend(CredentialsBackend):
  label = 'google'

  def __init__(self, browser_credentials):
    super(GoogleCredentialsBackend, self).__init__(browser_credentials)
    self._logged_in = False

  def LoginNeeded(self, tab):
    if not self._logged_in:
      self._logged_in = self._LoginToGoogleAccount(tab)

  def LoginNoLongerNeeded(self, tab):
    pass

  def GoogleAccountsLogin(self, username, password, tab, url=None):
    """Log into Google Accounts.

    Attempts to login to Google by entering the username/password into the
    google login page and click submit button.

    Args:
      username: users login input.
      password: users login password input.
      url: an alternative url for login page, if None, original one will be
      used.
    """
    url = url or 'https://accounts.google.com/'

    try:
      logging.info('Loading %s...', url)
      tab.page.Navigate(url)
      form_id = 'gaia_loginform'
      self.WaitForFormToLoad(form_id, tab)
      tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()
      logging.info('Loaded page: %s', url)

      email_id = 'document.getElementById("Email").value = "%s"; ' % username
      password = 'document.getElementById("Passwd").value = "%s"; ' % password
      tab.runtime.Execute(email_id)
      tab.runtime.Execute(password)

      self.SubmitFormAndWait(form_id, tab)

      return True
    except chrome_remote_control.TimeoutException:
      logging.warning('Timed out while loading: %s', url)
    return False


  def _LoginToGoogleAccount(self, tab):
    """Logs in to a test Google account.

    Login with user-defined credentials if they exist.
    Else fail.

    Raises:
      RuntimeError: if could not get credential information.
    """
    config = self.browser_credentials.GetConfig(
        {'google_username': None,
         'google_password': None,
         'google_account_url': 'https://accounts.google.com/'})
    google_account_url = config.get('google_account_url')
    username = config.get('google_username')
    password = config.get('google_password')
    if username and password:
      logging.info(
          'Using google account credential from %s',
          self.browser_credentials.GetCredentialsConfigFile())
    else:
      message = 'No credentials could be found. ' \
                'Please specify credential information in %s.' \
                % self.browser_credentials.GetCredentialsConfigFile()
      raise RuntimeError(message)
    return self.GoogleAccountsLogin(username, password, tab,
                                    url=google_account_url)
