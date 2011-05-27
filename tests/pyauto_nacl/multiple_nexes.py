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
  # (test.html, number of nexes loaded concurrently on page)
  nexes = [('basic_object.html', 2),
           ('earth_c.html', 1),
           ('earth_cc.html', 1),
           ('ppapi_core.html', 1),
           ('ppapi_example_audio.html#mute', 1),
           ('ppapi_file_system.html', 1),
           ('ppapi_geturl.html', 1),
           ('ppapi_progress_events.html', 1),
           # TODO(nfullagar): graphics 2d appears to lock up while waiting
           # for the replace contents flush callback. Enable this test when
           # we've figured out why this occurs and the problem is fixed.
           # ('ppapi_ppb_graphics2d.html', 1),
           ('srpc_basic.html', 1),
           ('srpc_hw.html', 1),
           # TODO(nfullagar): enable this test when it works on trybots.
           # ('srpc_hw_fd.html', 1),
           ('srpc_nrd_xfer.html', 2),
           ('srpc_plugin.html', 1),
           ('srpc_shm.html', 1),
           ('srpc_sockaddr.html', 1),
           ('srpc_url_as_nacl_desc.html', 1)]

  def testLoadNexesInMultipleTabs(self):
    """Load nexes in multiple tabs and surf away from all of them."""

    # Prime each tab by navigating to about:version.
    # TODO(mcgrathr): Reduced from 10 to 6 because 256MB*10 is too
    # much /dev/shm space for the bots to handle.
    # See http://code.google.com/p/nativeclient/issues/detail?id=503
    max_nexes = 6
    max_tabs = 6
    max_attempts = max_nexes * 10
    num_iterations = 10
    self.NavigateToURL('about:version')
    original_title = self.GetActiveTabTitle()
    for i in range(1, max_tabs):
      self.AppendTab(pyauto.GURL('about:version'))

    for j in range(0, num_iterations):
      # Pick a test for each tab and navigate to it.
      # Some tests have more than one nexe on the page, limit the total to
      # max_nexes, due to mcgrathr's /dev/shm note above.
      num_nexes = 0
      num_tabs = 0
      num_attempts = 0
      while (num_nexes < max_nexes) and (num_attempts < max_attempts):
        page_url, page_nexes = random.choice(NaClTest.nexes)
        if num_nexes + page_nexes <= max_nexes:
          self.GetBrowserWindow(0).GetTab(num_tabs).NavigateToURL(pyauto.GURL(
            self.GetHttpURLForDataPath(page_url)))
          num_nexes = num_nexes + page_nexes
          num_tabs = num_tabs + 1
        num_attempts = num_attempts + 1

      # Wait for all the tabs to fully load.
      for i in range(0, num_tabs):
        nacl_utils.WaitForNexeLoad(self, tab_index=i)

      # Make sure every tab successfully passed test(s).
      for i in range(0, num_tabs):
        nacl_utils.VerifyAllTestsPassed(self, tab_index=i)

      # Surf away from each tab and verify no crash occurred.
      for i in range(0, num_tabs):
        self.GetBrowserWindow(0).GetTab(i).GoBack()
        self.assertEqual(original_title, self.GetActiveTabTitle())

  def testLoadMultipleNexesInOneTab(self):
    """Load multiple nexes in one tab and load them one after another."""

    # Prime a tab by navigating to about:version.
    self.NavigateToURL('about:version')
    original_title = self.GetActiveTabTitle()

    # Navigate to a nexe and make sure it loads. Repeate for all nexes.
    for page_info in NaClTest.nexes:
      page_url, page_nexes = page_info
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
