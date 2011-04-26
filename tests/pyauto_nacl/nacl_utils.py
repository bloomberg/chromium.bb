#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_nacl  # Must be imported before pyauto
import pyauto


def WaitForNexeLoad(browser, tab_index=0):
  """Waits until a nexe has been fully loaded by the browser in the tab
  tab_index. Assumes that the caller has actually navigated to a nexe prior to
  calling this method. Also assumes that the nexe displays the string
  '[SHUTDOWN]' after fully loading.
  """
  browser.assertTrue(browser.WaitUntil(
      lambda:
      browser.FindInPage('[SHUTDOWN]', tab_index=tab_index)['match_count'],
      expect_retval=1))


def VerifyAllTestsPassed(browser, tab_index=0):
  """Returns true if all tests run by a nexe that was loaded by the browser in
  the tab tab_index have passed. Assumes that the nexe has fully loaded. Also
  assumes that the nexe displays the string '0 failed, 0 errors' when all tests
  have passed.
  """
  browser.assertTrue(browser.FindInPage(
      '0 failed, 0 errors', tab_index=tab_index)['match_count'] == 1)
