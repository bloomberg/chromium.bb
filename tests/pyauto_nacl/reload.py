#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_nacl  # Must be imported before pyauto
import pyauto
import nacl_utils
import random
import time


class NaClTest(pyauto.PyUITest):
  """Tests for NaCl."""

  def reloadMulti(self, page, title_word, num_tries, wait_min, wait_max):
    tab = self.GetBrowserWindow(0).GetTab(0)
    url = self.GetHttpURLForDataPath(page)
    tab.NavigateToURL(pyauto.GURL(url))
    page_title = self.GetActiveTabTitle()
    # Verify that test page exists by case insensitive title keyword check.
    self.assertNotEqual(page_title.upper().find(title_word.upper()), -1)
    # Surf to about:version as reference point for GoBack().
    tab.NavigateToURL(pyauto.GURL('about:version'))
    version_title = self.GetActiveTabTitle()
    # For a given page:
    #  - Surf to the test page.
    #  - Wait until the expected page title to appear.
    #  - Wait a random amount of time between wait_min..wait_max.
    #  - For num_tries, attempt reload w/ new random timeout
    #  - On the last try, verify the test ran and completed successfully
    #  - GoBack()
    #  - Verify a page snap didn't occur by checking the page title;
    #    expect to be back on the about:version page.
    # This test intentionally tries to interrupt the page during mid-load,
    # so it does not wait for the test to complete, except on the last
    # iteration.
    tab.NavigateToURLAsync(pyauto.GURL(url))
    nacl_utils.AssertNoCrash(self)
    self.WaitUntil(lambda: self.GetActiveTabTitle() == page_title)
    for i in range(0, num_tries):
      if wait_max > 0:
        wait_duration = random.uniform(wait_min, wait_max)
        stop_time = time.time() + wait_duration
        self.WaitUntil(lambda: time.time() > stop_time)
      self.assertEqual(page_title, self.GetActiveTabTitle())
      tab.Reload()
      nacl_utils.AssertNoCrash(self)
      self.WaitUntil(lambda: self.GetActiveTabTitle() == page_title)
    # After last reload, wait for test to complete before going back
    nacl_utils.WaitForNexeLoad(self)
    nacl_utils.VerifyAllTestsPassed(self)
    tab.GoBack()
    nacl_utils.AssertNoCrash(self)
    self.assertEqual(version_title, self.GetActiveTabTitle())

  def reloader(self, page, title_word):
    """Navigate to PPAPI page and surf away asynchronously."""
    print '---> pyauto reload: start testing', page
    # Repeatedly (5 times) immediately reload
    self.reloadMulti(page, title_word, 5, 0, 0)
    # Repeatedly (25 times) reload after a random delay (0 - 2 seconds)
    self.reloadMulti(page, title_word, 25, 0.0, 2.0)
    print '---> pyauto reload: finished testing', page

  def testReloadSRPCHelloWorld(self):
    self.reloader('srpc_hw.html', 'SRPC')

  def testReloadSRPCParameterPassing(self):
    self.reloader('srpc_basic.html', 'SRPC')

  def testReloadSRPCResourceDescriptor(self):
    self.reloader('srpc_nrd_xfer.html', 'SRPC')

  def testReloadExampleAudio(self):
    self.reloader('ppapi_example_audio.html#mute', 'PPAPI')

  def testReloadExampleFont(self):
    self.reloader('ppapi_example_font.html', 'PPAPI')

  # TODO(cstefansen): enable test when bug is fixed
  # http://code.google.com/p/nativeclient/issues/detail?id=1936
  def disabledTestReloadExampleGLES2(self):
    self.reloader('ppapi_example_gles2.html', 'PPAPI')

  def testReloadExamplePostMessage(self):
    self.reloader('ppapi_example_post_message.html', 'PPAPI')

  def testReloadGetURL(self):
    self.reloader('ppapi_geturl.html', 'PPAPI')

  # TODO(dspringer): enable test when 3D ABI is stable.
  # http://code.google.com/p/nativeclient/issues/detail?id=2060
  def disabledTestReloadGLESBookHelloTriangle(self):
    self.reloader('ppapi_gles_book.html?manifest='
                  'ppapi_gles_book_hello_triangle.nmf', 'PPAPI')

  # TODO(dspringer): enable test when 3D ABI is stable.
  # http://code.google.com/p/nativeclient/issues/detail?id=2060
  def disabledTestReloadGLESBookMipMap2D(self):
    self.reloader('ppapi_gles_book.html?manifest='
                  'ppapi_gles_book_mip_map_2d.nmf', 'PPAPI')

  # TODO(dspringer): enable test when 3D ABI is stable.
  # http://code.google.com/p/nativeclient/issues/detail?id=2060
  def disabledTestReloadGLESBookSimpleTexture2D(self):
    self.reloader('ppapi_gles_book.html?manifest='
                  'ppapi_gles_book_simple_texture_2d.nmf', 'PPAPI')

  # TODO(dspringer): enable test when 3D ABI is stable.
  # http://code.google.com/p/nativeclient/issues/detail?id=2060
  def disabledTestReloadGLESBookSimpleTextureCubemap(self):
    self.reloader('ppapi_gles_book.html?manifest='
                  'ppapi_gles_book_simple_texture_cubemap.nmf', 'PPAPI')

  # TODO(dspringer): enable test when 3D ABI is stable.
  # http://code.google.com/p/nativeclient/issues/detail?id=2060
  def disabledTestReloadGLESBookSimpleVertexShader(self):
    self.reloader('ppapi_gles_book.html?manifest='
                  'ppapi_gles_book_simple_vertex_shader.nmf', 'PPAPI')

  # TODO(dspringer): enable test when 3D ABI is stable.
  # http://code.google.com/p/nativeclient/issues/detail?id=2060
  def disabledTestReloadGLESBookStencilTest(self):
    self.reloader('ppapi_gles_book.html?manifest='
                  'ppapi_gles_book_stencil_test.nmf', 'PPAPI')

  # TODO(dspringer): enable test when 3D ABI is stable.
  # http://code.google.com/p/nativeclient/issues/detail?id=2060
  def disabledTestReloadGLESBookTextureWrap(self):
    self.reloader('ppapi_gles_book.html?manifest='
                  'ppapi_gles_book_texture_wrap.nmf', 'PPAPI')

  def testReloadProgressEvents(self):
    self.reloader('ppapi_progress_events.html', 'PPAPI')

  # TODO(ncbray): reenable
  # http://code.google.com/p/nativeclient/issues/detail?id=2448
  def disabledTestReloadPPBCore(self):
    self.reloader('ppapi_ppb_core.html', 'PPAPI')

  def testReloadPPBGraphics2D(self):
    self.reloader('ppapi_ppb_graphics2d.html', 'PPAPI')

  def testReloadPPBImageData(self):
    self.reloader('ppapi_ppb_image_data.html', 'PPAPI')

  def testReloadPPBFileSystem(self):
    self.reloader('ppapi_ppb_file_system.html', 'PPAPI')


if __name__ == '__main__':
  pyauto_nacl.Main()
