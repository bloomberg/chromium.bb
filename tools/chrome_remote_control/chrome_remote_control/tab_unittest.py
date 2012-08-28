# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import browser
import browser_finder
import browser_options
import tab
import unittest

class TabTest(unittest.TestCase):
  def setUp(self):
    self._browser = None
    self._tab = None
    options = browser_options.BrowserOptions()
    browser_to_create = browser_finder.FindBestPossibleBrowser(options)
    try:
      self._browser = browser_to_create.Create()
      self._tab = self._browser.ConnectToNthTab(0)
    except:
      self.tearDown()
      raise

  def tearDown(self):
    if self._tab:
      self._tab.Close()
    if self._browser:
      self._browser.Close()

  def testLoadURLAndWaitToForCompleteState(self):
    self._tab.BeginToLoadURL("http://www.google.com")
    self._tab.WaitForDocumentReadyStateToBeComplete()

  def testLoadURLAndWaitToForInteractiveState(self):
    self._tab.BeginToLoadURL("http://www.google.com")
    self._tab.WaitForDocumentReadyStateToBeInteractiveOrBetter()
