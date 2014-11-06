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


def XmmYmmOrMemory3operand(mnemonic_name, bitness):
  """Set of 3 operand xmm/ymm/memory ops (memory is a possible source operand).

  e.g.

  instr xmm1/mem, xmm2, xmm3
  or
  instr ymm1/mem, ymm2, ymm3

  Args:
    mnemonic_name: the mnemonic we are producing the pattern for.
    bitness: the bitness of the architecture (32 or 64)
  Returns:
    All possible disassembly sequences that follow the pattern.
  """
  xmm = proof_tools.AllXMMOperands(bitness)
  ymm = proof_tools.AllYMMOperands(bitness)
  mem = proof_tools.AllMemoryOperands(bitness)
  mnemonic = proof_tools.MnemonicOp(mnemonic_name)

  return (proof_tools.OpsProd(mnemonic, (xmm | mem), xmm, xmm) |
          proof_tools.OpsProd(mnemonic, (ymm | mem), ymm, ymm))

def XmmOrMemory3operand(mnemonic_name, bitness):
  """Set of 3 operand xmm/memory ops (memory is a possible source operand).

  e.g.  instr xmm1/mem, xmm2, xmm3

  Args:
    mnemonic_name: the mnemonic we are producing the pattern for.
    bitness: the bitness of the architecture (32 or 64)
  Returns:
    All possible disassembly sequences that follow the pattern.
  """
  xmm = proof_tools.AllXMMOperands(bitness)
  mem = proof_tools.AllMemoryOperands(bitness)
  mnemonic = proof_tools.MnemonicOp(mnemonic_name)

  return (proof_tools.OpsProd(mnemonic, (xmm | mem), xmm, xmm))
