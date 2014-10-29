# Copyright (c) 2014 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Proof for verifying that 16bit xadd is added to the 32 bit DFA."""

import proof_tools


def Validate(trie_diffs, bitness):
  """Validate the trie_diffs adds 16 bit xadd to 32bit mode."""

  # No instructions should be removed for 32/64 bit DFAs.
  # No instructions should be added for 64 bit DFA because it already
  # contains 16 bit xadd instruction.
  # in 32-bit mode, 16 bit xadd instruction should be added.
  # The version with lock is only allowed with the second operand being
  # memory. The version without the lock is allowed with second
  # operand being either reg/memory.
  #  lock xadd reg16, mem
  #  xadd reg16, mem/reg16
  if bitness == 32:
    xadd = proof_tools.MnemonicOp('xadd')
    reg16 = proof_tools.GprOperands(bitness=bitness, operand_size=16)
    mem = proof_tools.AllMemoryOperands(bitness=bitness)
    lock = proof_tools.LockPrefix()
    expected_adds = (proof_tools.OpsProd(lock, xadd, reg16, mem) |
                     proof_tools.OpsProd(xadd, reg16, reg16 | mem))
  else:
    expected_adds = set()

  proof_tools.AssertDiffSetEquals(trie_diffs,
                                  expected_adds=expected_adds,
                                  expected_removes=set())


if __name__ == '__main__':
  proof_tools.RunProof(proof_tools.ParseStandardOpts(), Validate)
