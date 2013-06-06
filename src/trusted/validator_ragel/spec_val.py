import spec
import validator


class Validator(object):

  BITNESS = None
  MAX_SUPERINSTRUCTION_LENGTH = None

  def __init__(self, data):
    self.data = data
    self.messages = []

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

  def Validate(self):
    if len(self.data) % spec.BUNDLE_SIZE != 0:
      self.messages.append((0, 'chunk size is not multiple of bundle size'))
      return

    insns = validator.DisassembleChunk(self.data, bitness=self.BITNESS)
    i = 0

    while i < len(insns):
      offset = insns[i].address
      try:
        # Greedy: try to match longest superinstructions first.
        for n in range(self.MAX_SUPERINSTRUCTION_LENGTH, 1, -1):
          if i + n > len(insns):
            continue
          try:
            self.ValidateSuperinstruction(insns[i:i+n])
            # TODO(shcherbina): Check for bundle boundaries.
            i += n
            break
          except spec.DoNotMatchError:
            continue
        else:
          # TODO(shcherbina): Check for bundle boundaries.
          for checker in self.EnumerateInstructionCheckers():
            try:
              checker(insns[i])
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

    # TODO(shcherbina): Check jump targets.


class Validator32(Validator):

  BITNESS = 32
  MAX_SUPERINSTRUCTION_LENGTH = 2

  def ValidateSuperinstruction(self, superinstruction):
    spec.ValidateSuperinstruction32(superinstruction)

  def ValidateDirectJump(self, instruction):
    spec.ValidateDirectJump(instruction)
    # TODO(shcherbina): Collect jump info.

  def EnumerateInstructionCheckers(self):
    return [
        spec.ValidateNop,
        self.ValidateDirectJump,
    ]
