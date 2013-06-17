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
  if instruction.disasm in [
      'nop',
      'xchg %ax,%ax',
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
  return r'(?P<%s>0x[\da-f]+)' % group_name


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
  return r'(?P<%s>%s?(\(%s?(,%s,\d)?\))?)' % (
      group_name,
      _HexRE(group_name=group_name + '_offset'),
      _AnyRegisterRE(group_name=group_name + '_base'),
      _AnyRegisterRE(group_name=group_name + '_index'))


def _OperandRE(group_name='operand'):
  return r'(?P<%s>%s|%s|%s)' % (
      group_name,
      _AnyRegisterRE(group_name=group_name + '_register'),
      _ImmediateRE(group_name=group_name + '_immediate'),
      _MemoryRE(group_name=group_name + '_memory'))


def _SplitOps(insn, args):
  # We can't use just args.split(',') because operands can contain commas
  # themselves, for example '(%r15,%rax,1)'.
  ops = []
  i = 0
  while True:
    m = re.compile(_OperandRE()).match(args, i)
    assert m is not None, (args, i)
    ops.append(m.group())
    i = m.end()
    if i == len(args):
      break
    assert args[i] == ',', (insn, args, i)
    i += 1
  return ops


def _ParseInstruction(instruction):
  elems = instruction.disasm.split()
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
    assert False, elems

  return prefixes, name, ops


def ValidateRegularInstruction(instruction, bitness):
  assert bitness in [32, 64]

  try:
    _ValidateNop(instruction)
    return
  except DoNotMatchError:
    pass

  try:
    _ValidateOperandlessInstruction(instruction, bitness)
    return
  except DoNotMatchError:
    pass

  if bitness == 32:
    try:
      _ValidateStringInstruction(instruction)
      return
    except DoNotMatchError:
      pass

    try:
      _ValidateTlsInstruction(instruction)
      return
    except DoNotMatchError:
      pass

  _ParseInstruction(instruction)

  raise DoNotMatchError(instruction)


def ValidateDirectJump(instruction):
  # TODO(shcherbina): return offset for potential use in text-based ncval

  cond_jumps_re = re.compile(
      r'(ja(e?)|jb(e?)|jg(e?)|jl(e?)|'
      r'j(n?)e|j(n?)o|j(n?)p|j(n?)s)'
      r' %s$' % _HexRE('destination'))
  m = cond_jumps_re.match(instruction.disasm)
  if m is not None:
    # 16-bit conditional jump has the following form:
    #   <optional branch hint (2e or 3e)>
    #   <data16 (66)>
    #   0f <cond jump opcode>
    #   <2-byte offset>
    # (branch hint and data16 can go in any order)
    if instruction.bytes[0] == 0x66 or (
        instruction.bytes[0] in [0x2e, 0x3e] and instruction.bytes[1] == 0x66):
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
