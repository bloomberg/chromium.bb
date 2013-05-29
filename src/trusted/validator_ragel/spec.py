#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Executable specification of valid instructions and superinstructions (in terms
# of their disassembler listing).
# Should serve as formal and up-to-date ABI reference and as baseline for
# validator exhaustive tests.

import re


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
  Returns:
      True if superinstruction is valid, otherwise False
  """

  call_jmp = re.compile(
      r'(call|jmp) ' # call or jmp
      r'[*](?P<register>%e[a-z]+)$') # register name

  and_for_call_jmp = re.compile(
      r'and [$]0xffffffe0,(?P<register>%e[a-z]+)$')

  dangerous_instruction = superinstruction[-1].disasm

  if call_jmp.match(dangerous_instruction):
    # If "dangerous instruction" is call or jmp then we need to check if two
    # lines match
    assert len(superinstruction) == 2
    register_and = (
        and_for_call_jmp.match(superinstruction[0].disasm).group('register'))
    register_call_jmp = call_jmp.match(dangerous_instruction).group('register')
    if register_and == register_call_jmp:
      return True
    return False

  assert False, ('Unknown "dangerous" instruction: {0}'.
                 format(dangerous_instruction))


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
  Returns:
      True if superinstruction is valid, otherwise False
  """

  dangerous_instruction = superinstruction[-1].disasm

  # This is dangerous instructions in naclcall/nacljmp
  callq_jmpq = re.compile(
      r'(callq|jmpq) ' # callq or jmpq
      r'[*](?P<register>%r[0-9a-z]+)$') # register name
  # These are sandboxing instructions for naclcall/nacljmp
  and_for_callq_jmpq = re.compile(
      r'and [$]0xffffffe0,(?P<register>%e[a-z][a-z]|%r[89]d|%r1[0-4]d)$')
  add_for_callq_jmpq = re.compile(
      r'add %r15,(?P<register>%r[0-9a-z]+)$')

  if callq_jmpq.match(dangerous_instruction):
    # If "dangerous instruction" is callq or jmpq then we need to check if all
    # three lines match
    assert len(superinstruction) == 3
    register_and = (
        and_for_callq_jmpq.match(superinstruction[0].disasm).group('register'))
    register_add = (
        add_for_callq_jmpq.match(superinstruction[1].disasm).group('register'))
    register_callq_jmpq = (
        callq_jmpq.match(dangerous_instruction).group('register'))
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
      return True
    return False

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
    assert len(superinstruction) == 3
    assert mov_esi_esi.match(superinstruction[0].disasm) is not None
    assert lea_r15_rsi_rsi.match(superinstruction[1].disasm) is not None
    return True

  if string_instruction_rdi_no_rsi.match(dangerous_instruction):
    assert len(superinstruction) == 3
    assert mov_edi_edi.match(superinstruction[0].disasm) is not None
    assert lea_r15_rdi_rdi.match(superinstruction[1].disasm) is not None
    # vmaskmovdqu is disabled for compatibility with the previous validator
    return not dangerous_instruction.startswith('vmaskmovdqu ')

  if string_instruction_rsi_rdi.match(dangerous_instruction):
    assert len(superinstruction) == 5
    assert mov_esi_esi.match(superinstruction[0].disasm) is not None
    assert lea_r15_rsi_rsi.match(superinstruction[1].disasm) is not None
    assert mov_edi_edi.match(superinstruction[2].disasm) is not None
    assert lea_r15_rdi_rdi.match(superinstruction[3].disasm) is not None
    return True

  assert False, ('Unknown "dangerous" instruction: {0}'.
                 format(dangerous_instruction))
