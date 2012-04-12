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

import gdb_rsp


class NaClTest(pyauto.PyUITest):
  """Tests for NaCl."""

  def chromeDebug(self, page, title_word):
    self.debug_socket_opened = False

    # This function will open port 4014 and send some RSP commands.
    # Last of all, it must send a 'c' command (continue) so that
    # the nexe will continue (past the initial breakpoint) and
    # load.
    def listen():
      print 'About to open GdbRsp connection'
      connection = gdb_rsp.GdbRspConnection()
      print 'Opened debug connection'
      self.debug_socket_opened = True
      # Query the debug_stub for architecture string
      reply = connection.RspRequest('qXfer:features:read:target.xml:0,fff')
      print 'reply to features is %s' % reply
      # Make sure string returned includes the word 'architecture'
      self.assertTrue(re.search('architecture', reply), reply)
      # Query for reason halted -- should return S0 and a number
      reply = connection.RspRequest('?')
      print 'reply to ? is %s' % reply
      # Validate we stopped for some kind of exception
      self.assertTrue(re.match('^S0', reply))
      # Just send a "c" so that the nexe continues. No more breakpoints
      # are expected, so don't wait for a reply.
      reply = connection.RspSendOnly('c')
      print 'Sent a "c"'

    # Navigate to the page that has the nexe
    url = self.GetHttpURLForDataPath(page)
    self.NavigateToURL(url)

    # Because sel_ldr will break on entry, we need to call the listen function
    # so that it will tell sel_ldr to continue.
    listen()
    nacl_utils.WaitForNexeLoad(self)
    nacl_utils.VerifyAllTestsPassed(self)
    # The debug socket must have opened for this test to pass, even if
    # the nexe loaded and ran.
    self.assertTrue(self.debug_socket_opened)

  def testChromeDebug(self):
    self.chromeDebug('debug_browser.html', 'PPAPI')


if __name__ == '__main__':
  pyauto_nacl.Main()
