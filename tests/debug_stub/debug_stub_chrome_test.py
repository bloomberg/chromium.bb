#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os.path
import sys

# Allow the import of third party modules
script_dir = os.path.dirname(__file__)
sys.path.append(os.path.join(script_dir, '../pyauto_nacl/'))

import pyauto_nacl  # Must be imported before pyauto
import pyauto
import nacl_utils
import re
import thread
import time

# Import debug_stub_test for RSP connectivity
import debug_stub_test

class NaClTest(pyauto.PyUITest):
  """Tests for NaCl."""

  def chromeDebug(self, page, title_word):
    self.debug_socket_opened = False

    def listen():
      print 'About to open GdbRsp connection'
      connection = debug_stub_test.GdbRspConnection()
      self.debug_socket_opened = True
      # Query the debug_stub for architecture string
      reply = connection.RspRequest('qXfer:features:read:target.xml:0,fff')
      # Make sure string returned includes the word 'architecture'
      self.assertTrue(re.search('architecture', reply), reply)
      # Query for reason halted -- should return S0 and a number
      reply = connection.RspRequest('?')
      # Validate we stopped for some kind of exception
      self.assertTrue(re.match('^S0', reply))
      # Just send a "c" so that the nexe continues. No more breakpoints
      # are expected, so don't wait for a reply.
      reply = connection.RspSendOnly('c')
      print 'Sent a "c"'

    # Start a thread that will send RSP debug commands on port 4014.
    # The main thread will open a url in chrome, which was launched with
    # --enable-nacl-debug, which will open port 4014.
    thread.start_new_thread(listen, ())

    url = self.GetHttpURLForDataPath(page)
    self.NavigateToURL(url)
    nacl_utils.WaitForNexeLoad(self)
    nacl_utils.VerifyAllTestsPassed(self)
    # The debug socket must have opened for this test to pass, even if
    # the nexe loaded and ran.
    self.assertTrue(self.debug_socket_opened)

  def testChromeDebug(self):
    self.chromeDebug('debug_browser.html', 'PPAPI')


if __name__ == '__main__':
  pyauto_nacl.Main()
