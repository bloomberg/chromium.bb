#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import re
import StringIO

import def_format


def Attribute(name):
  assert name in def_format.SUPPORTED_ATTRIBUTES
  return name


class Operand(object):

  __slots__ = [
      'read_write_attr',
      'arg_type',
      'size',
      'implicit',
      'index']

  read_write_attr_regex = r'[%s%s%s%s]?' % (
      def_format.OperandReadWriteMode.UNUSED,
      def_format.OperandReadWriteMode.READ,
      def_format.OperandReadWriteMode.WRITE,
      def_format.OperandReadWriteMode.READ_WRITE)
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
    return self.read_write_attr in [def_format.OperandReadWriteMode.READ,
                                    def_format.OperandReadWriteMode.READ_WRITE]

  def Writable(self):
    return self.read_write_attr in [def_format.OperandReadWriteMode.WRITE,
                                    def_format.OperandReadWriteMode.READ_WRITE]

  def ResidesInModRM(self):
    return self.arg_type in [
        def_format.OperandType.CONTROL_REGISTER,
        def_format.OperandType.DEBUG_REGISTER,
        def_format.OperandType.REGISTER_IN_REG,
        def_format.OperandType.REGISTER_IN_RM,
        def_format.OperandType.REGISTER_OR_MEMORY,
        def_format.OperandType.MEMORY,
        def_format.OperandType.SEGMENT_REGISTER_IN_RM,
        def_format.OperandType.MMX_REGISTER_IN_RM,
        def_format.OperandType.MMX_REGISTER_IN_REG,
        def_format.OperandType.MMX_REGISTER_OR_MEMORY,
        def_format.OperandType.XMM_REGISTER_IN_RM,
        def_format.OperandType.XMM_REGISTER_IN_REG,
        def_format.OperandType.XMM_REGISTER_OR_MEMORY]

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
    if self.size == 'w':
      return '16bit'
    if self.size == 'd':
      return '32bit'
    if self.size == 'q':
      return '64bit'
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
      'attributes',
      'rex',
      'required_prefixes',
      'optional_prefixes']

  class RexStatus(object):
    __slots__ = [
        'b_matters',
        'x_matters',
        'r_matters',
        'w_matters',
        'w_set']

  def __init__(self):
    self.rex = self.RexStatus()
    self.rex.b_matters = False
    self.rex.x_matters = False
    self.rex.r_matters = False
    self.rex.w_matters = True
    self.rex.w_set = False

    self.required_prefixes = []
    self.optional_prefixes = []

  def CollectPrefixes(self):
    if Attribute('branch_hint') in self.attributes:
      self.optional_prefixes.append('branch_hint')
    if Attribute('condrep') in self.attributes:
      self.optional_prefixes.append('condrep')
    if Attribute('rep') in self.attributes:
      self.optional_prefixes.append('rep')

    while True:
      opcode = self.opcodes[0]
      if opcode == 'rexw':
        self.rex.w_set = True
      elif opcode in [
          'data16',
          '0x66',  # data16 as well (the difference is that it is treated as
                   # part of opcode, and not as operand size indicator)
          '0xf0',  # lock
          '0xf2',  # rep(nz)
          '0xf3']:  # repz/condrep/branch_hint
        self.required_prefixes.append(opcode)
      else:
        # Prefixes ended, we get to the regular part of the opcode.
        break
      del self.opcodes[0]

  @staticmethod
  def Parse(line):
    """Parse one line of def file and return initialized Instruction object.

    Args:
      line: One line of def file (two or three columns separated by
          def_format.COLUMN_SEPARATOR).
          First column defines instruction name and operands,
          second one - opcodes and encoding details,
          third (optional) one - instruction attributes.

    Returns:
      Fully initialized Instruction object.
    """

    instruction = Instruction()

    columns = line.split(def_format.COLUMN_SEPARATOR)

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

    instruction.attributes = map(Attribute, attributes)

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
      if last:
        if len(operands) <= 2:
          default_rw = def_format.OperandReadWriteMode.READ_WRITE
        else:
          default_rw = def_format.OperandReadWriteMode.WRITE
      else:
        default_rw = def_format.OperandReadWriteMode.READ
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
    return self.FindOperand(
        def_format.OperandType.REGISTER_IN_OPCODE) is not None

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

  def IsVexOrXop(self):
    return (self.opcodes[0] == '0xc4' and self.name != 'les' or
            self.opcodes[0] == '0x8f' and self.name != 'pop')

  def __str__(self):
    return ' '.join([self.name] + map(str, self.operands))


DECODER = object()
VALIDATOR = object()


class InstructionPrinter(object):

  def __init__(self, mode, bitness):
    assert mode in [DECODER, VALIDATOR]
    assert bitness in [32, 64]
    self._mode = mode
    self._bitness = bitness
    self._out = StringIO.StringIO()

  def GetContent(self):
    return self._out.getvalue()

  def _PrintRexPrefix(self, instruction):
    """Print a machine for REX prefix."""
    if self._bitness != 64:
      return
    if instruction.IsVexOrXop():
      return
    if Attribute('norex') in instruction.attributes:
      return

    if instruction.rex.w_set:
      self._out.write('REXW_RXB\n')
    else:
      self._out.write('REX_RXB?\n')

  def _PrintOpcode(self, instruction):
    """Print a machine for opcode."""
    main_opcode_part = instruction.GetMainOpcodePart()
    if instruction.HasRegisterInOpcode():
      assert not instruction.HasModRM()
      assert not instruction.HasOpcodeInsteadOfImmediate()

      self._out.write(' '.join(main_opcode_part[:-1]))

      # Register is encoded in three least significant bits of the last byte
      # of the opcode (in 64-bit case REX.B bit also will be involved, but it
      # will be handled elsewhere).
      last_byte = int(main_opcode_part[-1], 16)
      assert last_byte & 0b00000111 == 0
      self._out.write(' (%s)' % '|'.join(
          hex(b) for b in range(last_byte, last_byte + 2**3)))

      # TODO(shcherbina): Print operand*_from_opcode(_x87) actions (or maybe
      # do it somewhere else).
    else:
      self._out.write(' '.join(main_opcode_part))

  def _PrintSignature(self, instruction):
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
    if self._mode == DECODER:
      self._out.write('@instruction_%s\n' % instruction.GetNameAsIdentifier())
      self._out.write('@operands_count_is_%d\n' % len(instruction.operands))

    # TODO(shcherbina): 'memory' format?
    for operand in instruction.operands:
      if self._NeedOperandInfo(operand):
        self._out.write('@operand%d_%s\n' %
                        (operand.index, operand.GetFormat()))

    # TODO(shcherbina): print operand sources and extract implicit operands.

    # TODO(shcherbina): print '@last_byte_is_not_immediate' when approptiate.
    # TODO(shcherbina): print '@modifiable_instruction' in validator64 if
    # attribute 'nacl-amd64-modifiable' is present.
    # TODO(shcherbina): print info about CPU features.
    # TODO(shcherbina): att_show_name_suffix.

    if (self._mode == DECODER and
        self._bitness == 64 and
        not instruction.IsVexOrXop()):
      # Note that even if 'norex' attribute is present, we print
      # @spurious_rex_... actions because NOP needs them (and it has REX
      # prefix specified as part of the opcode).
      # TODO(shcherbina): fix that?
      if not instruction.rex.b_matters:
        self._out.write('@set_spurious_rex_b\n')
      if not instruction.rex.x_matters:
        self._out.write('@set_spurious_rex_x\n')
      if not instruction.rex.r_matters:
        self._out.write('@set_spurious_rex_r\n')
      if not instruction.rex.w_matters:
        self._out.write('@set_spurious_rex_w\n')

  def _NeedOperandInfo(self, operand):
    """Whether we need to print actions describing operand format and source."""
    if self._mode == VALIDATOR and self._bitness == 64:
      # TODO(shcherbina): In this case we are only interested in writable
      # regular registers.
      raise NotImplementedError()

    return self._mode == DECODER

  def _PrintOperandSource(self, operand, source):
    """Print action specifying operand source."""
    # TODO(shcherbina): add mechanism to check that all operand sources are
    # printed.
    if self._NeedOperandInfo(operand):
      self._out.write('@operand%d_%s\n' % (operand.index, source))

  def _PrintImplicitOperandSources(self, instruction):
    """Print actions specifying sources of implicit operands.

    Args:
      instruction: instruction.

    Returns:
      None.
    """
    for operand in instruction.operands:
      if operand.arg_type == def_format.OperandType.ACCUMULATOR:
        self._PrintOperandSource(operand, 'rax')
      elif operand.arg_type == def_format.OperandType.DS_SI:
        self._PrintOperandSource(operand, 'ds_rsi')
      elif operand.arg_type == def_format.OperandType.ES_DI:
        self._PrintOperandSource(operand, 'es_rdi')

    # TODO(shcherbina): handle other implicit operands.

  def _PrintLegacyPrefixes(self, instruction):
    """Print a machine for all combinations of legacy prefixes."""
    legacy_prefix_combinations = GenerateLegacyPrefixes(
        instruction.required_prefixes,
        instruction.optional_prefixes)
    assert len(legacy_prefix_combinations) > 0
    if legacy_prefix_combinations == [()]:
      return
    self._out.write('(%s)' % ' | '.join(
        ' '.join(combination)
        for combination in legacy_prefix_combinations
        if combination != ()))
    # Use '(...)?' since Ragel does not allow '( | ...)'.
    if () in legacy_prefix_combinations:
      self._out.write('?')
    self._out.write('\n')

  def PrintInstructionWithoutModRM(self, instruction):
    assert not instruction.IsVexOrXop(), 'not supported yet'
    assert not instruction.HasModRM()

    self._PrintLegacyPrefixes(instruction)
    self._PrintRexPrefix(instruction)

    assert not instruction.HasOpcodeInsteadOfImmediate(), 'not supported yet'

    self._PrintOpcode(instruction)
    self._out.write('\n')

    self._PrintSignature(instruction)
    self._PrintImplicitOperandSources(instruction)

    # TODO(shcherbina): print immediate args.

    # Displacement encoded in the instruction.
    operand = instruction.FindOperand(def_format.OperandType.ABSOLUTE_DISP)
    if operand is not None:
      self._PrintOperandSource(operand, 'absolute_disp')
      self._out.write('disp%d\n' % self._bitness)

    # Relative jump/call target encoded in the instruction.
    operand = instruction.FindOperand(def_format.OperandType.RELATIVE_TARGET)
    if operand is not None:
      format = operand.GetFormat()
      if format == '8bit':
        self._out.write('rel8\n')
      elif format == '16bit':
        self._out.write('rel16\n')
      elif format == '32bit':
        self._out.write('rel32\n')
      else:
        assert False, format
      self._PrintOperandSource(operand, 'jmp_to')

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
      line = line[:-2] + def_format.COLUMN_SEPARATOR + next_line

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
