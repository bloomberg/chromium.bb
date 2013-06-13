import spec
import validator


class Validator(object):

  BITNESS = None
  MAX_SUPERINSTRUCTION_LENGTH = None

  def __init__(self, data):
    self.data = data
    self.messages = []
    self.valid_jump_targets = set()
    self.jumps = {}

  def ValidateSuperinstruction(self, superinstruction):
    raise NotImplementedError()

  def EnumerateInstructionCheckers(self):
    """Enumerate checkers for different kinds of (singleton) instructions.

    Returns:
      Sequence of callables that accept single instruction and follow usual
      spec convention (return / raise SandboxingError / raise DoNotMatchError).
      Each such callable is responsible for validation logic specific to a
      respective instruction class.
    """
    raise NotImplementedError()

  def ValidateDirectJump(self, instruction):
    destination = spec.ValidateDirectJump(instruction)
    self.jumps[instruction.address] = destination

  def FitsWithinBundle(self, insns):
    offset = insns[0].address
    last_byte_offset = offset + sum(len(insn.bytes) for insn in insns) - 1
    return offset // spec.BUNDLE_SIZE == last_byte_offset // spec.BUNDLE_SIZE

  def Validate(self):
    if len(self.data) % spec.BUNDLE_SIZE != 0:
      self.messages.append((0, 'chunk size is not multiple of bundle size'))
      return

    try:
      insns = validator.DisassembleChunk(self.data, bitness=self.BITNESS)
    except validator.DisassemblerError as e:
      self.messages.append((e.offset, 'failed to decode instruction'))
      return

    i = 0

    while i < len(insns):
      offset = insns[i].address
      self.valid_jump_targets.add(offset)
      try:
        # Greedy: try to match longest superinstructions first.
        for n in range(self.MAX_SUPERINSTRUCTION_LENGTH, 1, -1):
          if i + n > len(insns):
            continue
          try:
            self.ValidateSuperinstruction(insns[i:i+n])
            if not self.FitsWithinBundle(insns[i:i+n]):
              self.messages.append(
                  (offset, 'superinstruction crosses bundle boundary'))
            i += n
            break
          except spec.DoNotMatchError:
            continue
        else:
          for checker in self.EnumerateInstructionCheckers():
            try:
              checker(insns[i])
              if not self.FitsWithinBundle(insns[i:i+1]):
                self.messages.append(
                    (offset, 'instruction crosses bundle boundary'))
              i += 1
              break
            except spec.DoNotMatchError:
              continue
          else:
            self.messages.append(
                (offset, 'unrecognized instruction %r' % insns[i].disasm))
            i += 1
      except spec.SandboxingError as e:
        self.messages.append((offset, str(e)))
        i += 1

    assert i == len(insns)

    for source, destination in sorted(self.jumps.items()):
      if (destination % spec.BUNDLE_SIZE != 0 and
          destination not in self.valid_jump_targets):
        self.messages.append(
            (source, 'jump into a middle of instruction (0x%x)' % destination))


class Validator32(Validator):

  BITNESS = 32
  MAX_SUPERINSTRUCTION_LENGTH = 2

  def ValidateSuperinstruction(self, superinstruction):
    spec.ValidateSuperinstruction32(superinstruction)

  def EnumerateInstructionCheckers(self):
    return [
        self.ValidateDirectJump,
        spec.ValidateNop,
        lambda insn: spec.ValidateOperandlessInstruction(insn, bitness=32),
    ]


class Validator64(Validator):

  # TODO(shcherbina): track restricted_register

  BITNESS = 64
  MAX_SUPERINSTRUCTION_LENGTH = 5

  def ValidateSuperinstruction(self, superinstruction):
    spec.ValidateSuperinstruction64(superinstruction)

  def EnumerateInstructionCheckers(self):
    return [
        self.ValidateDirectJump,
        spec.ValidateNop,
        lambda insn: spec.ValidateOperandlessInstruction(insn, bitness=64),
    ]
