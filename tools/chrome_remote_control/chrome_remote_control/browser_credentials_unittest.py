# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest
import tempfile

from chrome_remote_control import browser_credentials

simple_config_string = """
{
  "google_username": "example",
  "google_password": "asdf",

  "google_account_url": null,


  "othersite_username": "test",
  "othersite_password": "1234"
}
"""

desired_config = {'google_username':None, 'othersite_password':None}

result_config = {'google_username':'example', 'othersite_password':'1234'}

class TestBrowserCredentials(unittest.TestCase):
  def testGetConfig(self):
    browser_cred = browser_credentials.BrowserCredentials()
    with tempfile.NamedTemporaryFile() as f:
      f.write(simple_config_string)
      f.flush()

      browser_cred.SetCredentialsConfigFile(f.name)
      config = browser_cred.GetConfig(desired_config)

      self.assertEqual(config, result_config)
