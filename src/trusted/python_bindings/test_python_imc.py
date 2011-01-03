# Copyright (c) 2010 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import threading
import unittest

import naclimc


def MakeSocketPair():
  # TODO(mseaborn): Use of a thread here is necessary on Windows,
  # but not on Linux or Mac OS X.  See
  # http://code.google.com/p/nativeclient/issues/detail?id=692
  boundsock, sockaddr = naclimc.imc_makeboundsock()
  result = [None]
  def RunThread():
    result[0] = boundsock.imc_accept()
  thread = threading.Thread(target=RunThread)
  thread.start()
  sock1 = sockaddr.imc_connect()
  thread.join()
  if result[0] is None:
    raise Exception("imc_connect() failed")
  sock2 = result[0]
  return sock1, sock2


class ImcTest(unittest.TestCase):

  def _CheckDataMessages(self, sock1, sock2):
    sock1.imc_sendmsg("foo")
    got = sock2.imc_recvmsg(100)
    self.assertEquals(got, "foo")

  def test_send_recv_data(self):
    sock1, sock2 = MakeSocketPair()
    self._CheckDataMessages(sock1, sock2)
    self._CheckDataMessages(sock2, sock1)

  def test_send_error(self):
    # Note that this assumes prompt garbage collection.
    sock = MakeSocketPair()[0]
    self.assertRaises(Exception, lambda: sock.imc_sendmsg("data"))

  def test_recv_error(self):
    # Note that this assumes prompt garbage collection.
    sock = MakeSocketPair()[0]
    self.assertRaises(Exception, lambda: sock.imc_recvmsg(100))

  def test_accept_error(self):
    # imc_accept() raises an error here because it is not
    # implemented for this descriptor type.
    sock = MakeSocketPair()[0]
    self.assertRaises(Exception, lambda: sock.imc_accept())

  def test_connect_error(self):
    # imc_connect() raises an error here because it is not
    # implemented for this descriptor type.
    sock = MakeSocketPair()[0]
    self.assertRaises(Exception, lambda: sock.imc_connect())


if __name__ == "__main__":
  unittest.main()
