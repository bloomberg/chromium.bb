#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


# Technically, columns are separated with mere ',' followed by spaces for
# readability, but there are quoted instruction names that include commas
# not followed by spaces (see nops.def).
# For simplicity I choose to rely on this coincidence and use split-based parser
# instead of proper recursive descent one.
# If by accident somebody put ', ' in quoted instruction name, it will fail
# loudly, because closing quote then will fall into second or third column and
# will cause parse error.
# TODO(shcherbina): use for column separator something that is never encountered
# in columns, like semicolon?
COLUMN_SEPARATOR = ', '


SUPPORTED_ATTRIBUTES = [
    # Parsing attributes.
    'branch_hint',
    'condrep',
    'lock',
    'no_memory_access',
    'norex',
    'norexw',
    'rep',

    # CPUID attributes.
    'CPUFeature_3DNOW',
    'CPUFeature_3DPRFTCH',
    'CPUFeature_AES',
    'CPUFeature_AESAVX',
    'CPUFeature_ALTMOVCR8',
    'CPUFeature_AVX',
    'CPUFeature_BMI1',
    'CPUFeature_CLFLUSH',
    'CPUFeature_CLMUL',
    'CPUFeature_CLMULAVX',
    'CPUFeature_CMOV',
    'CPUFeature_CMOVx87',
    'CPUFeature_CX16',
    'CPUFeature_CX8',
    'CPUFeature_E3DNOW',
    'CPUFeature_EMMX',
    'CPUFeature_EMMXSSE',
    'CPUFeature_F16C',
    'CPUFeature_FMA',
    'CPUFeature_FMA4',
    'CPUFeature_FXSR',
    'CPUFeature_LAHF',
    'CPUFeature_LWP',
    'CPUFeature_LZCNT',
    'CPUFeature_MMX',
    'CPUFeature_MON',
    'CPUFeature_MOVBE',
    'CPUFeature_MSR',
    'CPUFeature_POPCNT',
    'CPUFeature_SEP',
    'CPUFeature_SFENCE',
    'CPUFeature_SKINIT',
    'CPUFeature_SSE',
    'CPUFeature_SSE2',
    'CPUFeature_SSE3',
    'CPUFeature_SSE41',
    'CPUFeature_SSE42',
    'CPUFeature_SSE4A',
    'CPUFeature_SSSE3',
    'CPUFeature_SVM',
    'CPUFeature_SYSCALL',
    'CPUFeature_TBM',
    'CPUFeature_TSC',
    'CPUFeature_TSCP',
    'CPUFeature_TZCNT',
    'CPUFeature_x87',
    'CPUFeature_XOP',

    # Attributes for enabling/disabling based on architecture and validity.
    'ia32',
    'amd64',
    'nacl-ia32-forbidden',
    'nacl-amd64-forbidden',
    'nacl-forbidden',
    'nacl-amd64-zero-extends',
    'nacl-amd64-modifiable',

    # AT&T Decoder attributes.
    'att-show-memory-suffix-b',
    'att-show-memory-suffix-l',
    'att-show-memory-suffix-ll',
    'att-show-memory-suffix-t',
    'att-show-memory-suffix-s',
    'att-show-memory-suffix-q',
    'att-show-memory-suffix-x',
    'att-show-memory-suffix-y',
    'att-show-memory-suffix-w',
    'att-show-name-suffix-b',
    'att-show-name-suffix-l',
    'att-show-name-suffix-ll',
    'att-show-name-suffix-t',
    'att-show-name-suffix-s',
    'att-show-name-suffix-q',
    'att-show-name-suffix-x',
    'att-show-name-suffix-y',
    'att-show-name-suffix-w',

    # Spurious REX.W bits (instructions 'in', 'out', 'nop', etc).
    'spurious-rex.w'
]


def ParseInstructionAndOperands(s):
  # If instruction name is quoted, it is allowed to contain spaces.
  if s.startswith('"'):
    i = s.index('"', 1)
    instruction_name = s[1:i]
    operands = s[i+1:].split()
  else:
    operands = s.split()
    instruction_name = operands.pop(0)

  read_write_attr = r'[\'=!&]?'
  arg_type = r'[acdbfgioprtxBCDEGHIJLMNOPQRSUVWXY]'
  size = (r'|2|7|b|d|do|dq|fq|o|p|pb|pd|pdw|pdwx|pdx|ph|phx|pi|pj|pjx|pk|pkx|'
          r'pq|pqw|pqwx|pqx|ps|psx|pw|q|r|s|sb|sd|se|si|sq|sr|ss|st|sw|sx|'
          r'v|w|x|y|z')
  implicit_mark = r'\*?'

  operand_regex = re.compile(r'(%s)(%s)(%s)(%s)$' % (
      read_write_attr,
      arg_type,
      size, implicit_mark))

  for operand in operands:
    assert operand_regex.match(operand), operand


def ParseDefFile(filename):
  # .def file format is documented in general_purpose_instructions.def.

  with open(filename) as def_file:
    lines = [line for line in def_file if not line.startswith('#')]

  lines = iter(lines)
  while True:
    line = next(lines, None)
    if line is None:
      break

    # Comma-terminated lines are expected to be continued.
    while line.endswith(',\n'):
      next_line = next(lines)
      line = line[:-2] + COLUMN_SEPARATOR + next_line

    assert '#' not in line

    line = line.strip()
    if line == '':
      continue
    columns = line.split(COLUMN_SEPARATOR)

    # Third column is optional.
    assert 2 <= len(columns) <= 3, line
    instruction_and_operands = columns[0]
    opcodes = columns[1].split()
    if len(columns) == 3:
      attributes = columns[2].split()
    else:
      attributes = []

    ParseInstructionAndOperands(instruction_and_operands)

    opcode_regex = re.compile(
        r'0x[0-9a-f]{2}|'  # raw bytes
        r'data16|'
        r'rexw|'  # REX prefix with W-bit set
        r'fwait|'  # fwait is treated as prefix
        r'/[0-7]|'  # opcode stored in ModRM byte
        r'/r|'  # register operand
        r'/m|'  # memory operand
        r'/s|'  # segment register operand
        r'/|'  # precedes 3DNow! opcode map
        r'RXB\.([01][0-9A-F]|[01]{5})|'  # VEX-specific, 5 bits (hex or binary)
        r'[xW01]\.(src|src1|dest|cntl|1111)\.[Lx01]\.[01]{2}'  # VEX-specific
    )

    for opcode in opcodes:
      assert opcode_regex.match(opcode), opcode

    for attribute in attributes:
      assert attribute in SUPPORTED_ATTRIBUTES, attribute


def main():
  pass


if __name__ == '__main__':
  main()
