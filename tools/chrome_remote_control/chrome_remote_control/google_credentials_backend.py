# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

import chrome_remote_control
from chrome_remote_control import util

GOOGLE_CREDENTIALS_TYPE = 'google'
GOOGLE_ACCOUNTS_URL = 'https://accounts.google.com/'

def _WaitForFormToLoad(form_id, tab):
  def IsFormLoaded():
    return tab.runtime.Evaluate(
        'document.querySelector("#%s")!== null' % form_id)

  # Wait until the form is submitted and the page completes loading.
  util.WaitFor(lambda: IsFormLoaded(), 60) # pylint: disable=W0108

def _SubmitFormAndWait(form_id, tab):
  js = 'document.getElementById("%s").submit();' % form_id
  tab.runtime.Execute(js)

  def IsLoginStillHappening():
    return tab.runtime.Evaluate(
        'document.querySelector("#%s")!== null' % form_id)

  # Wait until the form is submitted and the page completes loading.
  util.WaitFor(lambda: not IsLoginStillHappening(), 60)

class GoogleCredentialsBackend(object):
  def __init__(self):
    self._logged_in = False

  @property
  def credentials_type(self): # pylint: disable=R0201
    return GOOGLE_CREDENTIALS_TYPE

  def LoginNeeded(self, tab, config):
    """Logs in to a test Google account.

    Raises:
      RuntimeError: if could not get credential information.
    """
    if self._logged_in:
      return True

    if 'username' not in config or 'password' not in config:
      message = 'Credentials for "google" must include username and password.'
      raise RuntimeError(message)

    logging.debug(
      'Logging into google account...')

    url = GOOGLE_ACCOUNTS_URL

    try:
      logging.info('Loading %s...', url)
      tab.page.Navigate(url)
      form_id = 'gaia_loginform'
      _WaitForFormToLoad(form_id, tab)
      tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()
      logging.info('Loaded page: %s', url)

      email_id = 'document.getElementById("Email").value = "%s"; ' % (
        config['username'])
      password = 'document.getElementById("Passwd").value = "%s"; ' % (
        config['password'])
      tab.runtime.Execute(email_id)
      tab.runtime.Execute(password)

      _SubmitFormAndWait(form_id, tab)

      self._logged_in = True
      return True
    except chrome_remote_control.TimeoutException:
      logging.warning('Timed out while loading: %s', url)
      return False

  def LoginNoLongerNeeded(self, tab): # pylint: disable=W0613
    assert self._logged_in
