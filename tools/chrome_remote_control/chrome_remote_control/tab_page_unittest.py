# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import tab_page
import tab_test_case

class TabPageTest(tab_test_case.TabTestCase):
  def testPageNavigateToNormalUrl(self):
    res = self._tab.page.Navigate("http://www.google.com")
    self._tab.WaitForDocumentReadyStateToBeComplete()

  def testPageNavigateToUrlChanger(self):
    # The Url that we actually load is http://www.youtube.com/.
    res = self._tab.page.Navigate("http://youtube.com/")

    self._tab.WaitForDocumentReadyStateToBeComplete()

  def testPageNavigateToImpossibleURL(self):
    res = self._tab.page.Navigate("http://23f09f0f9fsdflajsfaldfkj2f3f.com")
    self._tab.WaitForDocumentReadyStateToBeComplete()
