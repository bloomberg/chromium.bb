#!/usr/bin/python
# Copyright (c) 2013 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""%prog [option...] superinsructions1.txt superinsructions2.txt...

Regex-based verifier for superinstructions list."""

from __future__ import print_function

import collections
import optparse
import os
import re
import subprocess
import sys
import tempfile
import validator


ObjdumpLine = collections.namedtuple('ObjdumpLine',
                                     ['address', 'bytes', 'command'])


def SplitObjdumpLine(line):
  """Split objdump line.

  Typical line from objdump output looks like this:

  "   0:	48 c7 84 80 78 56 34 	movq   $0x12345678,0x12345678(%rax,%rax,4)"
  "   7:	12 78 56 34 12 "

  There are three columns (separated with tabs):
    address: offset for a particular line
    bytes: few bytes which encode a given command
    command: textual representation of command

  Third column is optional (as in example above).

  Args:
      line: objdump line (a single one).
  Returns
      ObjdumpLine tuple.
  """

  # Split columns
  split = line.strip().split('\t')
  if len(split) == 2:
    split.append('')

  # Split bytes
  split[1] = split[1].split()

  return ObjdumpLine(*split)


def RemoveRexFromAssemblerLine(line):
  """Remove rex from assembler line.

  Find and remove "rex" or "rex.WRXB" prefix from line."

  Args:
      line: source line (binary string)
  Returns
      line with rex prefix removed.
  """

  rex_prefix = re.compile(r'(rex([.]W?R?X?B?)? )')

  return re.sub(rex_prefix, '', line, count=1)


def ValidateSuperinstructionWithRegex32(superinstruction):
  """Validate superinstruction with ia32 set of regexps.

  If set of instructions includes something unknown (unknown functions
  or prefixes, wrong number of instructions, etc), then assert is triggered.

  There corner case exist: naclcall/nacljmp instruction sequences are too
  complex to process by DFA alone (it produces too large DFA and MSVC chokes
  on it) thus it's verified partially by DFA and partially by code in
  actions.  For these we generate either "True" or "False".

  Args:
      superinstruction: list of "ObjdumpLine"s from the objdump output
  Returns:
      True if superinstruction is valid, otherwise False
  """

  # Pick only disassembler text from objdump output
  disasm = [RemoveRexFromAssemblerLine(re.sub(' +', ' ', line.command))
            for line in superinstruction]

  call_jmp = re.compile(
      r'(call|jmp) ' # call or jmp
      r'[*](?P<register>%e[a-z]+)$') # register name

  and_for_call_jmp = re.compile(
      r'and [$]0xffffffe0,(?P<register>%e[a-z]+)$')

  dangerous_instruction = disasm[-1]

  if call_jmp.match(dangerous_instruction):
    # If "dangerous instruction" is call or jmp then we need to check if two
    # lines match
    assert len(disasm) == 2
    register_and = and_for_call_jmp.match(disasm[0]).group('register')
    register_call_jmp = call_jmp.match(disasm[1]).group('register')
    if register_and == register_call_jmp:
      return True
    return False

  assert False, ('Unknown "dangerous" instruction: {0}'.
                 format(dangerous_instruction))


def ValidateSuperinstructionWithRegex64(superinstruction):
  """Validate superinstruction with x86-64 set of regexps.

  If set of instructions includes something unknown (unknown functions
  or prefixes, wrong number of instructions, etc), then assert is triggered.

  There corner case exist: naclcall/nacljmp instruction sequences are too
  complex to process by DFA alone (it produces too large DFA and MSVC chokes
  on it) thus it's verified partially by DFA and partially by code in
  actions.  For these we generate either "True" or "False", other
  superinstruction always produce "True" or throw an error.

  Args:
      superinstruction: list of "ObjdumpLine"s from the objdump output
  Returns:
      True if superinstruction is valid, otherwise False
  """

  # Pick only disassembler text from objdump output
  disasm = [RemoveRexFromAssemblerLine(re.sub(' +', ' ', line.command))
            for line in superinstruction]

  dangerous_instruction = disasm[-1]

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
    assert len(disasm) == 3
    register_and = and_for_callq_jmpq.match(disasm[0]).group('register')
    register_add = add_for_callq_jmpq.match(disasm[1]).group('register')
    register_callq_jmpq = callq_jmpq.match(disasm[2]).group('register')
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
    assert len(disasm) == 3
    assert mov_esi_esi.match(disasm[0]) is not None
    assert lea_r15_rsi_rsi.match(disasm[1]) is not None
    return True

  if string_instruction_rdi_no_rsi.match(dangerous_instruction):
    assert len(disasm) == 3
    assert mov_edi_edi.match(disasm[0]) is not None
    assert lea_r15_rdi_rdi.match(disasm[1]) is not None
    # vmaskmovdqu is disabled for compatibility with the previous validator
    return not dangerous_instruction.startswith('vmaskmovdqu ')

  if string_instruction_rsi_rdi.match(dangerous_instruction):
    assert len(disasm) == 5
    assert mov_esi_esi.match(disasm[0]) is not None
    assert lea_r15_rsi_rsi.match(disasm[1]) is not None
    assert mov_edi_edi.match(disasm[2]) is not None
    assert lea_r15_rdi_rdi.match(disasm[3]) is not None
    return True

  assert False, ('Unknown "dangerous" instruction: {0}'.
                 format(dangerous_instruction))

def ValidateSuperinstruction(superinstruction, bitness):
  """Validate superinstruction using the actual validator.

  Args:
    superinstruction: superinstruction byte sequence to validate
    bitness: 32 or 64
  Returns:
    True if superinstruction is valid, otherwise False
  """

  bundle = bytes(superinstruction +
                 ((-len(superinstruction)) % validator.BUNDLE_SIZE) *  b'\x90')
  assert len(bundle) % validator.BUNDLE_SIZE == 0

  result = validator.ValidateChunk(bundle, bitness=bitness)

  # Superinstructions are accepted if restricted_register != REG_RSP or REG_RBP
  if bitness == 64:
    for register in validator.ALL_REGISTERS:
      # restricted_register can not be ever equal to %r15
      if register == validator.REG_R15:
        continue
      expected = result and register not in (validator.REG_RBP,
                                             validator.REG_RSP)
      assert validator.ValidateChunk(bundle,
                                     restricted_register=register,
                                     bitness=bitness) == expected

  # All bytes in the superinstruction are invalid jump targets
  #
  # Note: valid boundaries are determined in a C code without looking on
  # restricted_register variable.
  # TODO(khim): collect all the assumptions about C code fragments in one
  # document and move this note there.
  for offset in range(0, len(superinstruction) + 1):
    jmp_command = b'\xeb' + bytearray([0xfe - len(bundle) + offset])
    jmp_check = bundle + bytes(jmp_command) + (validator.BUNDLE_SIZE -
                                               len(jmp_command)) * b'\x90'
    expected = (offset == 0 or offset == len(superinstruction)) and result
    assert validator.ValidateChunk(jmp_check, bitness=bitness) == expected

  return result


def ProcessSuperinstructionsFile(filename, bitness, gas, objdump, out_file):
  """Process superinstructions file.

  Each line produces either "True" or "False" plus text of original command
  (for the documentation purposes).  "True" means instruction is safe, "False"
  means instruction is unsafe.  This is needed since some instructions are
  incorrect but accepted by DFA (these should be rejected by actions embedded
  in DFA).

  If line contains something except valid set of x86 instruction assert
  error is triggered.

  Args:
      filename: name of file to process
      bitness: 32 or 64
      gas: path to the GAS executable
      objdump: path to the OBJDUMP executable
  Returns:
      None
  """

  try:
    object_file = tempfile.NamedTemporaryFile(
        prefix='verify_superinstructions_',
        suffix='.o',
        delete=False)
    object_file.close()

    subprocess.check_call([gas,
                           '--{0}'.format(bitness),
                           filename,
                           '-o{0}'.format(object_file.name)])

    objdump_proc = subprocess.Popen(
        [objdump, '-d', object_file.name],
        stdout=subprocess.PIPE)

    objdump_lines = objdump_proc.stdout

    # Objdump prints few lines about file and section before disassembly
    # listing starts, so we have to skip that.
    for i in range(7):
      objdump_lines.readline()

    line_prefix = '.byte '

    with open(filename) as superinstructions:
      for superinstruction_line in superinstructions:
        # Split the source line to find bytes
        assert superinstruction_line.startswith(line_prefix)
        # Leave only bytes here
        bytes_only = superinstruction_line[len(line_prefix):]
        superinstruction_bytes = [byte.strip(' \n')
                                  for byte in bytes_only.split(',')]
        superinstruction_validated = ValidateSuperinstruction(
            bytearray([int(byte, 16) for byte in superinstruction_bytes]),
            bitness)
        # Pick disassembled form of the superinstruction from objdump output
        superinstruction = []
        objdump_bytes = []
        while len(objdump_bytes) < len(superinstruction_bytes):
          nextline = objdump_lines.readline().decode()
          instruction = SplitObjdumpLine(nextline)
          superinstruction.append(instruction)
          objdump_bytes += instruction.bytes
        # Bytes in objdump output in and source file should match
        assert ['0x' + b for b in objdump_bytes] == superinstruction_bytes
        if bitness == 32:
          validate_superinstruction = ValidateSuperinstructionWithRegex32
        else:
          validate_superinstruction = ValidateSuperinstructionWithRegex64
        superinstruction_valid = validate_superinstruction(superinstruction)
        assert superinstruction_validated == superinstruction_valid
  finally:
    os.remove(object_file.name)


def main():
  parser = optparse.OptionParser(__doc__)

  parser.add_option('-b', '--bitness',
                    type=int,
                    help='The subarchitecture: 32 or 64')
  parser.add_option('-a', '--gas',
                    help='Path to GNU AS executable')
  parser.add_option('-d', '--objdump',
                    help='Path to objdump executable')
  parser.add_option('-v', '--validator_dll',
                    help='Path to librdfa_validator_dll')
  parser.add_option('-o', '--out',
                    help='Output file name (instead of sys.stdout')

  (options, args) = parser.parse_args()

  if options.bitness not in [32, 64]:
    parser.error('specify -b 32 or -b 64')

  if not (options.gas and options.objdump and options.validator_dll):
    parser.error('specify path to gas, objdump, and validator_dll')

  if options.out is not None:
    out_file = open(options.out, "w")
  else:
    out_file = sys.stdout

  validator.Init(options.validator_dll)

  try:
    for file in args:
      ProcessSuperinstructionsFile(file,
                                   options.bitness,
                                   options.gas,
                                   options.objdump,
                                   out_file)
  finally:
    if out_file is not sys.stdout:
      out_file.close()


if __name__ == '__main__':
  main()
