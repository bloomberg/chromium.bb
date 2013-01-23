#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import re
import StringIO


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


class Operand(object):

  __slots__ = [
      'read_write_attr',
      'arg_type',
      'size',
      'implicit',
      'index']

  # Operand read/write modes.
  UNUSED = '\''
  READ = '='
  WRITE = '!'
  READ_WRITE = '&'

  read_write_attr_regex = r'[%s%s%s%s]?' % (UNUSED, READ, WRITE, READ_WRITE)
  arg_type_regex = r'[acdbfgioprtxBCDEGHIJLMNOPQRSUVWXY]'
  size_regex = (
      r'|2|7|b|d|do|dq|fq|o|p|pb|pd|pdw|pdwx|pdx|ph|phx|pi|pj|pjx|pk|pkx|'
      r'pq|pqw|pqwx|pqx|ps|psx|pw|q|r|s|sb|sd|se|si|sq|sr|ss|st|sw|sx|'
      r'v|w|x|y|z')
  implicit_mark_regex = r'\*?'

  operand_regex = re.compile(r'(%s)(%s)(%s)(%s)$' % (
      read_write_attr_regex,
      arg_type_regex,
      size_regex,
      implicit_mark_regex))

  @staticmethod
  def Parse(s, default_rw):
    m = Operand.operand_regex.match(s)

    return Operand(
        read_write_attr=m.group(1) or default_rw,
        arg_type=m.group(2),
        size=m.group(3),
        implicit=(m.group(4) == '*'))

  def __init__(self, read_write_attr, arg_type, size, implicit=False):
    self.read_write_attr = read_write_attr
    self.arg_type = arg_type
    self.size = size
    self.implicit = implicit

  def Readable(self):
    return self.read_write_attr in [self.READ, self.READ_WRITE]

  def Writable(self):
    return self.read_write_attr in [self.WRITE, self.READ_WRITE]

  def ResidesInModRM(self):
    return self.arg_type in 'CDEGMNPQRSUVW'

  def GetFormat(self):
    """Get human-readable string for operand type and size.

    This string is used as a suffix in action names like 'operand0_8bit'. Values
    set by these actions are in turn used by disassembler to determine how to
    print operand.

    Actually, there is one format ('memory') that is never returned because it
    is handled at a higher level.

    TODO(shcherbina): it is possible that format will also be needed by
    validator64 in order to identify zero-extending instruction, but I'm not
    sure how it will be done.

    Returns:
      String like '8bit', '32bit', 'xmm', 'mmx', etc.
    """
    if self.size == 'b':
      return '8bit'
    # TODO(shcherbina): support other formats.
    raise NotImplementedError()

  def __str__(self):
    return '%s%s%s%s' % (
        self.read_write_attr,
        self.arg_type,
        self.size,
        '*' if self.implicit else '')


class Instruction(object):

  __slots__ = [
      'name',
      'operands',
      'opcodes',
      'attributes']

  @staticmethod
  def Parse(line):
    """Parse one line of def file and return initialized Instruction object.

    Args:
      line: One line of def file (two or three columns separated by
          COLUMN_SEPARATOR). First column defines instruction name and operands,
          second one - opcodes and encoding details, third (optional)
          one - instruction attributes.

    Returns:
      Fully initialized Instruction object.
    """

    instruction = Instruction()

    columns = line.split(COLUMN_SEPARATOR)

    # Third column is optional.
    assert 2 <= len(columns) <= 3, line
    name_and_operands_column = columns[0]
    opcodes_column = columns[1]
    if len(columns) == 3:
      attributes = columns[2].split()
    else:
      attributes = []

    instruction.ParseNameAndOperands(name_and_operands_column)
    instruction.ParseOpcodes(opcodes_column)

    for attribute in attributes:
      assert attribute in SUPPORTED_ATTRIBUTES, attribute
    instruction.attributes = attributes

    return instruction

  def ParseNameAndOperands(self, s):
    # If instruction name is quoted, it is allowed to contain spaces.
    if s.startswith('"'):
      i = s.index('"', 1)
      self.name = s[1:i]
      operands = s[i+1:].split()
    else:
      operands = s.split()
      self.name = operands.pop(0)

    self.operands = []
    for i, op in enumerate(operands):
      # By default one- and two-operand instructions are assumed to read all
      # operands and store result to the last one, while instructions with
      # three or more operands are assumed to read all operands except last one
      # which is used to store the result of the execution.
      last = (i == len(operands) - 1)
      if len(operands) <= 2:
        default_rw = Operand.READ_WRITE if last else Operand.READ
      else:
        default_rw = Operand.WRITE if last else Operand.READ
      operand = Operand.Parse(op, default_rw=default_rw)
      operand.index = i
      self.operands.append(operand)

  def ParseOpcodes(self, opcodes_column):
    opcodes = opcodes_column.split()

    # TODO(shcherbina): Just remove these 'opcodes' from .def files once
    # gen_dfa.py is enabled. We can't do that right now because old gen_dfa
    # uses them, even though they are redundant.
    opcodes = [opcode for opcode in opcodes if opcode not in ['/r', '/m', '/s']]

    opcode_regex = re.compile(
        r'0x[0-9a-f]{2}|'  # raw bytes
        r'data16|'
        r'rexw|'  # REX prefix with W-bit set
        r'/[0-7]|'  # opcode stored in ModRM byte
        r'/|'  # precedes 3DNow! opcode map
        r'RXB\.([01][0-9A-F]|[01]{5})|'  # VEX-specific, 5 bits (hex or binary)
        r'[xW01]\.(src|src1|dest|cntl|1111)\.[Lx01]\.[01]{2}'  # VEX-specific
    )

    assert len(opcodes) > 0
    for opcode in opcodes:
      assert opcode_regex.match(opcode), opcode

    self.opcodes = opcodes

  def HasModRM(self):
    return any(operand.ResidesInModRM() for operand in self.operands)

  def FindOperand(self, arg_type):
    result = None
    for operand in self.operands:
      if operand.arg_type == arg_type:
        assert result is None, 'multiple operands of type %s' % arg_type
        result = operand
    return result

  def HasRegisterInOpcode(self):
    return self.FindOperand('r') is not None

  def HasOpcodeInsteadOfImmediate(self):
    return '/' in self.opcodes

  def GetMainOpcodePart(self):
    result = []
    for opcode in self.opcodes:
      if opcode.startswith('/'):
        # Either '/' (opcode in immediate) or '/0'-'/7' (opcode in ModRM).
        # Anyway, main part of the opcode is over.
        break
      result.append(opcode)
    return result

  def GetNameAsIdentifier(self):
    """Return name in a form suitable to use as part of C identifier.

    In principle, collisions are possible, but will result in compilation,
    failure, so we are not checking for them here for simplicity.

    Returns:
      Instruction name with all non-alphanumeric characters replaced with '_'.
    """
    return re.sub(r'\W', '_', self.name)

  def __str__(self):
    return ' '.join([self.name] + map(str, self.operands))


DECODER = object()
VALIDATOR = object()


class InstructionPrinter(object):

  def __init__(self, mode, bitness):
    assert mode in [DECODER, VALIDATOR]
    assert bitness in [32, 64]
    self.mode = mode
    self.bitness = bitness
    self.out = StringIO.StringIO()

  def GetContent(self):
    return self.out.getvalue()

  def PrintOpcode(self, instruction):
    main_opcode_part = instruction.GetMainOpcodePart()
    if instruction.HasRegisterInOpcode():
      assert not instruction.HasModRM()
      assert not instruction.HasOpcodeInsteadOfImmediate()

      self.out.write(' '.join(main_opcode_part[:-1]))

      # Register is encoded in three least significant bits of the last byte
      # of the opcode (in 64-bit case REX.B bit also will be involved, but it
      # will be handled elsewhere).
      last_byte = int(main_opcode_part[-1], 16)
      assert last_byte & 0b00000111 == 0
      self.out.write(' (%s)' % '|'.join(
          hex(b) for b in range(last_byte, last_byte + 2**3)))

      # TODO(shcherbina): Print operand*_from_opcode(_x87) actions (or maybe
      # do it somewhere else).
    else:
      self.out.write(' '.join(main_opcode_part))

  def PrintSignature(self, instruction):
    """Print actions specifying instruction name and info about its operands.

    Example signature:
      @instruction_add
      @operands_count_is_2
      @operand0_8bit
      @operand1_8bit

    Depending on the mode, parts of the signature may be omitted.

    Args:
      instruction: instruction (with full information about operand types, etc.)

    Returns:
      None.
    """
    if self.mode == DECODER:
      self.out.write('@instruction_%s\n' % instruction.GetNameAsIdentifier())
      self.out.write('@operands_count_is_%d\n' % len(instruction.operands))

    # TODO(shcherbina): 'memory' format?
    for i, operand in enumerate(instruction.operands):
      self.out.write('@operand%d_%s\n' % (i, operand.GetFormat()))

    # TODO(shcherbina): print operand sources and extract implicit operands.

    # TODO(shcherbina): print '@last_byte_is_not_immediate' when approptiate.
    # TODO(shcherbina): print '@modifiable_instruction' in validator64 if
    # attribute 'nacl-amd64-modifiable' is present.
    # TODO(shcherbina): print info about CPU features.
    # TODO(shcherbina): att_show_name_suffix.

    # TODO(shcherbina): print spurious REX stuff (probably not in this
    # function).

  def PrintImplicitOperandSources(self, instruction):
    """Print actions specifying sources of implicit operands.

    Args:
      instruction: instruction.

    Returns:
      None.
    """
    operand = instruction.FindOperand('a')
    if operand is not None:
      self.out.write('@operand%d_rax\n' % operand.index)
    # TODO(shcherbina): handle other implicit operands.

  def PrintInstructionWithoutModRM(self, instruction):
    # TODO(shcherbina): print legacy prefixes.
    # TODO(shcherbina): print REX prefix.

    assert not instruction.HasModRM()
    assert not instruction.HasOpcodeInsteadOfImmediate(), 'not supported yet'

    self.PrintOpcode(instruction)
    self.out.write('\n')

    self.PrintSignature(instruction)
    self.PrintImplicitOperandSources(instruction)

    # TODO(shcherbina): print immediate or relative args.

    # Displacement encoded in the instruction.
    operand = instruction.FindOperand('O')
    if operand is not None:
      self.out.write('@operand%d_absolute_disp\n' % operand.index)
      self.out.write('disp%d\n' % self.bitness)

    # TODO(shcherbina): add mechanism to check that all operand sources are
    # printed.

    # TODO(shcherbina): subtract NOP from XCHG


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

    Instruction.Parse(line)


def GenerateLegacyPrefixes(required_prefixes, optional_prefixes):
  """Produce list of all possible combinations of legacy prefixes.

  Legacy prefixes are defined in processor manual:
    operand-size override (data16),
    address-size override,
    segment override,
    LOCK,
    REP/REPE/REPZ,
    REPNE/REPNZ.

  All permutations are enumerated, but repeated prefixes are not allowed
  (they make state count too large), even though processor decoder allows
  repetitions.

  In the future we might want to decide on a single preferred order of prefixes
  that validator allows.

  Args:
    required_prefixes: List of prefixes that have to be included in each
        combination produced
    optional_prefixes: List of prefixes that may or may not be present in
        resulting combinations.
  Returns:
    List of tuples of prefixes.
  """
  all_prefixes = required_prefixes + optional_prefixes
  assert len(set(all_prefixes)) == len(all_prefixes), 'duplicate prefixes'

  required_prefixes = tuple(required_prefixes)

  result = []
  for k in range(len(optional_prefixes) + 1):
    for optional in itertools.combinations(optional_prefixes, k):
      for prefixes in itertools.permutations(required_prefixes + optional):
        result.append(prefixes)

  assert len(set(result)) == len(result), 'duplicate resulting combinations'

  return result


def main():
  pass


if __name__ == '__main__':
  main()
