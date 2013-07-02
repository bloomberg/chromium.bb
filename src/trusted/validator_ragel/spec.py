#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Executable specification of valid instructions and superinstructions (in terms
# of their disassembler listing).
# Should serve as formal and up-to-date ABI reference and as baseline for
# validator exhaustive tests.

# It is generally organized as a set of functions responsible for recognizing
# and validating specific patterns (jump instructions, regular instructions,
# superinstructions, etc.)
# There are three outcomes for running such function:
#   - function raises DoNotMatchError (which means instruction is of completely
#     different structure, for example when we call ValidateSuperinstruction on
#     nop)
#   - function raises SandboxingError (which means instruction generally matches
#     respective pattern, but some rules are violated)
#   - function returns (which means instruction(s) is(are) safe)
#
# Why exceptions instead of returning False or something? Because they carry
# stack traces, which makes it easier to investigate why particular instruction
# was rejected.
# Why distinguish DoNotMatchError and SandboxingError? Because on the topmost
# level we attempt to call all matchers and we need to see which error message
# was most relevant.

import re


class DoNotMatchError(Exception):
  pass


class SandboxingError(Exception):
  pass


BUNDLE_SIZE = 32


def _ValidateNop(instruction):
  # TODO(shcherbina): whitelist nops by bytes, not only by disasm.
  # Example:
  #   66 66 0f 1f 84 00 00 00 00 00
  # is decoded as
  #   nopw   0x0(%rax,%rax,1)
  # and so seems valid, but we should only allow
  #   66 0f 1f 84 00 00 00 00 00
  if instruction.disasm in [
      'nop',
      'nopl (%rax)',
      'nopl 0x0(%rax)',
      'nopl 0x0(%rax,%rax,1)',
      ]:
    return

  if re.match(
      r'(data32 )*nopw (%cs:)?0x0\(%[er]ax,%[er]ax,1\)$',
      instruction.disasm):
    return
  raise DoNotMatchError(instruction)


def _ValidateOperandlessInstruction(instruction, bitness):
  assert bitness in [32, 64]

  if instruction.disasm in [
      'cpuid',
      'hlt',
      'lahf',
      'sahf',
      ]:
    return

  if bitness == 32 and instruction.disasm in [
      'leave',
      ]:
    return

  raise DoNotMatchError(instruction)


def _ValidateStringInstruction(instruction):
  prefix_re = r'(rep |repz |repnz )?'
  lods_re = r'lods %ds:\(%esi\),(%al|%ax|%eax)'
  stos_re = r'stos (%al|%ax|%eax),%es:\(%edi\)'
  scas_re = r'scas %es:\(%edi\),(%al|%ax|%eax)'
  movs_re = r'movs[bwl] %ds:\(%esi\),%es:\(%edi\)'
  cmps_re = r'cmps[bwl] %es:\(%edi\),%ds:\(%esi\)'

  string_insn_re = '%s(%s)$' % (
      prefix_re,
      '|'.join([lods_re, stos_re, scas_re, movs_re, cmps_re]))

  if re.match(string_insn_re, instruction.disasm):
    return

  raise DoNotMatchError(instruction)


def _ValidateTlsInstruction(instruction):
  if re.match(r'mov %gs:(0x0|0x4),%e[a-z][a-z]$', instruction.disasm):
    return

  raise DoNotMatchError(instruction)


def _AnyRegisterRE(group_name='register'):
  # TODO(shcherbina): explicitly list all kinds of registers we care to
  # distinguish for validation purposes.
  return r'(?P<%s>%%(st\(\d+\)|\w+))' % group_name


def _HexRE(group_name='value'):
  return r'(?P<%s>-?0x[\da-f]+)' % group_name


def _ImmediateRE(group_name='immediate'):
  return r'(?P<%s>\$%s)' % (
      group_name,
      _HexRE(group_name=group_name + '_value'))


def _MemoryRE(group_name='memory'):
  # Possible forms:
  #   (%eax)
  #   (%eax,%ebx,1)
  #   (,%ebx,1)
  #   0x42(...)
  # and even
  #   0x42
  return r'(?P<%s>(?P<%s_segment>%%[cdefgs]s:)?%s?(\(%s?(,%s,\d)?\))?)' % (
      group_name,
      group_name,
      _HexRE(group_name=group_name + '_offset'),
      _AnyRegisterRE(group_name=group_name + '_base'),
      _AnyRegisterRE(group_name=group_name + '_index'))


def _IndirectJumpTargetRE(group_name='target'):
  return r'(?P<%s>\*(%s|%s))' % (
      group_name,
      _AnyRegisterRE(group_name=group_name + '_register'),
      _MemoryRE(group_name=group_name + '_memory'))


def _OperandRE(group_name='operand'):
  return r'(?P<%s>%s|%s|%s|%s)' % (
      group_name,
      _AnyRegisterRE(group_name=group_name + '_register'),
      _ImmediateRE(group_name=group_name + '_immediate'),
      _MemoryRE(group_name=group_name + '_memory'),
      _IndirectJumpTargetRE(group_name=group_name + '_target'))


def _SplitOps(insn, args):
  # We can't use just args.split(',') because operands can contain commas
  # themselves, for example '(%r15,%rax,1)'.
  ops = []
  i = 0
  while True:
    # We do not use mere re.compile(_OperandRE(), args, i) here because
    # python backtracking regexes do not guarantee to find longest match.
    m = re.compile(r'(%s)($|,)' % _OperandRE()).match(args, i)
    assert m is not None, (args, i)
    ops.append(m.group(1))
    i = m.end(1)
    if i == len(args):
      break
    assert args[i] == ',', (insn, args, i)
    i += 1
  return ops


def _ParseInstruction(instruction):
  # Strip comment.
  disasm, _, _ = instruction.disasm.partition('#')
  elems = disasm.split()
  prefixes = []
  while elems[0] in ['lock', 'rep', 'repz', 'repnz']:
    prefixes.append(elems.pop(0))

  # There could be branching expectation information in instruction names:
  #    jo,pt      <addr>
  #    jge,pn     <addr>
  name_re = r'[a-z]\w*(,p[nt])?$'
  name = elems[0]
  assert re.match(name_re, name), name

  if len(elems) == 1:
    ops = []
  elif len(elems) == 2:
    ops = _SplitOps(instruction, elems[1])
  else:
    assert False, instruction

  return prefixes, name, ops


REG32_TO_REG64 = {
    '%eax' : '%rax',
    '%ebx' : '%rbx',
    '%ecx' : '%rcx',
    '%edx' : '%rdx',
    '%esi' : '%rsi',
    '%edi' : '%rdi',
    '%esp' : '%rsp',
    '%ebp' : '%rbp',
    '%r8d' : '%r8',
    '%r9d' : '%r9',
    '%r10d' : '%r10',
    '%r11d' : '%r11',
    '%r12d' : '%r12',
    '%r13d' : '%r13',
    '%r14d' : '%r14',
    '%r15d' : '%r15'}

REGS64 = REG32_TO_REG64.values()


class Condition(object):
  """Represents assertion about the state of 64-bit registers.

  (used as precondition and postcondition)

  Supported assertions:
    0. %rpb and %rsp are sandboxed (and nothing is known about other registers)
    1. {%rax} is restricted, %rbp and %rsp are sandboxed
    2-13. same for %rbx-%r14 not including %rbp and %rsp
    14. %rbp is restricted, %rsp is sandboxed
    15. %rsp is restricted, %rpb is sandboxed

  It can be observed that all assertions 1..15 differ from default 0 in a single
  register, which prompts internal representation of a single field,
  _restricted_register, which stores name of this standing out register
  (or None).

  * 'restricted' means higher 32 bits are zeroes
  * 'sandboxed' means within [%r15, %r15 + 2**32) range
  It goes without saying that %r15 is never changed and by definition sandboxed.
  """

  def __init__(self, restricted=None, restricted_instead_of_sandboxed=None):
    self._restricted_register = None
    if restricted is not None:
      assert restricted_instead_of_sandboxed is None
      assert restricted in REGS64
      assert restricted not in ['%r15', '%rbp', '%rsp']
      self._restricted_register = restricted
    if restricted_instead_of_sandboxed is not None:
      assert restricted is None
      assert restricted_instead_of_sandboxed in ['%rbp', '%rsp']
      self._restricted_register = restricted_instead_of_sandboxed

  def GetAlteredRegisters(self):
    """ Return pair (restricted, restricted_instead_of_sandboxed).

    Each item is either register name or None.
    """
    if self._restricted_register is None:
      return None, None
    elif self._restricted_register in ['%rsp', '%rbp']:
      return None, self._restricted_register
    else:
      return self._restricted_register, None

  def __eq__(self, other):
    return self._restricted_register == other._restricted_register

  def __ne__(self, other):
    return not self == other

  def Implies(self, other):
    return self.WhyNotImplies(other) is None

  def WhyNotImplies(self, other):
    if other._restricted_register is None:
      if self._restricted_register in ['%rbp', '%rsp']:
        return '%s should not be restricted' % self._restricted_register
      else:
        return None
    else:
      if self._restricted_register != other._restricted_register:
        return (
            'register %s should be restricted, '
            'while in fact %r is restricted' % (
                other._restricted_register, self._restricted_register))
      else:
        return None

  def __repr__(self):
    if self._restricted_register is None:
      return 'Condition(default)'
    elif self._restricted_register in ['%rbp', '%rsp']:
      return ('Condition(%s restricted instead of sandboxed)'
              % self._restricted_register)
    else:
      return 'Condition(%s restricted)' % self._restricted_register

  @staticmethod
  def All():
    yield Condition()
    for reg in REGS64:
      if reg not in ['%r15', '%rbp', '%rsp']:
        yield Condition(restricted=reg)
    yield Condition(restricted_instead_of_sandboxed='%rbp')
    yield Condition(restricted_instead_of_sandboxed='%rsp')


def _GetLegacyPrefixes(instruction):
  result = []
  for b in instruction.bytes:
    if b not in [
        0x66, 0x67, 0x2e, 0x3e, 0x26, 0x64, 0x65, 0x36, 0xf0, 0xf3, 0xf2]:
      break
    if b in result:
      raise SandboxingError('duplicate legacy prefix', instruction)
    result.append(b)
  return result


def _ProcessMemoryAccess(instruction, operands):
  """Make sure that memory access is valid and return precondition required.

  (only makes sense for 64-bit instructions)

  Args:
    instruction: Instruction tuple
    operands: list of instruction operands as strings, for example
              ['%eax', '(%r15,%rbx,1)']
  Returns:
    Condition object representing precondition required for memory access (if
    it's present among operands) to be valid.
  Raises:
    SandboxingError if memory access is invalid.
  """
  precondition = Condition()
  for op in operands:
    m = re.match(_MemoryRE() + r'$', op)
    if m is not None:
      assert m.group('memory_segment') is None
      base = m.group('memory_base')
      index = m.group('memory_index')
      allowed_bases = ['%r15', '%rbp', '%rsp', '%rip']
      if base not in allowed_bases:
        raise SandboxingError(
            'memory access only is allowed with base from %s'
            % allowed_bases,
            instruction)
      if index is not None:
        if index == '%riz':
          pass
        elif index in REGS64:
          if index in ['%r15', '%rsp', '%rbp']:
            raise SandboxingError(
                '%s can\'t be used as index in memory access' % index,
                instruction)
          else:
            assert precondition == Condition()
            precondition = Condition(restricted=index)
        else:
          raise SandboxingError(
              'unrecognized register is used for memory access as index',
              instruction)
  return precondition


def _ProcessOperandWrites(instruction, write_operands, zero_extending=False):
  """Check that writes to operands are valid, return postcondition established.

  (only makes sense for 64-bit instructions)

  Args:
    instruction: Instruction tuple
    write_operands: list of operands instruction writes to as strings,
                    for example ['%eax', '(%r15,%rbx,1)']
    zero_extending: whether instruction is considered zero extending
  Returns:
    Condition object representing postcondition established by operand writes.
  Raises:
    SandboxingError if write is invalid.
  """
  postcondition = Condition()
  for op in write_operands:
    # TODO(shcherbina): disallow writes to
    #   cs, ds, es, fs, gs
    if op in ['%r15', '%r15d', '%r15w', '%r15b']:
      raise SandboxingError('changes to r15 are not allowed', instruction)
    if op in ['%bpl', '%bp', '%rbp']:
      raise SandboxingError('changes to rbp are not allowed', instruction)
    if op in ['%spl', '%sp', '%rsp']:
      raise SandboxingError('changes to rsp are not allowed', instruction)

    if op in REG32_TO_REG64 and zero_extending:
      if not postcondition.Implies(Condition()):
        raise SandboxingError(
            '%s when zero-extending %s'
            % (postcondition.WhyNotImplies(Condition()), op),
            instruction)

      r = REG32_TO_REG64[op]
      if r in ['%rbp', '%rsp']:
        postcondition = Condition(restricted_instead_of_sandboxed=r)
      else:
        postcondition = Condition(restricted=r)
  return postcondition


def _InstructionNameIn(name, candidates):
  return re.match('(%s)[bwlq]?$' % '|'.join(candidates), name) is not None


def ValidateRegularInstruction(instruction, bitness):
  """Validate regular instruction (not direct jump).

  Args:
    instruction: objdump_parser.Instruction tuple
    bitness: 32 or 64
  Returns:
    Pair (precondition, postcondition) of Condition instances.
    (for 32-bit case they are meaningless and are not used)
  Raises:
    According to usual convention.
  """
  assert bitness in [32, 64]

  if instruction.disasm.startswith('.byte '):
    raise DoNotMatchError(instruction)

  try:
    _ValidateNop(instruction)
    return Condition(), Condition()
  except DoNotMatchError:
    pass

  # Report error on duplicate prefixes (note that they are allowed in nops),.
  _GetLegacyPrefixes(instruction)

  try:
    _ValidateOperandlessInstruction(instruction, bitness)
    return Condition(), Condition()
  except DoNotMatchError:
    pass

  if bitness == 32:
    try:
      _ValidateStringInstruction(instruction)
      return Condition(), Condition()
    except DoNotMatchError:
      pass

    try:
      _ValidateTlsInstruction(instruction)
      return Condition(), Condition()
    except DoNotMatchError:
      pass

  _, name, ops = _ParseInstruction(instruction)
  # TODO(shcherbina): prohibit excessive prefixes.

  for op in ops:
    m = re.match(_MemoryRE() + r'$', op)
    if m is not None and m.group('memory_segment') is not None:
      raise SandboxingError(
          'segments in memory references are not allowed', instruction)

  if bitness == 32:
    if _InstructionNameIn(
        name,
        ['mov', 'add', 'sub', 'and', 'or', 'xor',
         'xchg', 'xadd',
         'inc', 'dec', 'neg', 'not',
        ]):
      return Condition(), Condition()
    else:
      raise DoNotMatchError(instruction)

  elif bitness == 64:
    precondition = Condition()
    postcondition = Condition()
    zero_extending = False

    if _InstructionNameIn(
          name, ['mov', 'add', 'sub', 'and', 'or', 'xor']):
      assert len(ops) == 2
      zero_extending = True
      write_ops = [ops[1]]
    elif _InstructionNameIn(name, ['xchg', 'xadd']):
      assert len(ops) == 2
      zero_extending = True
      write_ops = ops
    elif _InstructionNameIn(name, ['inc', 'dec', 'neg', 'not']):
      assert len(ops) == 1
      zero_extending = True
      write_ops = ops
    else:
      raise DoNotMatchError(instruction)

    precondition = _ProcessMemoryAccess(instruction, ops)
    postcondition = _ProcessOperandWrites(
        instruction, write_ops, zero_extending)

    return precondition, postcondition

  else:
    assert False, bitness


def ValidateDirectJump(instruction):
  cond_jumps_re = re.compile(
      r'(data16 )?'
      r'(ja(e?)|jb(e?)|jg(e?)|jl(e?)|'
      r'j(n?)e|j(n?)o|j(n?)p|j(n?)s)'
      r' %s$' % _HexRE('destination'))
  m = cond_jumps_re.match(instruction.disasm)
  if m is not None:
    # Unfortunately we can't rely on presence of 'data16' prefix in disassembly,
    # because nacl-objdump prints it, but objdump we base our decoder on
    # does not.
    # So we look at bytes.
    if 0x66 in _GetLegacyPrefixes(instruction):
      raise SandboxingError(
          '16-bit conditional jumps are disallowed', instruction)
    return int(m.group('destination'), 16)

  jumps_re = re.compile(r'(jmp|call)(|w|q) %s$' % _HexRE('destination'))
  m = jumps_re.match(instruction.disasm)
  if m is not None:
    if m.group(2) == 'w':
      raise SandboxingError('16-bit jumps are disallowed', instruction)
    return int(m.group('destination'), 16)

  raise DoNotMatchError(instruction)


def ValidateDirectJumpOrRegularInstruction(instruction, bitness):
  """Validate anything that is not superinstruction.

  Args:
    instruction: objdump_parser.Instruction tuple.
    bitness: 32 or 64.
  Returns:
    Triple (jump_destination, precondition, postcondition).
    jump_destination is either absolute offset or None if instruction is not
    jump. Pre/postconditions are as in ValidateRegularInstructions.
  Raises:
    According to usual convention.
  """
  assert bitness in [32, 64]
  try:
    destination = ValidateDirectJump(instruction)
    return destination, Condition(), Condition()
  except DoNotMatchError:
    pass

  precondition, postcondition = ValidateRegularInstruction(instruction, bitness)
  return None, precondition, postcondition


def ValidateSuperinstruction32(superinstruction):
  """Validate superinstruction with ia32 set of regexps.

  If set of instructions includes something unknown (unknown functions
  or prefixes, wrong number of instructions, etc), then assert is triggered.

  There corner case exist: naclcall/nacljmp instruction sequences are too
  complex to process by DFA alone (it produces too large DFA and MSVC chokes
  on it) thus it's verified partially by DFA and partially by code in
  actions.  For these we generate either "True" or "False".

  Args:
      superinstruction: list of objdump_parser.Instruction tuples
  """

  call_jmp = re.compile(
      r'(call|jmp) '  # call or jmp
      r'[*](?P<register>%e[a-z]+)$')  # register name

  # TODO(shcherbina): actually we only want to allow 0xffffffe0 as a mask,
  # but it's safe anyway because what really matters is that lower 5 bits
  # of the mask are zeroes.
  # Disallow 0xe0 once
  # https://code.google.com/p/nativeclient/issues/detail?id=3164 is fixed.
  and_for_call_jmp = re.compile(
      r'and [$]0x(ffffff)?e0,(?P<register>%e[a-z]+)$')

  dangerous_instruction = superinstruction[-1].disasm

  if call_jmp.match(dangerous_instruction):
    # If "dangerous instruction" is call or jmp then we need to check if two
    # lines match

    if len(superinstruction) != 2:
      raise DoNotMatchError(superinstruction)

    m = and_for_call_jmp.match(superinstruction[0].disasm)
    if m is None:
      raise DoNotMatchError(superinstruction)
    register_and = m.group('register')

    m = call_jmp.match(dangerous_instruction)
    if m is None:
      raise DoNotMatchError(superinstruction)
    register_call_jmp = m.group('register')

    if register_and == register_call_jmp:
      return

    raise SandboxingError(
        'nacljump32/naclcall32: {0} != {1}'.format(
            register_and, register_call_jmp),
        superinstruction)

  raise DoNotMatchError(superinstruction)


def ValidateSuperinstruction64(superinstruction):
  """Validate superinstruction with x86-64 set of regexps.

  If set of instructions includes something unknown (unknown functions
  or prefixes, wrong number of instructions, etc), then assert is triggered.

  There corner case exist: naclcall/nacljmp instruction sequences are too
  complex to process by DFA alone (it produces too large DFA and MSVC chokes
  on it) thus it's verified partially by DFA and partially by code in
  actions.  For these we generate either "True" or "False", other
  superinstruction always produce "True" or throw an error.

  Args:
      superinstruction: list of objdump_parser.Instruction tuples
  """

  dangerous_instruction = superinstruction[-1].disasm

  # This is dangerous instructions in naclcall/nacljmp
  callq_jmpq = re.compile(
      r'(callq|jmpq) ' # callq or jmpq
      r'[*](?P<register>%r[0-9a-z]+)$') # register name
  # These are sandboxing instructions for naclcall/nacljmp
  # TODO(shcherbina): actually we only want to allow 0xffffffe0 as a mask,
  # but it's safe anyway because what really matters is that lower 5 bits
  # of the mask are zeroes.
  # Disallow 0xe0 once
  # https://code.google.com/p/nativeclient/issues/detail?id=3164 is fixed.
  and_for_callq_jmpq = re.compile(
      r'and [$]0x(ffffff)?e0,(?P<register>%e[a-z][a-z]|%r[89]d|%r1[0-4]d)$')
  add_for_callq_jmpq = re.compile(
      r'add %r15,(?P<register>%r[0-9a-z]+)$')

  if callq_jmpq.match(dangerous_instruction):
    # If "dangerous instruction" is callq or jmpq then we need to check if all
    # three lines match

    if len(superinstruction) != 3:
      raise DoNotMatchError(superinstruction)

    m = and_for_callq_jmpq.match(superinstruction[0].disasm)
    if m is None:
      raise DoNotMatchError(superinstruction)
    register_and = m.group('register')

    m = add_for_callq_jmpq.match(superinstruction[1].disasm)
    if m is None:
      raise DoNotMatchError(superinstruction)
    register_add = m.group('register')

    m = callq_jmpq.match(dangerous_instruction)
    if m is None:
      raise DoNotMatchError(superinstruction)
    register_callq_jmpq = m.group('register')

    # Double-check that registers are 32-bit and convert them to 64-bit so
    # they can be compared
    if register_and[1] == 'e':
      register_and = '%r' + register_and[2:]
    elif re.match(r'%r\d+d', register_and):
      register_and = register_and[:-1]
    else:
      assert False, ('Unknown (or possible non-32-bit) register found. '
                     'This should never happen!')
    if register_and == register_add == register_callq_jmpq:
      return

    raise SandboxingError(
        'nacljump64/naclcall64: registers do not match ({0}, {1}, {2})'.format(
            register_and, register_add, register_callq_jmpq),
        superinstruction)

    raise DoNotMatchError(superinstruction)

  # These are dangerous string instructions (there are three cases)
  string_instruction_rdi_no_rsi = re.compile(
      r'(maskmovq %mm[0-7],%mm[0-7]|' # maskmovq
      r'v?maskmovdqu %xmm([0-9]|1[0-5]),%xmm([0-9]|1[0-5])|' # [v]maskmovdqu
      r'((repnz|repz) )?scas %es:[(]%rdi[)],(%al|%ax|%eax|%rax)|' # scas
      r'(rep )?stos (%al|%ax|%eax|%rax),%es:[(]%rdi[)])$') # stos
  string_instruction_rsi_no_rdi = re.compile(
      r'(rep )?lods %ds:[(]%rsi[)],(%al|%ax|%eax|%rax)$') # lods
  string_instruction_rsi_rdi = re.compile(
      r'(((repnz|repz) )?cmps[blqw] %es:[(]%rdi[)],%ds:[(]%rsi[)]|' # cmps
      r'(rep )?movs[blqw] %ds:[(]%rsi[)],%es:[(]%rdi[)])$') # movs
  # These are sandboxing instructions for string instructions
  mov_esi_esi = re.compile(r'mov %esi,%esi$')
  lea_r15_rsi_rsi = re.compile(r'lea [(]%r15,%rsi,1[)],%rsi$')
  mov_edi_edi = re.compile(r'mov %edi,%edi$')
  lea_r15_rdi_rdi = re.compile(r'lea [(]%r15,%rdi,1[)],%rdi$')

  if string_instruction_rsi_no_rdi.match(dangerous_instruction):
    if len(superinstruction) != 3:
      raise DoNotMatchError(superinstruction)
    if mov_esi_esi.match(superinstruction[0].disasm) is None:
      raise DoNotMatchError(superinstruction)
    if lea_r15_rsi_rsi.match(superinstruction[1].disasm) is None:
      raise DoNotMatchError(superinstruction)
    return

  if string_instruction_rdi_no_rsi.match(dangerous_instruction):
    if len(superinstruction) != 3:
      raise DoNotMatchError(superinstruction)
    if mov_edi_edi.match(superinstruction[0].disasm) is None:
      raise DoNotMatchError(superinstruction)
    if lea_r15_rdi_rdi.match(superinstruction[1].disasm) is None:
      raise DoNotMatchError(superinstruction)
    # vmaskmovdqu is disabled for compatibility with the previous validator
    if dangerous_instruction.startswith('vmaskmovdqu '):
      raise SandboxingError('vmaskmovdqu is disallowed', superinstruction)
    return

  if string_instruction_rsi_rdi.match(dangerous_instruction):
    if len(superinstruction) != 5:
      raise DoNotMatchError(superinstruction)
    if mov_esi_esi.match(superinstruction[0].disasm) is None:
      raise DoNotMatchError(superinstruction)
    if lea_r15_rsi_rsi.match(superinstruction[1].disasm) is None:
      raise DoNotMatchError(superinstruction)
    if mov_edi_edi.match(superinstruction[2].disasm) is None:
      raise DoNotMatchError(superinstruction)
    if lea_r15_rdi_rdi.match(superinstruction[3].disasm) is None:
      raise DoNotMatchError(superinstruction)
    return

  raise DoNotMatchError(superinstruction)
