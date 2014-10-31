# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Templates for grouping together similar proofs."""

import proof_tools


def LockedRegWithRegOrMem16bit(mnemonic_name, bitness):
  """Computes all 16 bit optionally locked reg, reg/mem ops.

  e.g.

  lock instr reg16 mem
    or
  instr reg16 mem/reg16.

  Note: when locking is issued, the second operand must be memory and not a reg.

  Args:
    mnemonic_name: the mnemonic we are producing the pattern for.
    bitness: the bitness of the architecture (32 or 64)
  Returns:
    All possible disassembly sequences that follow the pattern.
  """
  assert bitness in (32, 64)
  instr = proof_tools.MnemonicOp(mnemonic_name)
  reg16 = proof_tools.GprOperands(bitness=bitness, operand_size=16)
  mem = proof_tools.AllMemoryOperands(bitness=bitness)
  lock = proof_tools.LockPrefix()
  return (proof_tools.OpsProd(lock, instr, reg16, mem) |
          proof_tools.OpsProd(instr, reg16, reg16 | mem))
