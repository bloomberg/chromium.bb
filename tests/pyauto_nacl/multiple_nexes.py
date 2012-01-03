#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_nacl  # Must be imported before pyauto
import pyauto
import nacl_utils
import random

class NaClTest(pyauto.PyUITest):
  """Tests for NaCl."""
  # (test.html, number of nexes loaded concurrently on page)
  nexes = [('ppapi_example_audio.html#mute', 1),
           ('ppapi_example_font.html', 1),
           # TODO(cstefansen): enable test when bug is fixed
           # http://code.google.com/p/nativeclient/issues/detail?id=1936
           # ('ppapi_example_gles2.html', 1),
           ('ppapi_example_post_message.html', 1),
           ('ppapi_geturl.html', 1),
           # TODO(dspringer): enable tests when 3D ABI is stable.
           # http://code.google.com/p/nativeclient/issues/detail?id=2060
           # ('ppapi_gles_book.html?manifest='
           #  'ppapi_gles_book_hello_triangle.nmf', 1),
           # ('ppapi_gles_book.html?manifest='
           #  'ppapi_gles_book_mip_map_2d.nmf', 1),
           # ('ppapi_gles_book.html?manifest='
           #  'ppapi_gles_book_simple_texture_2d.nmf', 1),
           # ('ppapi_gles_book.html?manifest='
           #  'ppapi_gles_book_simple_texture_cubemap.nmf', 1),
           # ('ppapi_gles_book.html?manifest='
           #  'ppapi_gles_book_simple_vertex_shader.nmf', 1),
           # ('ppapi_gles_book.html?manifest='
           #  'ppapi_gles_book_stencil_test.nmf', 1),
           # ('ppapi_gles_book.html?manifest='
           #  'ppapi_gles_book_texture_wrap.nmf', 1),
           ('ppapi_progress_events.html', 1),
           # TODO(ncbray): reenable
           # http://code.google.com/p/nativeclient/issues/detail?id=2448
           #('ppapi_ppb_core.html', 1),
           ('ppapi_ppb_file_system.html', 1),
           ('ppapi_ppb_graphics2d.html', 1),
           # TODO(nfullagar): reenable
           # http://code.google.com/p/chromium/issues/detail?id=93806
           # ('ppapi_ppb_image_data.html', 1),
           ('srpc_basic.html', 1),
           ('srpc_hw.html', 1),
           ('srpc_nrd_xfer.html', 2)]

  def testLoadNexesInMultipleTabs(self):
    """Load nexes in multiple tabs and surf away from all of them."""

    # Prime each tab by navigating to about:version.
    # TODO(mcgrathr): Reduced from 10 to 6 because 256MB*10 is too
    # much /dev/shm space for the bots to handle.
    # See http://code.google.com/p/nativeclient/issues/detail?id=503
    max_nexes = 6
    max_tabs = 6
    max_attempts = max_nexes * 10
    num_iterations = 5
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
          # Print combination of tests to output
          print '---> pyauto multiple_nexes: Tab', num_tabs, 'running',
          print page_url, 'featuring', page_nexes, 'nexes.'
          num_nexes = num_nexes + page_nexes
          num_tabs = num_tabs + 1
        num_attempts = num_attempts + 1

      # Wait for all the tabs to fully load.
      for i in range(0, num_tabs):
        print '---> pyauto multiple_nexes: Loading tab', str(i) + '...',
        # Make the tab active to make it visible, which is required
        # to receive pending Flush completion callbacks in the tests
        # that use ppb_graphics_2d.
        self.GetBrowserWindow(0).ActivateTab(i)
        nacl_utils.WaitForNexeLoad(self, tab_index=i)
        print 'done!'
      print '---> pyauto multiple_nexes: All nexes loaded.'

      # Make sure every tab successfully passed test(s).
      for i in range(0, num_tabs):
        print '---> pyauto multiple_nexes: Verifying tab ', str(i) + '...',
        self.GetBrowserWindow(0).ActivateTab(i)
        nacl_utils.VerifyAllTestsPassed(self, tab_index=i)
        print 'done!'
      print '---> pyauto multiple_nexes: All nexes verified.'

      # Surf away from each tab and verify no crash occurred.
      for i in range(0, num_tabs):
        print '---> pyauto multiple_nexes: Surf back on tab ', str(i) + '...',
        self.GetBrowserWindow(0).GetTab(i).GoBack()
        self.assertEqual(original_title, self.GetActiveTabTitle())
        print 'done!'

  def testLoadMultipleNexesInOneTab(self):
    """Load multiple nexes in one tab and load them one after another."""

    # Prime a tab by navigating to about:version.
    self.NavigateToURL('about:version')
    original_title = self.GetActiveTabTitle()

    # Navigate to a nexe and make sure it loads. Repeat for all nexes.
    for page_info in NaClTest.nexes:
      page_url, page_nexes = page_info
      print '---> pyauto multiple_nexes: Navigating to', str(page_url) + '...',
      self.NavigateToURL(self.GetHttpURLForDataPath(page_url))
      print 'done!'
      print '---> pyauto multiple_nexes: Loading', str(page_url) + '...',
      nacl_utils.WaitForNexeLoad(self)
      print 'done!'
      print '---> pyauto multiple_nexes: Verifying', str(page_url) + '...',
      nacl_utils.VerifyAllTestsPassed(self)
      print 'done!'

    # Keep hitting the back button and make sure all the nexes load.
    print '---> pyauto multiple_nexes: Surfing back:'
    for i in range(0, len(NaClTest.nexes) - 1):
      self.GetBrowserWindow(0).GetTab(0).GoBack()
      print '---> pyauto multiple_nexes: Loading...',
      nacl_utils.WaitForNexeLoad(self)
      print 'done!'
      print '---> pyauto multiple_nexes: Verifying...',
      nacl_utils.VerifyAllTestsPassed(self)
      print 'done!'

    # Go back one last time and make sure we ended up where we started.
    print '---> pyauto multiple_nexes: Checking for about:version...',
    self.GetBrowserWindow(0).GetTab(0).GoBack()
    self.assertEqual(original_title, self.GetActiveTabTitle())
    print 'done!'


if __name__ == '__main__':
  pyauto_nacl.Main()
