# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import struct
import subprocess
import sys
import unittest

import gdb_rsp


# These are set up by Main().
ARCH = None
SEL_LDR_COMMAND = None


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


ARM_REG_DEFS = [('r%d' % regno, 'I') for regno in xrange(16)]


REG_DEFS = {
    'x86-32': X86_32_REG_DEFS,
    'x86-64': X86_64_REG_DEFS,
    'arm': ARM_REG_DEFS,
    }


SP_REG = {
    'x86-32': 'esp',
    'x86-64': 'rsp',
    'arm': 'r13',
    }


IP_REG = {
    'x86-32': 'eip',
    'x86-64': 'rip',
    'arm': 'r15',
    }


def DecodeRegs(reply):
  defs = REG_DEFS[ARCH]
  names = [reg_name for reg_name, reg_fmt in defs]
  fmt = ''.join([reg_fmt for reg_name, reg_fmt in defs])

  values = struct.unpack_from(fmt, DecodeHex(reply))
  return dict(zip(names, values))


def EncodeRegs(regs):
  defs = REG_DEFS[ARCH]
  names = [reg_name for reg_name, reg_fmt in defs]
  fmt = ''.join([reg_fmt for reg_name, reg_fmt in defs])

  values = [regs[r] for r in names]
  return EncodeHex(struct.pack(fmt, *values))


def PopenDebugStub(test):
  gdb_rsp.EnsurePortIsAvailable()
  # Double '-c' turns off validation. This is needed because the test is using
  # instructions that do not validate, for example, 'int3'.
  return subprocess.Popen(SEL_LDR_COMMAND + ['-c', '-c', '-g', test])


class DebugStubTest(unittest.TestCase):

  def test_initial_breakpoint(self):
    # Any arguments to the nexe would work here because we are only
    # testing that we get a breakpoint at the _start entry point.
    proc = PopenDebugStub('test_getting_registers')
    try:
      connection = gdb_rsp.GdbRspConnection()
      reply = connection.RspRequest('?')
      # TODO(mseaborn): Use a stricter check here when all platforms
      # report the same signal number for the initial breakpoint.
      self.assertTrue(reply.startswith('T'))
    finally:
      proc.kill()
      proc.wait()

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
    elif ARCH == 'arm':
      self.assertEquals(registers['r0'], 0x00000001)
      self.assertEquals(registers['r1'], 0x10000002)
      self.assertEquals(registers['r2'], 0x20000003)
      self.assertEquals(registers['r3'], 0x30000004)
      self.assertEquals(registers['r4'], 0x40000005)
      self.assertEquals(registers['r5'], 0x50000006)
      self.assertEquals(registers['r6'], 0x60000007)
      self.assertEquals(registers['r7'], 0x70000008)
      self.assertEquals(registers['r8'], 0x80000009)
      self.assertEquals(registers['r10'], 0xa000000b)
      self.assertEquals(registers['r11'], 0xb000000c)
      self.assertEquals(registers['r12'], 0xc000000d)
      self.assertEquals(registers['r14'], 0xe000000f)
    else:
      raise AssertionError('Unknown architecture')

  # Test that we can write registers.
  def CheckWriteRegisters(self, connection):
    if ARCH == 'x86-32':
      reg_name = 'edx'
    elif ARCH == 'x86-64':
      reg_name = 'rdx'
    elif ARCH == 'arm':
      reg_name = 'r0'
    else:
      raise AssertionError('Unknown architecture')

    # Read registers.
    regs = DecodeRegs(connection.RspRequest('g'))

    # Change a register.
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
    proc = PopenDebugStub('test_getting_registers')
    try:
      connection = gdb_rsp.GdbRspConnection()
      # Tell the process to continue, because it starts at the
      # breakpoint set at its start address.
      reply = connection.RspRequest('c')
      # We expect a reply that indicates that the process stopped on hlt.
      # TODO(eaeltsin): Windows and Linux signals raised for hlt differ.
      # Investigate/fix that and make a stricter check here.
      self.assertTrue(reply.startswith('T'))

      self.CheckReadRegisters(connection)
      self.CheckWriteRegisters(connection)
      self.CheckReadMemory(connection)
      self.CheckReadMemoryAtInvalidAddr(connection)

    finally:
      proc.kill()
      proc.wait()

  def test_breakpoint(self):
    proc = PopenDebugStub('test_breakpoint')
    try:
      connection = gdb_rsp.GdbRspConnection()

      # Continue until breakpoint.
      reply = connection.RspRequest('c')
      self.assertTrue(reply.startswith('T05thread:'))

      ip = DecodeRegs(connection.RspRequest('g'))[IP_REG[ARCH]]

      if ARCH == 'x86-32' or ARCH == 'x86-64':
        # Check ip points after the int3 instruction.
        reply = connection.RspRequest('m%x,%x' % (ip - 1, 1))
        self.assertEquals(reply, 'cc')
        # Continue until exit.
        reply = connection.RspRequest('c')
        self.assertEquals(reply, 'W00')
      elif ARCH == 'arm':
        # Check pc points to the "bkpt 0x7777" instruction.  We do not
        # skip past these instructions because the sandbox uses them
        # as "halt" instructions, and skipping past them is unsafe.
        breakpoint_instruction = EncodeHex(struct.pack('I', 0xe1277777))
        reply = connection.RspRequest('m%x,%x' % (ip, 4))
        self.assertEquals(reply, breakpoint_instruction)
      else:
        raise AssertionError('Unknown architecture')
    finally:
      proc.kill()
      proc.wait()

  def test_exit_code(self):
    proc = PopenDebugStub('test_exit_code')
    try:
      connection = gdb_rsp.GdbRspConnection()
      reply = connection.RspRequest('c')
      self.assertEquals(reply, 'W02')
    finally:
      proc.kill()
      proc.wait()

  # Single-step and check IP corresponds to debugger_test:test_single_step
  def CheckSingleStep(self, connection, step_command, stop_reply):
    if ARCH == 'x86-32':
      instruction_sizes = [1, 2, 3, 6]
    elif ARCH == 'x86-64':
      instruction_sizes = [1, 3, 4, 6]
    else:
      raise AssertionError('Unknown architecture')

    ip = DecodeRegs(connection.RspRequest('g'))[IP_REG[ARCH]]

    for size in instruction_sizes:
      reply = connection.RspRequest(step_command)
      self.assertEqual(reply, stop_reply)
      ip += size
      actual_ip = DecodeRegs(connection.RspRequest('g'))[IP_REG[ARCH]]
      self.assertEqual(actual_ip, ip)

  def test_single_step(self):
    if ARCH == 'arm':
      # Skip this test because single-stepping is not supported on ARM.
      return
    proc = PopenDebugStub('test_single_step')
    try:
      connection = gdb_rsp.GdbRspConnection()

      # Continue, test we stopped on int3.
      reply = connection.RspRequest('c')
      self.assertTrue(reply.startswith('T05thread:'))
      self.assertTrue(reply.endswith(';'))

      self.CheckSingleStep(connection, 's', reply)
    finally:
      proc.kill()
      proc.wait()

  def test_vCont(self):
    # Basically repeat test_single_step, but using vCont commands.
    if ARCH == 'arm':
      # Skip this test because single-stepping is not supported on ARM.
      return
    proc = PopenDebugStub('test_single_step')
    try:
      connection = gdb_rsp.GdbRspConnection()

      # Test if vCont is supported.
      reply = connection.RspRequest('vCont?')
      self.assertEqual(reply, 'vCont;s;S;c;C')

      # Continue using vCont, test we stopped on int3.
      # Get signalled thread id.
      reply = connection.RspRequest('vCont;c')
      self.assertTrue(reply.startswith('T05thread:'))
      self.assertTrue(reply.endswith(';'))
      tid = int(reply[len('T05thread:'):-1], 16)

      self.CheckSingleStep(connection, 'vCont;s:%x' % tid, reply)

      # Try to single-step the thread and to continue all others.
      reply = connection.RspRequest('vCont;s:%x;c' % tid)
      self.assertTrue(reply.startswith('E'))

      # Try to continue the thread and to single-step all others.
      reply = connection.RspRequest('vCont;c:%x;s' % tid)
      self.assertTrue(reply.startswith('E'))

      # Try to single-step wrong thread.
      reply = connection.RspRequest('vCont;s:%x' % (tid + 2))
      self.assertTrue(reply.startswith('E'))

      # Try to single-step all threads.
      reply = connection.RspRequest('vCont;s')
      self.assertTrue(reply.startswith('E'))
    finally:
      proc.kill()
      proc.wait()


def Main():
  # TODO(mseaborn): Clean up to remove the global variables.  They are
  # currently here because unittest does not help with making
  # parameterised tests.
  index = sys.argv.index('--')
  args = sys.argv[index + 1:]
  # The remaining arguments go to unittest.main().
  sys.argv = sys.argv[:index]
  global ARCH
  global SEL_LDR_COMMAND
  ARCH = args.pop(0)
  SEL_LDR_COMMAND = args
  unittest.main()


if __name__ == '__main__':
  Main()
