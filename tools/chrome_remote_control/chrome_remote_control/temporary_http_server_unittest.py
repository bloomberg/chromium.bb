# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import unittest
import urllib2
import urlparse

import browser_finder
import browser_options
import temporary_http_server

class TemporaryHTTPServerTest(unittest.TestCase):
  def testBasicHosting(self):
    unittest_data_dir = os.path.join(os.path.dirname(__file__),
                                     "..", "unittest_data")
    options = browser_options.options_for_unittests
    browser_to_create = browser_finder.FindBrowser(options)
    with browser_to_create.Create() as b:
      with b.CreateTemporaryHTTPServer(unittest_data_dir) as s:
        with b.ConnectToNthTab(0) as t:
          t.page.Navigate(s.UrlOf("/blank.html"))
          t.WaitForDocumentReadyStateToBeComplete()
          x = t.runtime.Evaluate("document.body.innerHTML")
          x = x.strip()

          self.assertEquals(x, "Hello world")

