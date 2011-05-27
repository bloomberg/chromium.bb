#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_nacl  # Must be imported before pyauto
import pyauto
import nacl_utils
import random
import time

class NaClTest(pyauto.PyUITest):
  """Tests for NaCl."""

  def surfAwayAsyncMulti(self, page, title_word, num_tries, wait_min, wait_max):
    tab = self.GetBrowserWindow(0).GetTab(0)
    url = self.GetHttpURLForDataPath(page)
    tab.NavigateToURL(pyauto.GURL(url))
    page_title = self.GetActiveTabTitle()
    # Verify that test page exists by case insensitive title keyword check.
    self.assertNotEqual(page_title.upper().find(title_word.upper()), -1)
    # Surf to about:version as reference point for GoBack().
    tab.NavigateToURL(pyauto.GURL('about:version'))
    version_title = self.GetActiveTabTitle()
    # For a given number of tries:
    #  - Surf to the test page.
    #  - Wait until the expected page title to appear.
    #  - Wait a random amount of time between wait_min..wait_max.
    #  - Surf away via GoBack()
    #  - Verify a page snap didn't occur by checking the page title;
    #    expect to be back on the about:version page.
    # This test intentionally tries to interrupt the page during mid-load,
    # so it does not wait for the test to complete.
    for i in range(0, num_tries):
      tab.NavigateToURLAsync(pyauto.GURL(url))
      self.WaitUntil(lambda: self.GetActiveTabTitle() == page_title)
      if wait_max > 0:
        wait_duration = random.uniform(wait_min, wait_max)
        stop_time = time.time() + wait_duration
        self.WaitUntil(lambda: time.time() > stop_time)
      self.assertEqual(page_title, self.GetActiveTabTitle())
      tab.GoBack()
      self.assertEqual(version_title, self.GetActiveTabTitle())

  def surfAwayAsync(self, page, title_word):
    """Navigate to PPAPI page and surf away asynchronously."""
    # Repeatedly (5 times) immediately surfaway
    self.surfAwayAsyncMulti(page, title_word, 5, 0, 0)
    # Repeatedly (25 times) surfaway after a random delay (0 - 2 seconds)
    self.surfAwayAsyncMulti(page, title_word, 25, 0.0, 2.0)

  def testSurfAwayAsyncSRPCHelloWorld(self):
    self.surfAwayAsync('srpc_hw.html', 'SRPC')

  def testSurfAwayAsyncSRPCParameterPassing(self):
    self.surfAwayAsync('srpc_basic.html', 'SRPC')

  def testSurfAwayAsyncSRPCPluginProperties(self):
    self.surfAwayAsync('srpc_plugin.html', 'SRPC')

  def testSurfAwayAsyncSRPCSocketAddress(self):
    self.surfAwayAsync('srpc_sockaddr.html', 'SRPC')

  def testSurfAwayAsyncSRPCSharedMemory(self):
    self.surfAwayAsync('srpc_shm.html', 'SRPC')

  def testSurfAwayAsyncSRPCResourceDescriptor(self):
    self.surfAwayAsync('srpc_nrd_xfer.html', 'SRPC')

  def testSurfAwayAsyncSRPCHelloWorldFileDescriptor(self):
    self.surfAwayAsync('srpc_hw_fd.html', 'SRPC')

  def testSurfAwayAsyncSRPCURLContentAsNaclResourceDescriptor(self):
    self.surfAwayAsync('srpc_url_as_nacl_desc.html', 'SRPC')

  def testSurfAwayAsyncScripting(self):
    self.surfAwayAsync('basic_object.html', 'PPAPI')

  def testSurfAwayAsyncEvents(self):
    self.surfAwayAsync('event_example.html', 'PPAPI')

  def testSurfAwayAsyncExampleAudio(self):
    self.surfAwayAsync('ppapi_example_audio.html#mute', 'PPAPI')

  def testSurfAwayAsyncGetURL(self):
    self.surfAwayAsync('ppapi_geturl.html', 'PPAPI')

  def testSurfAwayAsyncEarthC(self):
    self.surfAwayAsync('earth_c.html', 'Globe')

  def testSurfAwayAsyncEarthCC(self):
    self.surfAwayAsync('earth_cc.html', 'Globe')

  def testSurfAwayAsyncProgressEvents(self):
    self.surfAwayAsync('ppapi_progress_events.html', 'PPAPI')

  def testSurfAwayAsyncExample2D(self):
    self.surfAwayAsync('ppapi_example_2d.html', 'PPAPI')

  def testSurfAwayAsyncPPBCore(self):
    self.surfAwayAsync('ppapi_core.html', 'PPAPI')

  def testSurfAwayAsyncPPBGraphics2D(self):
    self.surfAwayAsync('ppapi_ppb_graphics2d.html', 'PPAPI')

  def testSurfAwayAsyncPPBFileSystem(self):
    self.surfAwayAsync('ppapi_file_system.html', 'PPAPI')

if __name__ == '__main__':
  pyauto_nacl.Main()
