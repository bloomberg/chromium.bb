# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import struct
import subprocess
import sys
import unittest

import gdb_rsp


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


class DebugStubTest(unittest.TestCase):

  def test_getting_registers(self):
    gdb_rsp.EnsurePortIsAvailable()
    proc = subprocess.Popen([os.environ['NACL_SEL_LDR'], '-g',
                             os.environ['DEBUGGER_TEST_PROG']])
    try:
      connection = gdb_rsp.GdbRspConnection()
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
      stack_val = struct.unpack(
          'I', DecodeHex(connection.RspRequest('m%x,%x' % (stack_addr, 4))))[0]
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
