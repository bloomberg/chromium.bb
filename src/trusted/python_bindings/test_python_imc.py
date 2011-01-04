# Copyright (c) 2010 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import threading
import unittest

import naclimc


def ConnectAndAccept(boundsock, sockaddr):
  # TODO(mseaborn): Use of a thread here is necessary on Windows,
  # but not on Linux or Mac OS X.  See
  # http://code.google.com/p/nativeclient/issues/detail?id=692
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


def MakeSocketPair():
  boundsock, sockaddr = naclimc.imc_makeboundsock()
  return ConnectAndAccept(boundsock, sockaddr)


class ImcTest(unittest.TestCase):

  def _CheckDataMessages(self, sock1, sock2):
    sock1.imc_sendmsg("foo", ())
    got = sock2.imc_recvmsg(100)
    self.assertEquals(got, ("foo", ()))

  def test_send_recv_data(self):
    sock1, sock2 = MakeSocketPair()
    self._CheckDataMessages(sock1, sock2)
    self._CheckDataMessages(sock2, sock1)

  def test_send_recv_descriptor(self):
    sock1, sock2 = MakeSocketPair()
    boundsock, sockaddr = naclimc.imc_makeboundsock()
    sock1.imc_sendmsg("desc", tuple([sockaddr]))
    got_data, got_socks = sock2.imc_recvmsg(100)
    self.assertEquals(got_data, "desc")
    self.assertEquals(type(got_socks), tuple)
    self.assertEquals(len(got_socks), 1)
    sockaddr2 = got_socks[0]
    new_sock1, new_sock2 = ConnectAndAccept(boundsock, sockaddr2)
    self._CheckDataMessages(new_sock1, new_sock2)
    self._CheckDataMessages(new_sock2, new_sock1)

  def test_send_max_descriptors(self):
    sock1, sock2 = MakeSocketPair()
    boundsock, sockaddr = naclimc.imc_makeboundsock()
    sock1.imc_sendmsg("maxdesc", tuple([sockaddr] * naclimc.DESC_MAX))
    got_data, got_socks = sock2.imc_recvmsg(100)
    self.assertEquals(got_data, "maxdesc")
    self.assertEquals(type(got_socks), tuple)
    self.assertEquals(len(got_socks), naclimc.DESC_MAX)
    for sockaddr2 in got_socks:
      new_sock1, new_sock2 = ConnectAndAccept(boundsock, sockaddr2)
      self._CheckDataMessages(new_sock1, new_sock2)
      self._CheckDataMessages(new_sock2, new_sock1)

  def test_send_error(self):
    # Note that this assumes prompt garbage collection.
    sock = MakeSocketPair()[0]
    self.assertRaises(Exception, lambda: sock.imc_sendmsg("data", ()))

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

  def test_send_nondescriptor_error(self):
    sock = MakeSocketPair()[0]
    self.assertRaises(TypeError,
                      lambda: sock.imc_sendmsg("data", tuple([None])))
    self.assertRaises(TypeError,
                      lambda: sock.imc_sendmsg("data", tuple([None, sock])))
    self.assertRaises(TypeError,
                      lambda: sock.imc_sendmsg("data", tuple([sock, None])))

  def test_send_too_many_descriptors(self):
    sock = MakeSocketPair()[0]
    boundsock, sockaddr = naclimc.imc_makeboundsock()
    descs = tuple([sockaddr] * (naclimc.DESC_MAX + 1))
    self.assertRaises(Exception, lambda: sock.imc_sendmsg("msg data", descs))


if __name__ == "__main__":
  unittest.main()
