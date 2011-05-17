#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_nacl  # Must be imported before pyauto
import pyauto

def AssertTrueOrLogTab(browser, ok, msg, tab_index=0):
  """If passed False, scrapes the content of the tab and prints it before
  throwing an axception.
  """
  try:
    browser.assertTrue(ok, msg)
  except AssertionError:
    # NOTE!  A \n inside """ is a newline, not the text \n!
    # Hence \\n to get \n into the JavaScript source.
    js = """\
var results = document.getElementById('testresults');
if (results == null) {
  window.domAutomationController.send('*** FAILED TO FIND id="testresults"\\n');
} else {
  var lines = results.childNodes;
  var text = '*** ' + lines.length + ' lines:\\n';
  for (var i = 0; i < lines.length; ++i) {
    text = text + '*** ' + lines[i].innerHTML + '\\n';
  }
  window.domAutomationController.send(text);
}
"""
    print ('*** FAILED TEST ON TAB %u (%s)!  Log follows. ***' %
           (tab_index, msg))
    print browser.ExecuteJavascript(js, 0, tab_index),
    print '*** END OF FAILED TAB LOG ***'
    raise

def WaitForNexeLoad(browser, tab_index=0):
  """Waits until a nexe has been fully loaded by the browser in the tab
  tab_index. Assumes that the caller has actually navigated to a nexe prior to
  calling this method. Also assumes that the nexe displays the string
  '[SHUTDOWN]' after fully loading.
  """
  AssertTrueOrLogTab(browser, browser.WaitUntil(
      lambda:
        browser.FindInPage('[SHUTDOWN]', tab_index=tab_index)['match_count'],
      expect_retval=1),
                     'nexe did not load',
                     tab_index)

def VerifyAllTestsPassed(browser, tab_index=0):
  """Returns true if all tests run by a nexe that was loaded by the browser in
  the tab tab_index have passed. Assumes that the nexe has fully loaded. Also
  assumes that the nexe displays the string '0 failed, 0 errors' when all tests
  have passed.
  """
  AssertTrueOrLogTab(browser, browser.FindInPage(
      '0 failed, 0 errors', tab_index=tab_index)['match_count'] == 1,
                     'nexe test did not report success',
                     tab_index)
