#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_nacl  # Must be imported before pyauto
import pyauto
import nacl_utils


class NaClTest(pyauto.PyUITest):
  """Tests for NaCl."""

  def surfAwayMulti(self, page, title_word, num_tries):
    self.NavigateToURL('about:version')
    version_title = self.GetActiveTabTitle()
    for x in range(0, num_tries):
      # Load the page, run the tests, verify the tests pass,
      # verify that the page title contains the title key word,
      # surf back and verify that previous page is reached.
      url = self.GetHttpURLForDataPath(page)
      self.NavigateToURL(url)
      nacl_utils.WaitForNexeLoad(self)
      nacl_utils.VerifyAllTestsPassed(self)
      page_title = self.GetActiveTabTitle()
      self.assertNotEqual(page_title.upper().find(title_word.upper()), -1)
      self.GetBrowserWindow(0).GetTab(0).GoBack()
      self.assertEqual(version_title, self.GetActiveTabTitle())

  def surfAway(self, page, title_word):
    """Navigate multiple times to a sample nexe and then surf away."""
    self.surfAwayMulti(page, title_word, 5)

  def testSurfAwaySRPCHelloWorld(self):
    self.surfAway('srpc_hw.html', 'SRPC')

  def testSurfAwaySRPCParameterPassing(self):
    self.surfAway('srpc_basic.html', 'SRPC')

  def testSurfAwaySRPCResourceDescriptor(self):
    self.surfAway('srpc_nrd_xfer.html', 'SRPC')

  def testSurfAwayExampleAudio(self):
    self.surfAway('ppapi_example_audio.html#mute', 'PPAPI')

  def testSurfAwayExampleFont(self):
    self.surfAway('ppapi_example_font.html', 'PPAPI')

  # TODO(cstefansen): enable test when bug is fixed
  # http://code.google.com/p/nativeclient/issues/detail?id=1936
  def disabledTestSurfAwayExampleGLES2(self):
    self.surfAway('ppapi_example_gles2.html', 'PPAPI')

  def testSurfAwayGetURL(self):
    self.surfAway('ppapi_geturl.html', 'PPAPI')

  # TODO(dspringer): enable test when 3D ABI is stable.
  # http://code.google.com/p/nativeclient/issues/detail?id=2060
  def disabledTestSurfAwayGLESBookHelloTriangle(self):
    self.surfAway('ppapi_gles_book.html?manifest='
                  'ppapi_gles_book_hello_triangle.nmf', 'PPAPI')

  # TODO(dspringer): enable test when 3D ABI is stable.
  # http://code.google.com/p/nativeclient/issues/detail?id=2060
  def disabledTestSurfAwayGLESBookMipMap2D(self):
    self.surfAway('ppapi_gles_book.html?manifest='
                  'ppapi_gles_book_mip_map_2d.nmf', 'PPAPI')

  # TODO(dspringer): enable test when 3D ABI is stable.
  # http://code.google.com/p/nativeclient/issues/detail?id=2060
  def disabledTestSurfAwayGLESBookSimpleTexture2D(self):
    self.surfAway('ppapi_gles_book.html?manifest='
                  'ppapi_gles_book_simple_texture_2d.nmf', 'PPAPI')

  # TODO(dspringer): enable test when 3D ABI is stable.
  # http://code.google.com/p/nativeclient/issues/detail?id=2060
  def disabledTestSurfAwayGLESBookSimpleTextureCubemap(self):
    self.surfAway('ppapi_gles_book.html?manifest='
                  'ppapi_gles_book_simple_texture_cubemap.nmf', 'PPAPI')

  # TODO(dspringer): enable test when 3D ABI is stable.
  # http://code.google.com/p/nativeclient/issues/detail?id=2060
  def disabledTestSurfAwayGLESBookSimpleVertexShade(self):
    self.surfAway('ppapi_gles_book.html?manifest='
                  'ppapi_gles_book_simple_vertex_shader.nmf', 'PPAPI')

  # TODO(dspringer): enable test when 3D ABI is stable.
  # http://code.google.com/p/nativeclient/issues/detail?id=2060
  def disabledTestSurfAwayGLESBookStencilTest(self):
    self.surfAway('ppapi_gles_book.html?manifest='
                  'ppapi_gles_book_stencil_test.nmf', 'PPAPI')

  # TODO(dspringer): enable test when 3D ABI is stable.
  # http://code.google.com/p/nativeclient/issues/detail?id=2060
  def disabledTestSurfAwayGLESBookTextureWrap(self):
    self.surfAway('ppapi_gles_book.html?manifest='
                  'ppapi_gles_book_texture_wrap.nmf', 'PPAPI')

  def testSurfAwayEarthC(self):
    self.surfAway('earth_c.html', 'Globe')

  def testSurfAwayEarthCC(self):
    self.surfAway('earth_cc.html', 'Globe')

  def disabledTestSurfAwayProgressEvents(self):
    self.surfAway('ppapi_progress_events.html', 'PPAPI')

  # TODO(ncbray): reenable
  # http://code.google.com/p/nativeclient/issues/detail?id=2448
  def disabledTestSurfAwayPPBCore(self):
    self.surfAway('ppapi_ppb_core.html', 'PPAPI')

  def testSurfAwayPPBGraphics2D(self):
    self.surfAway('ppapi_ppb_graphics2d.html', 'PPAPI')

  def testSurfAwayPPBImageData(self):
    self.surfAway('ppapi_ppb_image_data.html', 'PPAPI')

  def testSurfAwayPPBFileSystem(self):
    self.surfAway('ppapi_ppb_file_system.html', 'PPAPI')

if __name__ == '__main__':
  pyauto_nacl.Main()
