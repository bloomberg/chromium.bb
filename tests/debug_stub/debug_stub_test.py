# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import signal
import socket
import struct
import subprocess
import sys
import time
import unittest


SEL_LDR_RSP_TCP_PORT = 4014


def RspChecksum(data):
  checksum = 0
  for char in data:
    checksum = (checksum + ord(char)) % 0x100
  return checksum


def DecodeHex(data):
  assert len(data) % 2 == 0
  return ''.join([chr(int(data[index * 2 : (index + 1) * 2], 16))
                  for index in xrange(len(data) / 2)])


X86_32_REGISTERS = ('eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi')

X86_64_REGISTERS = ('rax', 'rbx', 'rcx', 'rdx', 'rsi', 'rdi', 'rbp', 'rsp',
                    'r8', 'r9', 'r10', 'r11', 'r12', 'r13', 'r14', 'r15')

REGISTERS = {
    'x86-32': X86_32_REGISTERS,
    'x86-64': X86_64_REGISTERS,
    }

STACK_PTR_REG = {
    'x86-32': 'esp',
    'x86-64': 'rsp',
    }


def UnpackRegisters(data):
  # This does not cover flags and segment registers yet.
  reg_names = REGISTERS[ARCH]
  if ARCH == 'x86-64':
    fmt = 'Q'
  else:
    fmt = 'I'
  regs = struct.unpack_from(fmt * len(reg_names), data, 0)
  return dict(zip(reg_names, regs))


def EnsurePortIsAvailable():
  # As a sanity check, check that the TCP port is available by binding
  # to it ourselves (and then unbinding).  Otherwise, we could end up
  # talking to an old instance of sel_ldr that is still hanging
  # around, or to some unrelated service that uses the same port
  # number.  Of course, there is still a race condition because an
  # unrelated process could bind the port after we unbind.
  sock = socket.socket()
  sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, True)
  sock.bind(('', SEL_LDR_RSP_TCP_PORT))
  sock.close()


class GdbRspConnection(object):

  def __init__(self):
    self._socket = socket.socket()
    self._Connect()

  def _Connect(self):
    # We have to poll because we do not know when sel_ldr has
    # successfully done bind() on the TCP port.  This is inherently
    # unreliable.
    # TODO(mseaborn): Add a more reliable connection mechanism to
    # sel_ldr's debug stub.
    timeout_in_seconds = 10
    poll_time_in_seconds = 0.1
    for i in xrange(int(timeout_in_seconds / poll_time_in_seconds)):
      try:
        self._socket.connect(('localhost', SEL_LDR_RSP_TCP_PORT))
      except socket.error:
        # Retry after a delay.
        time.sleep(poll_time_in_seconds)
      else:
        return
    raise Exception('Could not connect to sel_ldr\'s debug stub in %i seconds'
                    % timeout_in_seconds)

  # Send an rsp message, but don't wait for or expect a reply.
  def RspSendOnly(self, data):
    msg = '$%s#%02x' % (data, RspChecksum(data))
    return self._socket.send(msg)

  def RspRequest(self, data):
    msg = '$%s#%02x' % (data, RspChecksum(data))
    self._socket.send(msg)
    reply = ''
    while True:
      reply += self._socket.recv(1024)
      if '#' in reply:
        break
    match = re.match('\+\$([^#]*)#([0-9a-fA-F]{2})$', reply)
    if match is None:
      raise AssertionError('Unexpected reply message: %r' % reply)
    reply_body = match.group(1)
    checksum = match.group(2)
    expected_checksum = '%02x' % RspChecksum(reply_body)
    if checksum != expected_checksum:
      raise AssertionError('Bad RSP checksum: %r != %r' %
                           (checksum, expected_checksum))
    # Send acknowledgement.
    self._socket.send('+')
    return reply_body

  def ReadMemory(self, address, size):
    return DecodeHex(self.RspRequest('m%x,%x' % (address, size)))


class DebugStubTest(unittest.TestCase):

  def test_getting_registers(self):
    EnsurePortIsAvailable()
    proc = subprocess.Popen([os.environ['NACL_SEL_LDR'], '-g',
                             os.environ['DEBUGGER_TEST_PROG']])
    try:
      connection = GdbRspConnection()
      # Tell the process to continue, because it starts at the
      # breakpoint set at its start address.
      reply = connection.RspRequest('c')
      # We expect a reply that indicates that the process stopped again.
      assert reply.startswith('S'), reply

      # Test that we can fetch register values.
      registers = UnpackRegisters(DecodeHex(connection.RspRequest('g')))
      if ARCH == 'x86-32':
        self.assertEquals(registers['eax'], 0x11000022)
        self.assertEquals(registers['ebx'], 0x22000033)
        self.assertEquals(registers['ecx'], 0x33000044)
        self.assertEquals(registers['edx'], 0x44000055)
        self.assertEquals(registers['esi'], 0x55000066)
        self.assertEquals(registers['edi'], 0x66000077)
        self.assertEquals(registers['ebp'], 0x77000088)
      elif ARCH == 'x86-64':
        self.assertEquals(registers['rax'], 0x1100000000000022)
        self.assertEquals(registers['rbx'], 0x2200000000000033)
        self.assertEquals(registers['rcx'], 0x3300000000000044)
        self.assertEquals(registers['rdx'], 0x4400000000000055)
        self.assertEquals(registers['rsi'], 0x5500000000000066)
        self.assertEquals(registers['rdi'], 0x6600000000000077)
        self.assertEquals(registers['r8'],  0x7700000000000088)
        self.assertEquals(registers['r9'],  0x8800000000000099)
        self.assertEquals(registers['r10'], 0x99000000000000aa)
        self.assertEquals(registers['r11'], 0xaa000000000000bb)
        self.assertEquals(registers['r12'], 0xbb000000000000cc)
        self.assertEquals(registers['r13'], 0xcc000000000000dd)
        self.assertEquals(registers['r14'], 0xdd000000000000ee)
      else:
        raise AssertionError('Unknown architecture')

      # Test that we can read from memory by reading from the stack.
      stack_addr = registers[STACK_PTR_REG[ARCH]]
      stack_val = struct.unpack('I', connection.ReadMemory(stack_addr, 4))[0]
      self.assertEquals(stack_val, 0x4bb00ccc)

      # Test that reading from an unreadable address gives a sensible error.
      mem_addr = 0
      result = connection.RspRequest('m%x,%x' % (mem_addr, 8))
      self.assertEquals(result, 'E03')
    finally:
      proc.kill()
      proc.wait()


if __name__ == '__main__':
  # TODO(mseaborn): Remove the global variable.  It is currently here
  # because unittest does not help with making parameterised tests.
  ARCH = sys.argv.pop(1)
  unittest.main()
