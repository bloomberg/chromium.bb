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


def EncodeHex(data):
  return ''.join('%02x' % ord(byte) for byte in data)


X86_32_REG_DEFS = [
    ('eax', 'I'),
    ('ecx', 'I'),
    ('edx', 'I'),
    ('ebx', 'I'),
    ('esp', 'I'),
    ('ebp', 'I'),
    ('esi', 'I'),
    ('edi', 'I'),
    ('eip', 'I'),
    ('eflags', 'I'),
    ('cs', 'I'),
    ('ss', 'I'),
    ('ds', 'I'),
    ('es', 'I'),
    ('fs', 'I'),
    ('gs', 'I'),
]


X86_64_REG_DEFS = [
    ('rax', 'Q'),
    ('rbx', 'Q'),
    ('rcx', 'Q'),
    ('rdx', 'Q'),
    ('rsi', 'Q'),
    ('rdi', 'Q'),
    ('rbp', 'Q'),
    ('rsp', 'Q'),
    ('r8', 'Q'),
    ('r9', 'Q'),
    ('r10', 'Q'),
    ('r11', 'Q'),
    ('r12', 'Q'),
    ('r13', 'Q'),
    ('r14', 'Q'),
    ('r15', 'Q'),
    ('rip', 'Q'),
    ('eflags', 'I'),
    ('cs', 'I'),
    ('ss', 'I'),
    ('ds', 'I'),
    ('es', 'I'),
    ('fs', 'I'),
    ('gs', 'I'),
]


REG_DEFS = {
    'x86-32': X86_32_REG_DEFS,
    'x86-64': X86_64_REG_DEFS,
    }


SP_REG = {
    'x86-32': 'esp',
    'x86-64': 'rsp',
    }


def DecodeRegs(reply):
  defs = REG_DEFS[ARCH]
  names = [reg_name for reg_name,reg_fmt in defs]
  fmt = ''.join([reg_fmt for reg_name,reg_fmt in defs])

  values = struct.unpack_from(fmt, DecodeHex(reply))
  return dict(zip(names, values))


def EncodeRegs(regs):
  defs = REG_DEFS[ARCH]
  names = [reg_name for reg_name,reg_fmt in defs]
  fmt = ''.join([reg_fmt for reg_name,reg_fmt in defs])

  values = [regs[r] for r in names]
  return EncodeHex(struct.pack(fmt, *values))


class DebugStubTest(unittest.TestCase):

  # Test that we can fetch register values.
  # This check corresponds to the last instruction of debugger_test.c
  def CheckReadRegisters(self, connection):
    registers = DecodeRegs(connection.RspRequest('g'))
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

  # Test that we can write registers.
  def CheckWriteRegisters(self, connection):
    if ARCH == 'x86-32':
      reg_name = 'edx'
    elif ARCH == 'x86-64':
      reg_name = 'rdx'
    else:
      raise AssertionError('Unknown architecture')

    # Read registers.
    regs = DecodeRegs(connection.RspRequest('g'))

    # Change a register.
    old_value = regs[reg_name]
    regs[reg_name] += 1
    new_value = regs[reg_name]

    # Write registers. Check for a new value in reply.
    regs = DecodeRegs(connection.RspRequest('G' + EncodeRegs(regs)))
    self.assertEquals(regs[reg_name], new_value)

    # Read registers again. Check for a new value.
    # This is needed in case 'G' packet reply does not correspond to reality :)
    regs = DecodeRegs(connection.RspRequest('g'))
    self.assertEquals(regs[reg_name], new_value)

    # TODO: Resume execution and check that changing the registers really
    # influenced the program's execution. This would require changing
    # debugger_test.c.

  # Test that we can read from memory by reading from the stack.
  # This check corresponds to the last instruction of debugger_test.c
  def CheckReadMemory(self, connection):
    registers = DecodeRegs(connection.RspRequest('g'))
    stack_addr = registers[SP_REG[ARCH]]
    stack_val = struct.unpack(
        'I', DecodeHex(connection.RspRequest('m%x,%x' % (stack_addr, 4))))[0]
    self.assertEquals(stack_val, 0x4bb00ccc)

  # Test that reading from an unreadable address gives a sensible error.
  def CheckReadMemoryAtInvalidAddr(self, connection):
    mem_addr = 0
    result = connection.RspRequest('m%x,%x' % (mem_addr, 8))
    self.assertEquals(result, 'E03')

  # Run tests on debugger_test.c binary.
  def test_debugger_test(self):
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

      self.CheckReadRegisters(connection)
      self.CheckWriteRegisters(connection)
      self.CheckReadMemory(connection)
      self.CheckReadMemoryAtInvalidAddr(connection)

    finally:
      proc.kill()
      proc.wait()


if __name__ == '__main__':
  # TODO(mseaborn): Remove the global variable.  It is currently here
  # because unittest does not help with making parameterised tests.
  ARCH = sys.argv.pop(1)
  unittest.main()
