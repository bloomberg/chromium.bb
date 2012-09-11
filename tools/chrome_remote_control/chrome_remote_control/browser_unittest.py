# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest
import browser
import browser_finder
import browser_options

class BrowserTest(unittest.TestCase):
  def testBasic(self):
    options = browser_options.options_for_unittests
    browser_to_create = browser_finder.FindBrowser(options)
    if not browser_to_create:
      raise Exception('No browser found, cannot continue test.')
    with browser_to_create.Create() as b:
      self.assertEquals(1, b.num_tabs)

      # Different browsers boot up to different things
      assert b.GetNthTabUrl(0)

  def testCommandLineOverriding(self):
    # This test starts the browser with --enable-benchmarking, which should
    # create a chrome.Interval namespace. This tests whether the command line is
    # being set.
    options = browser_options.options_for_unittests
    testJS = ("window.chrome.gpuBenchmarking !== undefined ||" +
             "chrome.Interval !== undefined")

    flag1 = "--user-agent=chrome_remote_control"
    options.extra_browser_args.append(flag1)
    try:
      browser_to_create = browser_finder.FindBrowser(options)
      with browser_to_create.Create() as b:
        with b.ConnectToNthTab(0) as t:
          t.page.Navigate("http://www.google.com/")
          t.WaitForDocumentReadyStateToBeInteractiveOrBetter()
          self.assertEquals(t.runtime.Evaluate('navigator.userAgent'),
                            'chrome_remote_control')

    finally:
      options.extra_browser_args.remove(flag1)
