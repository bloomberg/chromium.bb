# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import struct
import subprocess
import sys
import unittest

import gdb_rsp


NACL_SIGTRAP = 5
NACL_SIGSEGV = 11


# These are set up by Main().
ARCH = None
NM_TOOL = None
SEL_LDR_COMMAND = None


def AssertEquals(x, y):
  if x != y:
    raise AssertionError('%r != %r' % (x, y))


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


def KillProcess(process):
  try:
    process.kill()
  except OSError:
    if sys.platform == 'win32':
      # If process is already terminated, kill() throws
      # "WindowsError: [Error 5] Access is denied" on Windows.
      pass
    else:
      raise
  process.wait()


class LaunchDebugStub(object):

  def __init__(self, test):
    self._proc = PopenDebugStub(test)

  def __enter__(self):
    try:
      return gdb_rsp.GdbRspConnection()
    except:
      KillProcess(self._proc)
      raise

  def __exit__(self, exc_type, exc_value, traceback):
    KillProcess(self._proc)


def GetSymbols():
  assert '-f' in SEL_LDR_COMMAND
  nexe_filename = SEL_LDR_COMMAND[SEL_LDR_COMMAND.index('-f') + 1]
  symbols = {}
  proc = subprocess.Popen([NM_TOOL, '--format=posix', nexe_filename],
                          stdout=subprocess.PIPE)
  for line in proc.stdout:
    match = re.match('(\S+) [TtWwB] ([0-9a-fA-F]+)', line)
    if match is not None:
      name = match.group(1)
      addr = int(match.group(2), 16)
      symbols[name] = addr
  result = proc.wait()
  assert result == 0, result
  return symbols


def ParseThreadStopReply(reply):
  match = re.match('T([0-9a-f]{2})thread:([0-9a-f]+);$', reply)
  if match is None:
    raise AssertionError('Bad thread stop reply: %r' % reply)
  return {'signal': int(match.group(1), 16),
          'thread_id': int(match.group(2), 16)}


def AssertReplySignal(reply, signal):
  AssertEquals(ParseThreadStopReply(reply)['signal'], signal)


def ReadMemory(connection, address, size):
  reply = connection.RspRequest('m%x,%x' % (address, size))
  assert not reply.startswith('E'), reply
  return DecodeHex(reply)


def ReadUint32(connection, address):
  return struct.unpack('I', ReadMemory(connection, address, 4))[0]


class DebugStubTest(unittest.TestCase):

  def test_initial_breakpoint(self):
    # Any arguments to the nexe would work here because we are only
    # testing that we get a breakpoint at the _start entry point.
    with LaunchDebugStub('test_getting_registers') as connection:
      reply = connection.RspRequest('?')
      AssertReplySignal(reply, NACL_SIGTRAP)

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

    # Write registers.
    self.assertEquals(connection.RspRequest('G' + EncodeRegs(regs)), 'OK')

    # Read registers. Check for a new value.
    regs = DecodeRegs(connection.RspRequest('g'))
    self.assertEquals(regs[reg_name], new_value)

    # TODO: Resume execution and check that changing the registers really
    # influenced the program's execution. This would require changing
    # debugger_test.c.

  def CheckReadOnlyRegisters(self, connection):
    if ARCH == 'x86-32':
      sample_read_only_regs = ['cs', 'ds']
    elif ARCH == 'x86-64':
      sample_read_only_regs = ['r15', 'cs', 'ds']
    elif ARCH == 'arm':
      sample_read_only_regs = []
    else:
      raise AssertionError('Unknown architecture')

    for reg_name in sample_read_only_regs:
      # Read registers.
      regs = DecodeRegs(connection.RspRequest('g'))

      # Change a register.
      old_value = regs[reg_name]
      regs[reg_name] += 1

      # Write registers.
      self.assertEquals(connection.RspRequest('G' + EncodeRegs(regs)), 'OK')

      # Read registers. Check for an old value.
      regs = DecodeRegs(connection.RspRequest('g'))
      self.assertEquals(regs[reg_name], old_value)

  # Test that we can read from memory by reading from the stack.
  # This check corresponds to the last instruction of debugger_test.c
  def CheckReadMemory(self, connection):
    registers = DecodeRegs(connection.RspRequest('g'))
    stack_addr = registers[SP_REG[ARCH]]
    stack_val = ReadUint32(connection, stack_addr)
    self.assertEquals(stack_val, 0x4bb00ccc)

    # On x86-64, for reading/writing memory, the debug stub accepts
    # untrusted addresses with or without the %r15 sandbox base
    # address added, because GDB uses both.
    # TODO(eaeltsin): Fix GDB to not use addresses with %r15 added,
    # and only test this memory access with stack_addr masked as
    # below.
    if ARCH == 'x86-64':
      stack_addr &= 0xffffffff
      stack_val = ReadUint32(connection, stack_addr)
      self.assertEquals(stack_val, 0x4bb00ccc)

  # Test that reading from an unreadable address gives a sensible error.
  def CheckReadMemoryAtInvalidAddr(self, connection):
    mem_addr = 0
    result = connection.RspRequest('m%x,%x' % (mem_addr, 8))
    self.assertEquals(result, 'E03')

  # Run tests on debugger_test.c binary.
  def test_debugger_test(self):
    with LaunchDebugStub('test_getting_registers') as connection:
      # Tell the process to continue, because it starts at the
      # breakpoint set at its start address.
      reply = connection.RspRequest('c')
      if ARCH == 'arm':
        # The process should have stopped on a BKPT instruction.
        AssertReplySignal(reply, NACL_SIGTRAP)
      else:
        # The process should have stopped on a HLT instruction.
        AssertReplySignal(reply, NACL_SIGSEGV)

      self.CheckReadRegisters(connection)
      self.CheckWriteRegisters(connection)
      self.CheckReadOnlyRegisters(connection)
      self.CheckReadMemory(connection)
      self.CheckReadMemoryAtInvalidAddr(connection)

  def test_breakpoint(self):
    with LaunchDebugStub('test_breakpoint') as connection:
      # Continue until breakpoint.
      reply = connection.RspRequest('c')
      AssertReplySignal(reply, NACL_SIGTRAP)

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

  def test_exit_code(self):
    with LaunchDebugStub('test_exit_code') as connection:
      reply = connection.RspRequest('c')
      self.assertEquals(reply, 'W02')

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
      # TODO(eaeltsin):
      #   http://code.google.com/p/nativeclient/issues/detail?id=2911
      return
    with LaunchDebugStub('test_single_step') as connection:
      # Continue, test we stopped on int3.
      reply = connection.RspRequest('c')
      AssertReplySignal(reply, NACL_SIGTRAP)

      self.CheckSingleStep(connection, 's', reply)

  def test_vCont(self):
    # Basically repeat test_single_step, but using vCont commands.
    if ARCH == 'arm':
      # Skip this test because single-stepping is not supported on ARM.
      # TODO(eaeltsin):
      #   http://code.google.com/p/nativeclient/issues/detail?id=2911
      return
    with LaunchDebugStub('test_single_step') as connection:
      # Test if vCont is supported.
      reply = connection.RspRequest('vCont?')
      self.assertEqual(reply, 'vCont;s;S;c;C')

      # Continue using vCont, test we stopped on int3.
      # Get signalled thread id.
      reply = connection.RspRequest('vCont;c')
      AssertReplySignal(reply, NACL_SIGTRAP)
      tid = ParseThreadStopReply(reply)['thread_id']

      self.CheckSingleStep(connection, 'vCont;s:%x' % tid, reply)

      # Single step one thread and continue all others.
      reply = connection.RspRequest('vCont;s:%x;c' % tid)
      # WARNING! This check is valid in single-threaded case only!
      # In multi-threaded case another thread might stop first.
      self.assertEqual(reply, 'T05thread:%x;' % tid)

      # Try to continue the thread and to single-step all others.
      reply = connection.RspRequest('vCont;c:%x;s' % tid)
      self.assertTrue(reply.startswith('E'))

      # Try to single-step wrong thread.
      reply = connection.RspRequest('vCont;s:%x' % (tid + 2))
      self.assertTrue(reply.startswith('E'))

      # Try to single-step all threads.
      reply = connection.RspRequest('vCont;s')
      self.assertTrue(reply.startswith('E'))

  def test_interrupt(self):
    if ARCH == 'arm':
      # Skip this test because single-stepping is not supported on ARM.
      # TODO(eaeltsin):
      #   http://code.google.com/p/nativeclient/issues/detail?id=2911
      return
    with LaunchDebugStub('test_interrupt') as connection:
      # Continue (program will spin forever), then interrupt.
      connection.RspSendOnly('c')
      reply = connection.RspInterrupt()
      self.assertEqual(reply, 'T00')

      # Single-step.
      reply = connection.RspRequest('s')
      AssertReplySignal(reply, NACL_SIGTRAP)

  def test_software_single_step(self):
    # We want this test to work on ARM. As we can't skip past trap instruction
    # on ARM, we'll step from initial breakpoint to the first trap instruction.
    # We can use any test that has initial breakpoint and trap instruction on
    # the same thread.
    with LaunchDebugStub('test_breakpoint') as connection:
      # We stopped on initial breakpoint. Ask for stop reply.
      reply = connection.RspRequest('?')
      AssertReplySignal(reply, NACL_SIGTRAP)
      tid = ParseThreadStopReply(reply)['thread_id']

      # Continue one thread.
      # Check we stopped on the same thread.
      reply = connection.RspRequest('vCont;c:%x' % tid)
      self.assertEqual(reply, 'T05thread:%x;' % tid)


class DebugStubThreadSuspensionTest(unittest.TestCase):

  def SkipBreakpoint(self, connection):
    # On x86, int3 breakpoint instructions are skipped automatically,
    # so we only need to intervene to skip the breakpoint on ARM.
    if ARCH == 'arm':
      bundle_size = 16
      regs = DecodeRegs(connection.RspRequest('g'))
      assert regs['r15'] % bundle_size == 0, regs['r15']
      regs['r15'] += bundle_size
      AssertEquals(connection.RspRequest('G' + EncodeRegs(regs)), 'OK')

  def WaitForTestThreadsToStart(self, connection, symbols):
    # Wait until:
    #  * The main thread starts to modify g_main_thread_var.
    #  * The child thread executes a breakpoint.
    old_value = ReadUint32(connection, symbols['g_main_thread_var'])
    while True:
      reply = connection.RspRequest('c')
      AssertReplySignal(reply, NACL_SIGTRAP)
      self.SkipBreakpoint(connection)
      child_thread_id = ParseThreadStopReply(reply)['thread_id']
      if ReadUint32(connection, symbols['g_main_thread_var']) != old_value:
        break
    return child_thread_id

  def test_continuing_thread_with_others_suspended(self):
    with LaunchDebugStub('test_suspending_threads') as connection:
      symbols = GetSymbols()
      child_thread_id = self.WaitForTestThreadsToStart(connection, symbols)

      # Test continuing a single thread while other threads remain
      # suspended.
      for _ in range(3):
        main_thread_val = ReadUint32(connection, symbols['g_main_thread_var'])
        child_thread_val = ReadUint32(connection, symbols['g_child_thread_var'])
        reply = connection.RspRequest('vCont;c:%x' % child_thread_id)
        AssertReplySignal(reply, NACL_SIGTRAP)
        self.SkipBreakpoint(connection)
        self.assertEquals(ParseThreadStopReply(reply)['thread_id'],
                          child_thread_id)
        # The main thread should not be allowed to run, so should not
        # modify g_main_thread_var.
        self.assertEquals(
            ReadUint32(connection, symbols['g_main_thread_var']),
            main_thread_val)
        # The child thread should always modify g_child_thread_var
        # between each breakpoint.
        self.assertNotEquals(
            ReadUint32(connection, symbols['g_child_thread_var']),
            child_thread_val)

  def test_single_stepping_thread_with_others_suspended(self):
    with LaunchDebugStub('test_suspending_threads') as connection:
      symbols = GetSymbols()
      child_thread_id = self.WaitForTestThreadsToStart(connection, symbols)

      # Test single-stepping a single thread while other threads
      # remain suspended.
      for _ in range(3):
        main_thread_val = ReadUint32(connection, symbols['g_main_thread_var'])
        child_thread_val = ReadUint32(connection, symbols['g_child_thread_var'])
        while True:
          reply = connection.RspRequest('vCont;s:%x' % child_thread_id)
          AssertReplySignal(reply, NACL_SIGTRAP)
          self.SkipBreakpoint(connection)
          self.assertEquals(ParseThreadStopReply(reply)['thread_id'],
                            child_thread_id)
          # The main thread should not be allowed to run, so should not
          # modify g_main_thread_var.
          self.assertEquals(
              ReadUint32(connection, symbols['g_main_thread_var']),
              main_thread_val)
          # Eventually, the child thread should modify g_child_thread_var.
          if (ReadUint32(connection, symbols['g_child_thread_var'])
              != child_thread_val):
            break


def Main():
  # TODO(mseaborn): Clean up to remove the global variables.  They are
  # currently here because unittest does not help with making
  # parameterised tests.
  index = sys.argv.index('--')
  args = sys.argv[index + 1:]
  # The remaining arguments go to unittest.main().
  sys.argv = sys.argv[:index]
  global ARCH
  global NM_TOOL
  global SEL_LDR_COMMAND
  ARCH = args.pop(0)
  NM_TOOL = args.pop(0)
  SEL_LDR_COMMAND = args
  unittest.main()


if __name__ == '__main__':
  Main()
