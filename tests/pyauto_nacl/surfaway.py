#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_nacl  # Must be imported before pyauto
import pyauto
import nacl_utils


class NaClTest(pyauto.PyUITest):
  """Tests for NaCl."""

  def testSurfAway(self):
    """Navigate to a sample nexe and then surf away."""
    self.NavigateToURL('about:version')
    title = self.GetActiveTabTitle()
    url = self.GetHttpURLForDataPath('ppapi_geturl.html')
    self.NavigateToURL(url)
    nacl_utils.WaitForNexeLoad(self)
    nacl_utils.VerifyAllTestsPassed(self)
    self.GetBrowserWindow(0).GetTab(0).GoBack()
    self.assertEqual(title, self.GetActiveTabTitle())


if __name__ == '__main__':
  pyauto_nacl.Main()
