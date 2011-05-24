#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_nacl  # Must be imported before pyauto
import pyauto
import nacl_utils
import random

class NaClTest(pyauto.PyUITest):
  """Tests for NaCl."""
  nexes = ['basic_object.html',
           'earth_c.html',
           'earth_cc.html',
           'ppapi_core.html',
           'ppapi_example_audio.html#mute',
           'ppapi_file_system.html',
           'ppapi_geturl.html',
           'ppapi_progress_events.html',
           # TODO(nfullagar): graphics 2d appears to lock up while waiting
           # for the replace contents flush callback. Enable this test when
           # we've figured out why this occurs and the problem is fixed.
           # 'ppapi_ppb_graphics2d.html',
           'srpc_basic.html',
           'srpc_hw.html',
           # TODO(nfullagar): enable this test when it works on trybots.
           # 'srpc_hw_fd.html',
           'srpc_nrd_xfer.html',
           'srpc_plugin.html',
           'srpc_shm.html',
           'srpc_sockaddr.html',
           'srpc_url_as_nacl_desc.html']

  def testLoadNexesInMultipleTabs(self):
    """Load nexes in multiple tabs and surf away from all of them."""

    # Prime each tab by navigating to about:version.
    # TODO(mcgrathr): Reduced from 10 to 5 because 256MB*10 is too
    # much /dev/shm space for the bots to handle.
    # See http://code.google.com/p/nativeclient/issues/detail?id=503
    num_tabs = 5
    num_iterations = 10
    self.NavigateToURL('about:version')
    original_title = self.GetActiveTabTitle()
    for i in range(1, num_tabs):
      self.AppendTab(pyauto.GURL('about:version'))

    for j in range(0, num_iterations):
      # Pick a nexe for each tab and navigate to it.
      for i in range(0, num_tabs):
        page_url = random.choice(NaClTest.nexes)
        self.GetBrowserWindow(0).GetTab(i).NavigateToURL(pyauto.GURL(
            self.GetHttpURLForDataPath(page_url)))

      # Wait for all the nexes to fully load.
      for i in range(0, num_tabs):
        nacl_utils.WaitForNexeLoad(self, tab_index=i)

      # Make sure every nexe successfully passed test(s).
      for i in range(0, num_tabs):
        nacl_utils.VerifyAllTestsPassed(self, tab_index=i)

      # Surf away from each nexe and verify that the tab didn't crash.
      for i in range(0, num_tabs):
        self.GetBrowserWindow(0).GetTab(i).GoBack()
        self.assertEqual(original_title, self.GetActiveTabTitle())

  def testLoadMultipleNexesInOneTab(self):
    """Load multiple nexes in one tab and load them one after another."""

    # Prime a tab by navigating to about:version.
    self.NavigateToURL('about:version')
    original_title = self.GetActiveTabTitle()

    # Navigate to a nexe and make sure it loads. Repeate for all nexes.
    for page_url in NaClTest.nexes:
      self.NavigateToURL(self.GetHttpURLForDataPath(page_url))
      nacl_utils.WaitForNexeLoad(self)
      nacl_utils.VerifyAllTestsPassed(self)

    # Keep hitting the back button and make sure all the nexes load.
    for i in range(0, len(NaClTest.nexes) - 1):
      self.GetBrowserWindow(0).GetTab(0).GoBack()
      nacl_utils.WaitForNexeLoad(self)
      nacl_utils.VerifyAllTestsPassed(self)

    # Go back one last time and make sure we ended up where we started.
    self.GetBrowserWindow(0).GetTab(0).GoBack()
    self.assertEqual(original_title, self.GetActiveTabTitle())


if __name__ == '__main__':
  pyauto_nacl.Main()
