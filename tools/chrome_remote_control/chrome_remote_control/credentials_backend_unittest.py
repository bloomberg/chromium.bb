# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest
import os

from chrome_remote_control import browser_credentials
from chrome_remote_control import credentials_backend
from chrome_remote_control import browser_finder
from chrome_remote_control import browser_options
from chrome_remote_control import util

simple_config_dict = {'google_username':'example', 'google_password':'asdf',
                      'google_account_url': None, 'othersite_username':'test',
                      'othersite_password':'1234'}

google_credentials_file = os.path.join(os.path.dirname(__file__),
                                       'credentials.json')

google_credentials = {'google_username':None, 'google_password':None,
                      'google_account_url': None}

def getPossibleBrowser():
  assert browser_options.options_for_unittests
  options = (
    browser_options.options_for_unittests.Copy())
  temp_parser = options.CreateParser()
  defaults = temp_parser.get_default_values()
  for k, v in defaults.__dict__.items():
    if hasattr(options, k):
      continue
    setattr(options, k, v)

  possible_browser = browser_finder.FindBrowser(options)

  return possible_browser

def waitForExist(element_id, tab):
  def exists():
    return tab.runtime.Evaluate(
        'document.querySelector("#%s")!== null' % element_id)

  # Wait until the form is submitted and the page completes loading.
  util.WaitFor(lambda: exists(), 60) # pylint: disable=W0108

class TestCredentialsBackend(unittest.TestCase):
  def testSubmitForm(self):
    browser_cred = browser_credentials.BrowserCredentials()
    backend = credentials_backend.CredentialsBackend(browser_cred)
    possible_browser = getPossibleBrowser()
    self.assertIsNotNone(possible_browser) # pylint: disable=E1101

    browser_cred.SetCredentialsConfigFile(google_credentials_file)
    try:
      config = browser_cred.GetConfig(google_credentials)
    except IOError:
      self.skipTest( # pylint: disable=E1101
          'Google Credentials File Not Found: "%s"' %
                    google_credentials_file)

    with possible_browser.Create() as browser:
      with browser.ConnectToNthTab(0) as tab:
        tab.page.Navigate('http://accounts.google.com')
        form_id = 'gaia_loginform'
        waitForExist(form_id, tab)
        tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()

        username = config['google_username']
        password = config['google_password']

        email_id = 'document.getElementById("Email").value = "%s"; ' % username
        password = 'document.getElementById("Passwd").value = "%s"; ' % password
        tab.runtime.Execute(email_id)
        tab.runtime.Execute(password)

        backend.SubmitForm(form_id, tab)

        waitForExist('nav-privacy', tab)

  def testWaitForFormToLoad(self):
    browser_cred = browser_credentials.BrowserCredentials()
    backend = credentials_backend.CredentialsBackend(browser_cred)
    possible_browser = getPossibleBrowser()
    self.assertIsNotNone(possible_browser) # pylint: disable=E1101

    with possible_browser.Create() as browser:
      with browser.ConnectToNthTab(0) as tab:
        tab.page.Navigate('http://accounts.google.com')
        form_id = 'gaia_loginform'
        backend.WaitForFormToLoad(form_id, tab)
        waitForExist(form_id, tab)

  def testSubmitFormAndWait(self):
    browser_cred = browser_credentials.BrowserCredentials()
    backend = credentials_backend.CredentialsBackend(browser_cred)
    possible_browser = getPossibleBrowser()
    self.assertIsNotNone(possible_browser) # pylint: disable=E1101

    browser_cred.SetCredentialsConfigFile(google_credentials_file)
    try:
      config = browser_cred.GetConfig(google_credentials)
    except IOError:
      self.skipTest( # pylint: disable=E1101
          'Google Credentials File Not Found: "%s"' %
                    google_credentials_file)

    with possible_browser.Create() as browser:
      with browser.ConnectToNthTab(0) as tab:
        tab.page.Navigate('http://accounts.google.com')
        form_id = 'gaia_loginform'
        waitForExist(form_id, tab)
        tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()

        username = config['google_username']
        password = config['google_password']

        email_id = 'document.getElementById("Email").value = "%s"; ' % username
        password = 'document.getElementById("Passwd").value = "%s"; ' % password
        tab.runtime.Execute(email_id)
        tab.runtime.Execute(password)

        backend.SubmitFormAndWait(form_id, tab)

        waitForExist('nav-privacy', tab)
