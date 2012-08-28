# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest
import browser
import browser_finder
import browser_options

class BrowserTest(unittest.TestCase):
  def testBasic(self):
    options = browser_options.BrowserOptions()
    options.browser_to_use = browser_finder.ALL_BROWSER_TYPES
    browser_to_create = browser_finder.FindBestPossibleBrowser(options)
    with browser_to_create.Create() as b:
      self.assertEquals(1, b.num_tabs)
      self.assertEquals("chrome://newtab/", b.GetNthTabURL(0))
