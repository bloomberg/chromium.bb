#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_nacl  # Must be imported before pyauto
import pyauto


class NaClTest(pyauto.PyUITest):
  """Tests for NaCl."""

  def testSurfAway(self):
    """Navigate to the hello world nexe and then surf away."""
    self.NavigateToURL('about:version')
    title = self.GetActiveTabTitle()
    url = self.GetHttpURLForDataPath('srpc_hw.html')
    self.NavigateToURL(url)
    self.WaitUntil(
        lambda: self.FindInPage('[SHUTDOWN]')['match_count'],
        expect_retval=1)
    self.assertEqual(1, self.FindInPage(
        '[SHUTDOWN] 2 passed, 0 failed')['match_count'])
    self.GetBrowserWindow(0).GetTab(0).GoBack()
    self.assertEqual(title, self.GetActiveTabTitle())


if __name__ == '__main__':
  pyauto_nacl.Main()
