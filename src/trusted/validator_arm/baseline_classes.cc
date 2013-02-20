/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator_arm/baseline_classes.h"

#include <assert.h>
#include <string.h>

// Implementations of instruction classes, for those not completely defined in
// in the header.

namespace nacl_arm_dec {

// UncondDecoder
SafetyLevel UncondDecoder::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList UncondDecoder::defs(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

// CondDecoder
SafetyLevel CondDecoder::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList CondDecoder::defs(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

// CondAdvSIMDOp
SafetyLevel CondAdvSIMDOp::safety(Instruction i) const {
  return (cond.value(i) == Instruction::AL)
      ? MAY_BE_SAFE
      : DEPRECATED;
}

// VcvtPtAndFixedPoint_FloatingPoint
SafetyLevel VcvtPtAndFixedPoint_FloatingPoint::safety(Instruction i) const {
  int32_t size = (sx.value(i) == 0 ? 16 : 32);
  int32_t frac_bits =
    size - static_cast<int>((imm4.value(i) << 1) | i_bit.value(i));
  if (frac_bits < 0) return UNPREDICTABLE;
  return CondVfpOp::safety(i);
}

// MoveImmediate12ToApsr
SafetyLevel MoveImmediate12ToApsr::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList MoveImmediate12ToApsr::defs(Instruction i) const {
  RegisterList regs;
  if (UpdatesConditions(i)) {
    regs.Add(Register::Conditions());
  }
  return regs;
}

// Immediate16Use
SafetyLevel Immediate16Use::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList Immediate16Use::defs(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

// BranchImmediate24
SafetyLevel BranchImmediate24::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList BranchImmediate24::defs(Instruction i) const {
  return RegisterList(Register::Pc()).
      Add(link_flag.IsDefined(i) ? Register::Lr() : Register::None());
}

RegisterList BranchImmediate24::uses(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList(Register::Pc());
}

bool BranchImmediate24::is_relative_branch(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return true;
}

int32_t BranchImmediate24::branch_target_offset(Instruction i) const {
  return imm24.relative_address(i);
}

// BreakPointAndConstantPoolHead
SafetyLevel BreakPointAndConstantPoolHead::safety(Instruction i) const {
  if (i.GetCondition() != Instruction::AL)
    return UNPREDICTABLE;
  // Restrict BKPT's encoding to values we've chosen as safe.
  if (IsBreakPointAndConstantPoolHead(i))
    return MAY_BE_SAFE;
  return FORBIDDEN_OPERANDS;
}

bool BreakPointAndConstantPoolHead::
is_literal_pool_head(Instruction i) const {
  return i.Bits(31, 0) == kLiteralPoolHead;
}

// BranchToRegister
SafetyLevel BranchToRegister::safety(Instruction i) const {
  // Extra NaCl constraint: can't branch to PC. This would branch to 8 bytes
  // after the current instruction. This instruction should be in an instruction
  // pair, the mask should therefore be to PC and fail checking, but there's
  // little harm in checking.
  if (m.reg(i).Equals(Register::Pc())) return FORBIDDEN_OPERANDS;

  // Redundant with the above, but this is actually UNPREDICTABLE. Expect DCE.
  if (link_register.IsUpdated(i) && m.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  return MAY_BE_SAFE;
}

RegisterList BranchToRegister::defs(Instruction i) const {
  return RegisterList(Register::Pc()).
      Add(link_register.IsUpdated(i) ? Register::Lr() : Register::None());
}

RegisterList BranchToRegister::uses(Instruction i) const {
  return RegisterList(m.reg(i));
}

Register BranchToRegister::branch_target_register(Instruction i) const {
  return m.reg(i);
}

// Unary1RegisterImmediateOp12
SafetyLevel Unary1RegisterImmediateOp12::safety(Instruction i) const {
  if (d.reg(i).Equals(Register::Pc())) {
    if (conditions.is_updated(i)) return DECODER_ERROR;
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList Unary1RegisterImmediateOp12::defs(Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

// Unary1RegisterImmediateOp12DynCodeReplace
Instruction Unary1RegisterImmediateOp12DynCodeReplace::
dynamic_code_replacement_sentinel(Instruction i) const {
  if (!RegisterList::DynCodeReplaceFrozenRegs().Contains(d.reg(i))) {
    imm12.set_value(&i, 0);
  }
  return i;
}

// Unary1RegisterImmediateOp
SafetyLevel Unary1RegisterImmediateOp::safety(Instruction i) const {
  if (d.reg(i).Equals(Register::Pc())) return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

// Unary1RegisterImmediateOpDynCodeReplace
Instruction Unary1RegisterImmediateOpDynCodeReplace::
dynamic_code_replacement_sentinel(Instruction i) const {
  if (!RegisterList::DynCodeReplaceFrozenRegs().Contains(d.reg(i))) {
    imm4.set_value(&i, 0);
    imm12.set_value(&i, 0);
  }
  return i;
}

// Unary1RegisterImmediateOpPc
SafetyLevel Unary1RegisterImmediateOpPc::safety(Instruction i) const {
  if (d.reg(i).Equals(Register::Pc())) return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

RegisterList Unary1RegisterImmediateOpPc::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

RegisterList Unary1RegisterImmediateOpPc::uses(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList(Register::Pc());
}

// Unary1RegisterBitRangeMsbGeLsb
SafetyLevel Unary1RegisterBitRangeMsbGeLsb::safety(Instruction i) const {
  if (d.reg(i).Equals(Register::Pc()) ||
      msb.value(i) < lsb.value(i))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Unary1RegisterBitRangeMsbGeLsb::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

RegisterList Unary1RegisterBitRangeMsbGeLsb::uses(Instruction i) const {
  return RegisterList(d.reg(i));
}

// Binary2RegisterBitRangeMsbGeLsb
SafetyLevel Binary2RegisterBitRangeMsbGeLsb::safety(Instruction i) const {
  return (d.reg(i).Equals(Register::Pc()) ||
          (msb.value(i) < lsb.value(i)))
      ? UNPREDICTABLE
      : MAY_BE_SAFE;
}

RegisterList Binary2RegisterBitRangeMsbGeLsb::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

RegisterList Binary2RegisterBitRangeMsbGeLsb::uses(Instruction i) const {
  return RegisterList(n.reg(i)).Add(d.reg(i));
}

// Binary2RegisterBitRangeNotRnIsPcBitfieldExtract
SafetyLevel Binary2RegisterBitRangeNotRnIsPcBitfieldExtract
::safety(Instruction i) const {
  return (RegisterList(d.reg(i)).Add(n.reg(i)).Contains(Register::Pc()) ||
          (lsb.value(i) + widthm1.value(i) > 31))
      ? UNPREDICTABLE
      : MAY_BE_SAFE;
}

RegisterList Binary2RegisterBitRangeNotRnIsPcBitfieldExtract
::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

RegisterList Binary2RegisterBitRangeNotRnIsPcBitfieldExtract
::uses(Instruction i) const {
  return RegisterList(n.reg(i));
}

// Binary2RegisterImmediateOp
SafetyLevel Binary2RegisterImmediateOp::safety(Instruction i) const {
  if (d.reg(i).Equals(Register::Pc())) {
    if (conditions.is_updated(i)) return DECODER_ERROR;
    // NaCl Constraint.
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList Binary2RegisterImmediateOp::defs(Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

RegisterList Binary2RegisterImmediateOp::uses(Instruction i) const {
  return RegisterList(n.reg(i));
}

bool MaskedBinary2RegisterImmediateOp::clears_bits(
    Instruction i, uint32_t mask) const {
  return (imm.get_modified_immediate(i) & mask) == mask;
}

// Binary2RegisterImmediateOpDynCodeReplace
Instruction Binary2RegisterImmediateOpDynCodeReplace::
dynamic_code_replacement_sentinel(Instruction i) const {
  if (!RegisterList::DynCodeReplaceFrozenRegs().Contains(d.reg(i))) {
    imm.set_value(&i, 0);
  }
  return i;
}

// Binary2RegisterImmediateOpAddSub
SafetyLevel Binary2RegisterImmediateOpAddSub::safety(Instruction i) const {
  if (d.reg(i).Equals(Register::Pc())) return DECODER_ERROR;  // S=x
  return MAY_BE_SAFE;
}

// BinaryRegisterImmediateTest::
SafetyLevel BinaryRegisterImmediateTest::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList BinaryRegisterImmediateTest::defs(Instruction i) const {
  return RegisterList(conditions.conds_if_updated(i));
}

RegisterList BinaryRegisterImmediateTest::uses(Instruction i) const {
  return RegisterList(n.reg(i));
}

// MaskedBinaryRegisterImmediateTest
bool MaskedBinaryRegisterImmediateTest::sets_Z_if_bits_clear(
    Instruction i, Register r, uint32_t mask) const {
  return n.reg(i).Equals(r) &&
      (imm.get_modified_immediate(i) & mask) == mask &&
      defs(i).Contains(Register::Conditions());
}

// Unary2RegisterOp
SafetyLevel Unary2RegisterOp::safety(Instruction i) const {
  if (d.reg(i).Equals(Register::Pc()) && conditions.is_updated(i))
    return DECODER_ERROR;
  // NaCl Constraint.
  if (d.reg(i).Equals(Register::Pc())) return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

RegisterList Unary2RegisterOp::defs(Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

RegisterList Unary2RegisterOp::uses(Instruction i) const {
  return RegisterList(m.reg(i));
}

// Unary2RegsiterOpNotRmIsPc
SafetyLevel Unary2RegisterOpNotRmIsPc::safety(Instruction i) const {
  SafetyLevel level = Unary2RegisterOp::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (m.reg(i).Equals(Register::Pc()))
    return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

// Unary2RegisterShiftedOpImmNotZero
SafetyLevel Unary2RegisterShiftedOpImmNotZero::
safety(Instruction i) const {
  if (imm5.value(i) == 0) return DECODER_ERROR;
  return Unary2RegisterShiftedOp::safety(i);
}

// Binary3RegisterShiftedOp
RegisterList Binary3RegisterShiftedOp::uses(Instruction i) const {
  return RegisterList().Add(n.reg(i)).Add(m.reg(i));
}

// Binary3RegisterOp
SafetyLevel Binary3RegisterOp::safety(Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(d.reg(i)).Add(m.reg(i)).Add(n.reg(i)).
      Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Binary3RegisterOp::defs(Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

RegisterList Binary3RegisterOp::uses(const Instruction i) const {
  return RegisterList(n.reg(i)).Add(m.reg(i));
}

// Binary3RegisterOpAltA
SafetyLevel Binary3RegisterOpAltA::safety(Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(d.reg(i)).Add(m.reg(i)).Add(n.reg(i)).
      Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  // TODO(karl): This doesn't apply to all uses in rows in armv7.table.
  // However, it doesn't really matter since we only accept version 7.
  if ((ArchVersion() < 6) && m.reg(i).Equals(n.reg(i))) return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Binary3RegisterOpAltA::defs(Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

RegisterList Binary3RegisterOpAltA::uses(Instruction i) const {
  return RegisterList(m.reg(i)).Add(n.reg(i));
}

// Binary3RegisterOpAltANoCondsUpdate
RegisterList Binary3RegisterOpAltANoCondsUpdate::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

// Binary3RegisterOpAltBNoCondUpdates
SafetyLevel Binary3RegisterOpAltBNoCondUpdates::safety(Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(d.reg(i)).Add(m.reg(i)).Add(n.reg(i)).
      Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Binary3RegisterOpAltBNoCondUpdates::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

RegisterList Binary3RegisterOpAltBNoCondUpdates::uses(Instruction i) const {
  return RegisterList(n.reg(i)).Add(m.reg(i));
}

// Binary4RegisterDualOp
SafetyLevel Binary4RegisterDualOp::safety(Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(d.reg(i)).Add(m.reg(i)).Add(n.reg(i)).Add(a.reg(i)).
      Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Binary4RegisterDualOp::defs(Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

RegisterList Binary4RegisterDualOp::uses(Instruction i) const {
  return RegisterList(n.reg(i)).Add(m.reg(i)).Add(a.reg(i));
}

// Binary4RegisterDualOpNoCondsUpdate
RegisterList Binary4RegisterDualOpNoCondsUpdate::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

// Binary4RegisterDualOpLtV6RdNotRn
SafetyLevel Binary4RegisterDualOpLtV6RdNotRn::safety(Instruction i) const {
  SafetyLevel level = Binary4RegisterDualOp::safety(i);
  if (level != MAY_BE_SAFE) return level;

  if ((ArchVersion() < 6) && d.reg(i).Equals(n.reg(i))) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

// Binary4RegisterDualResult
SafetyLevel Binary4RegisterDualResult::safety(Instruction i) const {
  // Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE
  if (RegisterList(d_lo.reg(i)).Add(d_hi.reg(i)).Add(n.reg(i)).Add(m.reg(i)).
      Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  // RdHi == RdLo => UNPREDICTABLE
  if (d_hi.reg(i).Equals(d_lo.reg(i))) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Binary4RegisterDualResult::defs(Instruction i) const {
  return RegisterList(d_hi.reg(i)).Add(d_lo.reg(i)).
      Add(conditions.conds_if_updated(i));
}

RegisterList Binary4RegisterDualResult::uses(Instruction i) const {
  return RegisterList(d_lo.reg(i)).Add(d_hi.reg(i)).Add(n.reg(i)).Add(m.reg(i));
}

// Binary4RegisterDualResultNoCondsUpdate
RegisterList Binary4RegisterDualResultNoCondsUpdate::defs(Instruction i) const {
  return RegisterList(d_hi.reg(i)).Add(d_lo.reg(i));
}

// Binary4RegisterDualResultUsesRnRm
SafetyLevel Binary4RegisterDualResultUsesRnRm::safety(Instruction i) const {
  // Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE
  if (RegisterList(d_lo.reg(i)).Add(d_hi.reg(i)).Add(n.reg(i)).Add(m.reg(i)).
      Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  // RdHi == RdLo => UNPREDICTABLE
  if (d_hi.reg(i).Equals(d_lo.reg(i))) {
    return UNPREDICTABLE;
  }

  // (ArchVersion() < 6 & (RdHi == Rn | RdLo == Rn)) => UNPREDICTABLE
  if ((ArchVersion() < 6) &&
      (RegisterList(d_hi.reg(i)).Add(d_lo.reg(i)).Contains(n.reg(i))))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

// Binary4RegisterDualResultLtV6RdHiLoNotRn
SafetyLevel Binary4RegisterDualResultLtV6RdHiLoNotRn::
safety(Instruction i) const {
  // Pc in {RdLo, RdHi, Rn, Rm} => UNPREDICTABLE
  if (RegisterList(d_lo.reg(i)).Add(d_hi.reg(i)).Add(n.reg(i)).Add(m.reg(i)).
      Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  // RdHi == RdLo => UNPREDICTABLE
  if (d_hi.reg(i).Equals(d_lo.reg(i))) {
    return UNPREDICTABLE;
  }

  // (ArchVersion() < 6 & (RdHi == Rn | RdLo == Rn)) => UNPREDICTABLE
  if ((ArchVersion() < 6) &&
      (RegisterList(d_hi.reg(i)).Add(d_lo.reg(i)).Contains(n.reg(i))))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Binary4RegisterDualResultUsesRnRm::uses(Instruction i) const {
  return RegisterList(n.reg(i)).Add(m.reg(i));
}

// LoadExclusive2RegisterOp
SafetyLevel LoadExclusive2RegisterOp::safety(Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(n.reg(i)).Add(t.reg(i)).Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList LoadExclusive2RegisterOp::defs(Instruction i) const {
  return RegisterList(t.reg(i));
}

RegisterList LoadExclusive2RegisterOp::uses(Instruction i) const {
  return RegisterList(n.reg(i));
}

Register LoadExclusive2RegisterOp::base_address_register(Instruction i) const {
  return n.reg(i);
}

// LoadExclusive2RegisterDoubleOp
SafetyLevel LoadExclusive2RegisterDoubleOp::safety(Instruction i) const {
  SafetyLevel level = LoadExclusive2RegisterOp::safety(i);
  if (MAY_BE_SAFE != level) return level;

  // Arm restrictions for this instruction, based on double width.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (t2.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  return MAY_BE_SAFE;
}

RegisterList LoadExclusive2RegisterDoubleOp::defs(Instruction i) const {
  return RegisterList(t.reg(i)).Add(t2.reg(i));
}

// LoadRegisterImm8Op
SafetyLevel LoadRegisterImm8Op::safety(Instruction i) const {
  if (indexing.IsPostIndexing(i) && writes.IsDefined(i)) return DECODER_ERROR;
  if (indexing.IsDefined(i) == writes.IsDefined(i)) return UNPREDICTABLE;
  if (t.reg(i).Equals(Register::Pc())) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

Register LoadRegisterImm8Op::base_address_register(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return Register::Pc();
}

bool LoadRegisterImm8Op::is_literal_load(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return true;
}

RegisterList LoadRegisterImm8Op::defs(Instruction i) const {
  return RegisterList(t.reg(i));
}

RegisterList LoadRegisterImm8Op::uses(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList(Register::Pc());
}

// LoadRegisterImm8DoubleOp
SafetyLevel LoadRegisterImm8DoubleOp::safety(Instruction i) const {
  if (!t.IsEven(i)) return UNPREDICTABLE;
  if (t2.reg(i).Equals(Register::Pc())) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

RegisterList LoadRegisterImm8DoubleOp::defs(Instruction i) const {
  return RegisterList(t.reg(i)).Add(t2.reg(i));
}

// LoadStore2RegisterImm8Op
SafetyLevel LoadStore2RegisterImm8Op::safety(Instruction i) const {
  // Arm restrictions for this instruction.
  if (t.reg(i).Equals(Register::Pc())) {
    return FORBIDDEN_OPERANDS;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(Register::Pc()) ||
       n.reg(i).Equals(t.reg(i)))) {
    return UNPREDICTABLE;
  }

  // Above implies literal loads can't writeback, the following checks the
  // ARM restriction that literal loads can't have P == W.
  // This should always decode to another instruction, but checking it is good.
  if (n.reg(i).Equals(Register::Pc()) &&
      (indexing.IsDefined(i) == writes.IsDefined(i))) {
    return UNPREDICTABLE;
  }

  // Don't allow modification of PC (NaCl constraint).
  if (defs(i).Contains(Register::Pc())) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


bool LoadStore2RegisterImm8Op::base_address_register_writeback_small_immediate(
    Instruction i) const {
  return HasWriteBack(i);
}

Register LoadStore2RegisterImm8Op::base_address_register(Instruction i) const {
  return n.reg(i);
}

// Load2RegisterImm8Op
RegisterList Load2RegisterImm8Op::defs(Instruction i) const {
  return RegisterList(base_small_writeback_register(i)).Add(t.reg(i));
}

bool Load2RegisterImm8Op::is_literal_load(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return base_address_register(i).Equals(Register::Pc());
}

RegisterList Load2RegisterImm8Op::uses(Instruction i) const {
  return RegisterList(n.reg(i));
}

// Store2RegisterImm8Op
RegisterList Store2RegisterImm8Op::defs(Instruction i) const {
  return RegisterList(base_small_writeback_register(i));
}

RegisterList Store2RegisterImm8Op::uses(Instruction i) const {
  return RegisterList(t.reg(i)).Add(n.reg(i));
}

// LoadStore2RegisterImm8DoubleOp
SafetyLevel LoadStore2RegisterImm8DoubleOp::safety(Instruction i) const {
  SafetyLevel level = LoadStore2RegisterImm8Op::safety(i);
  if (MAY_BE_SAFE != level) return level;

  // Arm restrictions for this instruction, based on double width.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (t2.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) && n.reg(i).Equals(t2.reg(i))) {
    return UNPREDICTABLE;
  }

  return MAY_BE_SAFE;
}

// Load2RegisterImm8DoubleOp
RegisterList Load2RegisterImm8DoubleOp::defs(Instruction i) const {
  return RegisterList(base_small_writeback_register(i)).
      Add(t.reg(i)).Add(t2.reg(i));
}

bool Load2RegisterImm8DoubleOp::is_literal_load(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return base_address_register(i).Equals(Register::Pc());
}

RegisterList Load2RegisterImm8DoubleOp::uses(Instruction i) const {
  return RegisterList(n.reg(i));
}

// Store2RegisterImm8DoubleOp
RegisterList Store2RegisterImm8DoubleOp::defs(Instruction i) const {
  return RegisterList(base_small_writeback_register(i));
}

// PreloadRegisterImm12Op
SafetyLevel PreloadRegisterImm12Op::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList PreloadRegisterImm12Op::defs(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

RegisterList PreloadRegisterImm12Op::uses(Instruction i) const {
  return RegisterList(n.reg(i));
}

Register PreloadRegisterImm12Op::base_address_register(Instruction i) const {
  return n.reg(i);
}

bool PreloadRegisterImm12Op::is_literal_load(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return base_address_register(i).Equals(Register::Pc());
}

// PreloadRegisterPairOp
SafetyLevel PreloadRegisterPairOp::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  // Note: if we go back to allowing some of these preloads then there is
  //       a special case defined in version C of ARM ARM v7 manual.
  //       if m==15 || (n==15 && is_pldw) then UNPREDICTABLE;
  return FORBIDDEN_OPERANDS;
}

RegisterList PreloadRegisterPairOp::defs(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

RegisterList PreloadRegisterPairOp::uses(Instruction i) const {
  return RegisterList().Add(n.reg(i)).Add(m.reg(i));
}

Register PreloadRegisterPairOp::base_address_register(Instruction i) const {
  return n.reg(i);
}

// LoadStore2RegisterImm12Op
SafetyLevel LoadStore2RegisterImm12Op::safety(Instruction i) const {
  // Arm restrictions for this instruction.
  if (HasWriteBack(i) &&
      (n.reg(i).Equals(Register::Pc()) ||
       n.reg(i).Equals(t.reg(i)))) {
    return UNPREDICTABLE;
  }

  if (byte.IsDefined(i) && t.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  // Don't allow modification of PC (NaCl constraint).
  if (defs(i).Contains(Register::Pc())) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}


bool LoadStore2RegisterImm12Op::base_address_register_writeback_small_immediate(
    Instruction i) const {
  return HasWriteBack(i);
}

Register LoadStore2RegisterImm12Op::base_address_register(Instruction i) const {
  return n.reg(i);
}

// Load2RegisterImm12Op
RegisterList Load2RegisterImm12Op::defs(Instruction i) const {
  return RegisterList(base_small_writeback_register(i)).Add(t.reg(i));
}

RegisterList Load2RegisterImm12Op::uses(Instruction i) const {
  return RegisterList(n.reg(i));
}

bool Load2RegisterImm12Op::is_literal_load(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return base_address_register(i).Equals(Register::Pc());
}

// LdrImmediateOp:
// Test if of the form:
//    ldr Rn, [thread_reg]     ; load use thread pointer.
//    ldr Rn, [thread_reg, #4] ; load IRT thread pointer.
bool LdrImmediateOp::is_load_thread_address_pointer(Instruction i) const {
  // Must be based on thread register (r9).
  if (n.number(i) != Register::kTp) return false;

  // The instruction must be a load+offset instruction.
  if (indexing.IsPostIndexing(i) || writes.IsDefined(i) ||
      !direction.IsAdd(i)) return false;

  // The immediate value must be in { 0 , 4 }.
  uint32_t imm_value = imm12.value(i);
  return imm_value == 0 || imm_value == 4;
}

// Store2RegisterImm12Op
RegisterList Store2RegisterImm12Op::defs(Instruction i) const {
  return RegisterList(base_small_writeback_register(i));
}

RegisterList Store2RegisterImm12Op::uses(Instruction i) const {
  return RegisterList(n.reg(i)).Add(t.reg(i));
}

// LoadStoreRegisterList
SafetyLevel LoadStoreRegisterList::safety(Instruction i) const {
  if (n.reg(i).Equals(Register::Pc()) || (register_list.value(i) == 0)) {
    return UNPREDICTABLE;
  }
  return MAY_BE_SAFE;
}

RegisterList LoadStoreRegisterList::defs(Instruction i) const {
  return RegisterList(base_small_writeback_register(i));
}

Register LoadStoreRegisterList::
base_address_register(Instruction i) const {
  return n.reg(i);
}

bool LoadStoreRegisterList::base_address_register_writeback_small_immediate(
    Instruction i) const {
  return wback.IsDefined(i);
}

// LoadRegisterList
SafetyLevel LoadRegisterList::safety(Instruction i) const {
  SafetyLevel level = LoadStoreRegisterList::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (wback.IsDefined(i) && register_list.registers(i).Contains(n.reg(i))) {
    return UNPREDICTABLE;
  }
  if (register_list.registers(i).Contains(Register::Pc())) {
    return FORBIDDEN_OPERANDS;
  }
  return MAY_BE_SAFE;
}

RegisterList LoadRegisterList::defs(Instruction i) const {
  return register_list.registers(i).Union(LoadStoreRegisterList::defs(i));
}

RegisterList LoadRegisterList::uses(Instruction i) const {
  return RegisterList(n.reg(i));
}

// StoreRegisterList
SafetyLevel StoreRegisterList::safety(Instruction i) const {
  SafetyLevel level = LoadStoreRegisterList::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (wback.IsDefined(i) && register_list.registers(i).Contains(n.reg(i)) &&
      ((register_list.value(i) & (0xFFFFFFFFu << n.reg(i).number())) !=
       register_list.value(i))) {
    // Stored value is unknown if there's writeback and the base register isn't
    // at least the first register that is stored.
    return UNPREDICTABLE;
  }
  return MAY_BE_SAFE;
}

RegisterList StoreRegisterList::uses(Instruction i) const {
  return register_list.registers(i).Add(n.reg(i));
}

// LoadStoreVectorOp
Register LoadStoreVectorOp::FirstReg(const Instruction& i) const {
  // Assumes that safety has already been checked, and hence,
  // coproc in { 1010 , 1011 }.
  uint32_t first_reg = vd.number(i);
  if (coproc.value(i) == 11) {  // i.e. 1011
    if (d_bit.value(i) == 1) {
      first_reg += 16;
    }
  } else {
    first_reg = (first_reg << 1) + d_bit.value(i);
  }
  return Register(first_reg);
}

Register LoadStoreVectorOp::
base_address_register(Instruction i) const {
  return n.reg(i);
}

// LoadStoreVectorRegisterList
uint32_t LoadStoreVectorRegisterList::NumRegisters(const Instruction& i) const {
  // Assumes that safety has already been checked, and hence,
  // coproc in { 1010 , 1011 }.
  return (coproc.value(i) == 11)  // i.e. 1011
      ? (imm8.value(i) / 2)
      : imm8.value(i);
}

SafetyLevel LoadStoreVectorRegisterList::safety(Instruction i) const {
  SafetyLevel level = LoadStoreVectorOp::safety(i);
  if (MAY_BE_SAFE != level) return level;

  // ARM constraints.
  if (wback.IsDefined(i) && (indexing.IsDefined(i) == direction.IsAdd(i)))
    return UNDEFINED;

  if (wback.IsDefined(i) && n.reg(i).Equals(Register::Pc()))
    return UNPREDICTABLE;

  uint32_t first_reg = FirstReg(i).number();

  // Register list must have at least one register.
  if (imm8.value(i) == 0)
    return UNPREDICTABLE;

  switch (coproc.value(i)) {
    case 11: {  // A1: 64-bit registers.
      uint32_t regs = imm8.value(i) / 2;
      if (!imm8.IsEven(i))
        return DEPRECATED;
      if ((regs > 16) || ((first_reg + regs) > 32))
        return UNPREDICTABLE;
    } break;
    case 10: {  // A2: 32-bit registers.
      uint32_t regs = imm8.value(i);
      if ((first_reg + regs) > 32)
        return UNPREDICTABLE;
    } break;
    default:  // Bad coprocessor (this check should be redundant).
      return UNPREDICTABLE;
  }

  return MAY_BE_SAFE;
}

RegisterList LoadStoreVectorRegisterList::defs(Instruction i) const {
  return RegisterList(base_small_writeback_register(i));
}

RegisterList LoadStoreVectorRegisterList::uses(Instruction i) const {
  return RegisterList(n.reg(i));
}

bool LoadStoreVectorRegisterList::
  base_address_register_writeback_small_immediate(Instruction i) const {
  return wback.IsDefined(i);
}

// LoadVectorRegisterList
bool LoadVectorRegisterList::is_literal_load(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return base_address_register(i).Equals(Register::Pc());
}

// StoreVectorRegisterList
SafetyLevel StoreVectorRegisterList::safety(Instruction i) const {
  SafetyLevel level = LoadStoreVectorRegisterList::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (n.reg(i).Equals(Register::Pc()))
    return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

// LoadStoreVectorRegister
RegisterList LoadStoreVectorRegister::uses(Instruction i) const {
  return RegisterList(n.reg(i));
}

// LoadVectorRegister
bool LoadVectorRegister::is_literal_load(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return base_address_register(i).Equals(Register::Pc());
}

// StoreVectorRegister
SafetyLevel StoreVectorRegister::safety(Instruction i) const {
  SafetyLevel level = LoadStoreVectorRegister::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (n.reg(i).Equals(Register::Pc())) {
    // Note: covers ARM and NaCl constraints:
    //   Rn!=PC
    //   Rn=PC  && CurrentInstSet() != ARM then UNPREDICTABLE.
    return UNPREDICTABLE;
  }

  return MAY_BE_SAFE;
}

// LoadStore3RegisterOp
SafetyLevel LoadStore3RegisterOp::safety(Instruction i) const {
  if (indexing.IsPreIndexing(i)) {
    // If pre-indexing is set, the address of the load is computed as the sum
    // of the two register parameters.  We have checked that the first register
    // is within the sandbox, but this would allow adding an arbitrary value
    // to it, so it is not safe. (NaCl constraint).
    return FORBIDDEN;
  }

  // Arm restrictions for this instruction.
  if (RegisterList(t.reg(i)).Add(m.reg(i)).Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(Register::Pc()) ||
       n.reg(i).Equals(t.reg(i)))) {
    return UNPREDICTABLE;
  }

  if ((ArchVersion() < 6) && HasWriteBack(i) && m.reg(i).Equals(n.reg(i)))
    return UNPREDICTABLE;

  // Don't let addressing writeback alter PC (NaCl constraint).
  if (defs(i).Contains(Register::Pc())) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

Register LoadStore3RegisterOp::base_address_register(Instruction i) const {
  return n.reg(i);
}

// Load3RegisterOp
RegisterList Load3RegisterOp::defs(Instruction i) const {
  RegisterList defines(t.reg(i));
  if (HasWriteBack(i)) {
    defines.Add(n.reg(i));
  }
  return defines;
}

// Store3RegisterOp
RegisterList Store3RegisterOp::defs(Instruction i) const {
  RegisterList defines;
  if (HasWriteBack(i)) {
    defines.Add(n.reg(i));
  }
  return defines;
}

// LoadStore3RegisterDoubleOp
SafetyLevel LoadStore3RegisterDoubleOp::safety(Instruction i) const {
  SafetyLevel level = LoadStore3RegisterOp::safety(i);
  if (MAY_BE_SAFE != level) return level;

  // Arm restrictions for this instruction, based on double width.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (RegisterList(t2.reg(i)).Add(m.reg(i)).Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(Register::Pc()) ||
       n.reg(i).Equals(t2.reg(i)))) {
    return UNPREDICTABLE;
  }

  if (is_load_ && (m.reg(i).Equals(t.reg(i)) || m.reg(i).Equals(t2.reg(i)))) {
    return UNPREDICTABLE;
  }

  return MAY_BE_SAFE;
}

// StoreExclusive3RegisterOp
SafetyLevel StoreExclusive3RegisterOp::safety(Instruction i) const {
  // Arm restrictions for this instruction.
  if (RegisterList(d.reg(i)).Add(t.reg(i)).Add(n.reg(i)).
      Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  if (d.reg(i).Equals(n.reg(i)) || d.reg(i).Equals(t.reg(i))) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList StoreExclusive3RegisterOp::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

RegisterList StoreExclusive3RegisterOp::uses(Instruction i) const {
  return RegisterList(n.reg(i)).Add(t.reg(i));
}

Register StoreExclusive3RegisterOp::base_address_register(Instruction i) const {
  return n.reg(i);
}

// StoreExclusive3RegisterDoubleOp
SafetyLevel StoreExclusive3RegisterDoubleOp::safety(Instruction i) const {
  SafetyLevel level = StoreExclusive3RegisterOp::safety(i);
  if (MAY_BE_SAFE != level) return level;

  // Arm restrictions for this instruction, based on double width.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (t2.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  if (d.reg(i).Equals(t2.reg(i))) {
    return UNPREDICTABLE;
  }

  return MAY_BE_SAFE;
}

RegisterList StoreExclusive3RegisterDoubleOp::uses(Instruction i) const {
  return RegisterList(n.reg(i)).Add(t.reg(i)).Add(t2.reg(i));
}

// Load3RegisterDoubleOp
RegisterList Load3RegisterDoubleOp::defs(Instruction i) const {
  RegisterList defines;
  defines.Add(t.reg(i)).Add(t2.reg(i));
  if (HasWriteBack(i)) {
    defines.Add(n.reg(i));
  }
  return defines;
}

RegisterList Load3RegisterDoubleOp::uses(Instruction i) const {
  return RegisterList().Add(n.reg(i)).Add(m.reg(i));
}

// Store3RegisterDoubleOp
RegisterList Store3RegisterDoubleOp::defs(Instruction i) const {
  RegisterList defines;
  if (HasWriteBack(i)) {
    defines.Add(n.reg(i));
  }
  return defines;
}

RegisterList Store3RegisterDoubleOp::uses(Instruction i) const {
  return RegisterList().Add(t.reg(i)).Add(t2.reg(i)).
      Add(n.reg(i)).Add(m.reg(i));
}

// LoadStore3RegisterImm5Op
SafetyLevel LoadStore3RegisterImm5Op::safety(Instruction i) const {
  if (indexing.IsPreIndexing(i)) {
    // If pre-indexing is set, the address of the load is computed as the sum
    // of the two register parameters.  We have checked that the first register
    // is within the sandbox, but this would allow adding an arbitrary value
    // to it, so it is not safe. (NaCl constraint).
    return FORBIDDEN;
  }

  // Arm restrictions for this instruction.
  if (m.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(Register::Pc()) ||
       n.reg(i).Equals(t.reg(i)))) {
    return UNPREDICTABLE;
  }

  if (byte.IsDefined(i) & t.reg(i).Equals(Register::Pc())) {
    return UNPREDICTABLE;
  }

  if ((ArchVersion() < 6) && HasWriteBack(i) && m.reg(i).Equals(n.reg(i))) {
    return UNPREDICTABLE;
  }

  // Don't let addressing writeback alter PC (NaCl constraint).
  if (defs(i).Contains(Register::Pc())) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

Register LoadStore3RegisterImm5Op::base_address_register(Instruction i) const {
  return n.reg(i);
}

// Load3RegisterImm5Op
RegisterList Load3RegisterImm5Op::defs(Instruction i) const {
  RegisterList defines(t.reg(i));
  if (HasWriteBack(i)) {
    defines.Add(n.reg(i));
  }
  return defines;
}

// Store3RegisterImm5Op
RegisterList Store3RegisterImm5Op::defs(Instruction i) const {
  RegisterList defines;
  if (HasWriteBack(i)) {
    defines.Add(n.reg(i));
  }
  return defines;
}

RegisterList Store3RegisterImm5Op::uses(Instruction i) const {
  return RegisterList(m.reg(i)).Add(n.reg(i)).Add(t.reg(i));
}

// Unary2RegisterImmedShiftedOp
SafetyLevel Unary2RegisterImmedShiftedOp::safety(Instruction i) const {
  if (RegisterList(d.reg(i)).Add(m.reg(i)).Contains(Register::Pc()))
    return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

RegisterList Unary2RegisterImmedShiftedOp::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

RegisterList Unary2RegisterImmedShiftedOp::uses(Instruction i) const {
  return RegisterList(m.reg(i));
}

// Unary2RegisterSatImmedShiftedOp
SafetyLevel Unary2RegisterSatImmedShiftedOp::safety(Instruction i) const {
  if (RegisterList(d.reg(i)).Add(n.reg(i)).Contains(Register::Pc()))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Unary2RegisterSatImmedShiftedOp::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

RegisterList Unary2RegisterSatImmedShiftedOp::uses(Instruction i) const {
  return RegisterList(n.reg(i));
}

// Unary3RegisterShiftedOp
SafetyLevel Unary3RegisterShiftedOp::safety(Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(d.reg(i)).Add(s.reg(i)).Add(m.reg(i))
      .Contains(Register::Pc()))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Unary3RegisterShiftedOp::defs(Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

RegisterList Unary3RegisterShiftedOp::uses(const Instruction i) const {
  return RegisterList(m.reg(i)).Add(s.reg(i));
}

// Binary4RegisterShiftedOp
SafetyLevel Binary4RegisterShiftedOp::safety(Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(d.reg(i)).Add(n.reg(i)).Add(s.reg(i)).Add(m.reg(i)).
      Contains(Register::Pc())) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Binary4RegisterShiftedOp::defs(Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

RegisterList Binary4RegisterShiftedOp::uses(const Instruction i) const {
  return RegisterList(n.reg(i)).Add(m.reg(i)).Add(s.reg(i));
}

// Binary2RegisterImmedShiftedTest
SafetyLevel Binary2RegisterImmedShiftedTest::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList Binary2RegisterImmedShiftedTest::defs(Instruction i) const {
  return RegisterList(conditions.conds_if_updated(i));
}

RegisterList Binary2RegisterImmedShiftedTest::uses(Instruction i) const {
  return RegisterList().Add(n.reg(i)).Add(m.reg(i));
}

// Binary3RegisterShiftedTest
SafetyLevel Binary3RegisterShiftedTest::safety(Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(n.reg(i)).Add(s.reg(i)).Add(m.reg(i))
      .Contains(Register::Pc()))
    return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

RegisterList Binary3RegisterShiftedTest::defs(Instruction i) const {
  return RegisterList(conditions.conds_if_updated(i));
}

RegisterList Binary3RegisterShiftedTest::uses(const Instruction i) const {
  return RegisterList(n.reg(i)).Add(m.reg(i)).Add(s.reg(i));
}

// Vector2RegisterMiscellaneous_RG
SafetyLevel Vector2RegisterMiscellaneous_RG::safety(Instruction i) const {
  SafetyLevel level = Vector2RegisterMiscellaneous::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (op.value(i) + size.value(i) >= 3) return UNDEFINED;
  if (q.IsDefined(i) && (!vd.IsEven(i) || !vm.IsEven(i))) return UNDEFINED;
  return MAY_BE_SAFE;
}

// Vector2RegisterMiscellaneous_V8_16_32
SafetyLevel Vector2RegisterMiscellaneous_V8_16_32::safety(Instruction i) const {
  SafetyLevel level = Vector2RegisterMiscellaneous::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (size.value(i) == 3) return UNDEFINED;
  if (q.IsDefined(i) && (!vd.IsEven(i) || !vm.IsEven(i))) return UNDEFINED;
  return MAY_BE_SAFE;
}

// Vector2RegisterMiscellaneous_V8
SafetyLevel Vector2RegisterMiscellaneous_V8::safety(Instruction i) const {
  SafetyLevel level = Vector2RegisterMiscellaneous::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (size.value(i) != 0) return UNDEFINED;
  if (q.IsDefined(i) && (!vd.IsEven(i) || !vm.IsEven(i))) return UNDEFINED;
  return MAY_BE_SAFE;
}

// Vector2RegisterMiscellaneous_F32
SafetyLevel Vector2RegisterMiscellaneous_F32::safety(Instruction i) const {
  SafetyLevel level = Vector2RegisterMiscellaneous::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (q.IsDefined(i) && (!vd.IsEven(i) || !vm.IsEven(i))) return UNDEFINED;
  if (size.value(i) != 2) return UNDEFINED;
  return MAY_BE_SAFE;
}

// Vector2RegisterMiscellaneous_V8S
SafetyLevel Vector2RegisterMiscellaneous_V8S::safety(Instruction i) const {
  SafetyLevel level = Vector2RegisterMiscellaneous::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if ((m.value(i) == d.value(i)) && (vm.reg(i).Equals(vd.reg(i))))
    return UNKNOWN;
  if (size.value(i) != 0) return UNDEFINED;
  if (q.IsDefined(i) && (!vd.IsEven(i) || !vm.IsEven(i))) return UNDEFINED;
  return MAY_BE_SAFE;
}

// Vector2RegisterMiscellaneous_V8_16_32T
SafetyLevel Vector2RegisterMiscellaneous_V8_16_32T::
safety(Instruction i) const {
  SafetyLevel level = Vector2RegisterMiscellaneous::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if ((m.value(i) == d.value(i)) && (vm.reg(i).Equals(vd.reg(i))))
    return UNKNOWN;
  if (size.value(i) == 3) return UNDEFINED;
  if (q.IsDefined(i) && (!vd.IsEven(i) || !vm.IsEven(i))) return UNDEFINED;
  return MAY_BE_SAFE;
}

// Vector2RegisterMiscellaneous_V8_16_32I
SafetyLevel Vector2RegisterMiscellaneous_V8_16_32I::
safety(Instruction i) const {
  SafetyLevel level = Vector2RegisterMiscellaneous::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if ((m.value(i) == d.value(i)) && (vm.reg(i).Equals(vd.reg(i))))
    return UNKNOWN;
  if (size.value(i) == 3) return UNDEFINED;
  if (!q.IsDefined(i) && (size.value(i) == 2)) return UNDEFINED;
  if (q.IsDefined(i) && (!vd.IsEven(i) || !vm.IsEven(i))) return UNDEFINED;
  return MAY_BE_SAFE;
}

// Vector2RegisterMiscellaneous_V16_32_64N
SafetyLevel Vector2RegisterMiscellaneous_V16_32_64N::
safety(Instruction i) const {
  SafetyLevel level = Vector2RegisterMiscellaneous::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (size.value(i) == 3) return UNDEFINED;
  if (!vm.IsEven(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// Vector2RegisterMiscellaneous_I8_16_32L
SafetyLevel Vector2RegisterMiscellaneous_I8_16_32L::
safety(Instruction i) const {
  SafetyLevel level = Vector2RegisterMiscellaneous::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (size.value(i) == 3) return UNDEFINED;
  if (!vd.IsEven(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// Vector2RegisterMiscellaneous_CVT_F2I
SafetyLevel Vector2RegisterMiscellaneous_CVT_F2I::
safety(Instruction i) const {
  SafetyLevel level = Vector2RegisterMiscellaneous::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (q.IsDefined(i) && (!vd.IsEven(i) || !vm.IsEven(i))) return UNDEFINED;
  if (size.value(i) != 2) return UNDEFINED;
  return MAY_BE_SAFE;
}

// Vector2RegisterMiscellaneous_I16_32_64N
SafetyLevel Vector2RegisterMiscellaneous_I16_32_64N::
safety(Instruction i) const {
  if (op.value(i) == 0) return DECODER_ERROR;
  SafetyLevel level = VectorUnary2RegisterOpBase::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (size.value(i) == 3) return UNDEFINED;
  if (!vm.IsEven(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// Vector2RegisterMiscellaneous_CVT_H2S
SafetyLevel Vector2RegisterMiscellaneous_CVT_H2S::
safety(Instruction i) const {
  SafetyLevel level = VectorUnary2RegisterOpBase::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (size.value(i) != 1) return UNDEFINED;
  if (op.IsDefined(i) && !vd.IsEven(i)) return UNDEFINED;
  if (!op.IsDefined(i) && !vm.IsEven(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorUnary2RegisterDup
SafetyLevel VectorUnary2RegisterDup::safety(Instruction i) const {
  SafetyLevel level = VectorUnary2RegisterOpBase::safety(i);
  if (MAY_BE_SAFE != level) return level;

  if (q.IsDefined(i) && !vd.IsEven(i))
    return UNDEFINED;

  // Check that imm4 != 0000 (undefined), or that
  // it is in { xxx1, xx10, x100 }.
  uint32_t imm = imm4.value(i);
  if (imm == 0)
    return UNDEFINED;
  if (!(((imm & 0x1) == 0x1) ||
        ((imm & 0x3) == 0x2) ||
        ((imm & 0x7) == 0x4)))
    return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

// VectorBinary2RegisterShiftAmount_I
SafetyLevel VectorBinary2RegisterShiftAmount_I::safety(Instruction i) const {
  // L:imm6=0000xxx => DECODER_ERROR
  if ((l.value(i) == 0) && ((imm6.value(i) & 0x38) == 0)) return DECODER_ERROR;
  SafetyLevel level = VectorBinary2RegisterShiftAmount::safety(i);
  if (level != MAY_BE_SAFE) return level;
  // Q=1 & (Vd(0)=1 | Vm(1)=1) => UNDEFINED
  if (q.IsDefined(i) && (!vd.IsEven(i) || !vm.IsEven(i))) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary2RegisterShiftAmount_ILS
SafetyLevel VectorBinary2RegisterShiftAmount_ILS::safety(Instruction i) const {
  SafetyLevel level = VectorBinary2RegisterShiftAmount_I::safety(i);
  if (level != MAY_BE_SAFE) return level;
  // U=0 & op=0 => UNDEFINED
  if (!u.IsDefined(i) && !op.IsDefined(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary2RegisterShiftAmount_N16_32_64R
SafetyLevel VectorBinary2RegisterShiftAmount_N16_32_64R::
safety(Instruction i) const {
  // L:imm6=0000xxx => DECODER_ERROR
  if ((l.value(i) == 0) && ((imm6.value(i) & 0x38) == 0)) return DECODER_ERROR;
  SafetyLevel level = VectorBinary2RegisterShiftAmount::safety(i);
  if (level != MAY_BE_SAFE) return level;
  // Vm(0)=1 => UNDEFINED;
  if (!vm.IsEven(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary2RegisterShiftAmount_N16_32_64RS
SafetyLevel VectorBinary2RegisterShiftAmount_N16_32_64RS::
safety(Instruction i) const {
  // U=0 & op=0 => DECODER_ERROR;
  if (!u.IsDefined(i) && !op.IsDefined(i)) return DECODER_ERROR;
  return VectorBinary2RegisterShiftAmount_N16_32_64R::safety(i);
}

// VectorBinary2RegisterShiftAmount_E8_16_32L
SafetyLevel VectorBinary2RegisterShiftAmount_E8_16_32L::
safety(Instruction i) const {
  // imm6=000xxx => DECODER_ERROR
  if ((imm6.value(i) & 0x38) == 0) return DECODER_ERROR;
  SafetyLevel level = VectorBinary2RegisterShiftAmount::safety(i);
  if (level != MAY_BE_SAFE) return level;
  // Vd(0)=1 => UNDEFINED;
  if (!vd.IsEven(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary2RegisterShiftAmount_CVT
SafetyLevel VectorBinary2RegisterShiftAmount_CVT::safety(Instruction i) const {
  // imm6=000xxx => DECODER_ERROR
  if ((imm6.value(i) & 0x38) == 0) return DECODER_ERROR;
  SafetyLevel level = VectorBinary2RegisterShiftAmount::safety(i);
  if (level != MAY_BE_SAFE) return level;
  // imm6=0xxxxx => UNDEFINED
  if ((imm6.value(i) & 0x20) == 0) return UNDEFINED;
  // Q=1 & (Vd(0)=1 | Vm(1)=1) => UNDEFINED
  if (q.IsDefined(i) && (!vd.IsEven(i) || !vm.IsEven(i))) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary3RegisterSameLengthDQ
SafetyLevel VectorBinary3RegisterSameLengthDQ::safety(Instruction i) const {
  if (q.IsDefined(i) && (!vd.IsEven(i) || !vn.IsEven(i) || !vm.IsEven(i))) {
    return UNDEFINED;
  }
  return MAY_BE_SAFE;
}

// VectorBinary3RegisterSameLengthDQI8_16_32
SafetyLevel VectorBinary3RegisterSameLengthDQI8_16_32::
safety(Instruction i) const {
  SafetyLevel level = VectorBinary3RegisterSameLengthDQ::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (size.value(i) == 3) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary3RegisterSameLengthDQI8P
SafetyLevel VectorBinary3RegisterSameLengthDQI8P::
safety(Instruction i) const {
  SafetyLevel level = VectorBinary3RegisterSameLengthDQ::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (size.value(i) != 0) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary3RegisterSameLengthDQI16_32
SafetyLevel VectorBinary3RegisterSameLengthDQI16_32::
safety(Instruction i) const {
  SafetyLevel level = VectorBinary3RegisterSameLengthDQ::safety(i);
  if (MAY_BE_SAFE != level) return level;
  switch (size.value(i)) {
    case 1:
    case 2:
      return MAY_BE_SAFE;
    default:
      return UNDEFINED;
  }
}

// VectorBinary3RegisterSameLength32P
SafetyLevel VectorBinary3RegisterSameLength32P::safety(Instruction i) const {
  SafetyLevel level = VectorBinary3RegisterSameLength::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (q.IsDefined(i)) return UNDEFINED;
  switch (size.value(i)) {
    case 0:
    case 2:
      return MAY_BE_SAFE;
    default:
      return UNDEFINED;
  }
}

// VectorBinary3RegisterSameLength32_DQ
SafetyLevel VectorBinary3RegisterSameLength32_DQ::safety(Instruction i) const {
  SafetyLevel level = VectorBinary3RegisterSameLength::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (q.IsDefined(i) && (!vd.IsEven(i) || !vn.IsEven(i) || !vm.IsEven(i))) {
    return UNDEFINED;
  }
  switch (size.value(i)) {
    case 0:
    case 2:
      return MAY_BE_SAFE;
    default:
      return UNDEFINED;
  }
}

// VectorBinary3RegisterSameLengthDI
SafetyLevel VectorBinary3RegisterSameLengthDI::safety(Instruction i) const {
  if (size.value(i) == 3) return UNDEFINED;
  if (q.IsDefined(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}
// VectorBinary3RegisterDifferentLength_I8_16_32
SafetyLevel VectorBinary3RegisterDifferentLength_I8_16_32::
safety(Instruction i) const {
  if (size.value(i) == 3) return DECODER_ERROR;
  SafetyLevel level = VectorBinary3RegisterDifferentLength::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (!vd.IsEven(i)) return UNDEFINED;
  if ((op.IsDefined(i)) && !vn.IsEven(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary3RegisterDifferentLength_I16_32_64
SafetyLevel VectorBinary3RegisterDifferentLength_I16_32_64::
safety(Instruction i) const {
  if (size.value(i) == 3) return DECODER_ERROR;
  SafetyLevel level = VectorBinary3RegisterDifferentLength::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (!vn.IsEven(i)) return UNDEFINED;
  if (!vm.IsEven(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary3RegisterDifferentLength_I8_16_32L
SafetyLevel VectorBinary3RegisterDifferentLength_I8_16_32L::
safety(Instruction i) const {
  if (size.value(i) == 3) return DECODER_ERROR;
  SafetyLevel level = VectorBinary3RegisterDifferentLength::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (!vd.IsEven(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary3RegisterDifferentLength_I16_32L
SafetyLevel VectorBinary3RegisterDifferentLength_I16_32L::
safety(Instruction i) const {
  if (size.value(i) == 3) return DECODER_ERROR;
  SafetyLevel level = VectorBinary3RegisterDifferentLength::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (size.value(i) == 0) return UNDEFINED;
  if (!vd.IsEven(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary3RegisterDifferentLength_P8
SafetyLevel VectorBinary3RegisterDifferentLength_P8::
safety(Instruction i) const {
  if (size.value(i) == 3) return DECODER_ERROR;
  SafetyLevel level = VectorBinary3RegisterDifferentLength::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (u.IsDefined(i)) return UNDEFINED;
  if (size.value(i) != 0) return UNDEFINED;
  if (!vd.IsEven(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary2RegisterScalar_I16_32
SafetyLevel VectorBinary2RegisterScalar_I16_32::safety(Instruction i) const {
  if (size.value(i) == 3) return DECODER_ERROR;
  SafetyLevel level = VectorBinary2RegisterScalar::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (size.value(i) == 0) return UNDEFINED;
  if (q.IsDefined(i) && (!vd.IsEven(i) || !vn.IsEven(i))) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary2RegisterScalar_I16_32L
SafetyLevel VectorBinary2RegisterScalar_I16_32L::safety(Instruction i) const {
  if (size.value(i) == 3) return DECODER_ERROR;
  SafetyLevel level = VectorBinary2RegisterScalar::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (size.value(i) == 0 || !vd.IsEven(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary2RegisterScalar_F32
SafetyLevel VectorBinary2RegisterScalar_F32::safety(Instruction i) const {
  uint32_t size_i = size.value(i);
  if (size_i == 3) return DECODER_ERROR;
  SafetyLevel level = VectorBinary2RegisterScalar::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if ((size_i == 0) || (size_i == 1)) return UNDEFINED;
  if (q.IsDefined(i) && (!vd.IsEven(i) || !vn.IsEven(i))) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary3RegisterImmOp
SafetyLevel VectorBinary3RegisterImmOp::safety(Instruction i) const {
  SafetyLevel level = VectorBinary3RegisterOpBase::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (q.IsDefined(i)) {
    if (!vd.IsEven(i) || !vn.IsEven(i) || !vm.IsEven(i))
      return UNDEFINED;
  } else if (imm.value(i) > 0x7) {
    return UNDEFINED;
  }
  return MAY_BE_SAFE;
}

// Vector1RegisterImmediate_MOV
SafetyLevel Vector1RegisterImmediate_MOV::safety(Instruction i) const {
  if (!op.IsDefined(i) &&
      ((cmode.value(i) & 0x1) == 1) &&
      ((cmode.value(i) & 0xC) != 0xC))
    return DECODER_ERROR;
  if (op.IsDefined(i) && (cmode.value(i) !=  14)) return DECODER_ERROR;
  SafetyLevel level = Vector1RegisterImmediate::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (q.IsDefined(i) && !vd.IsEven(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// Vector1RegisterImmediate_BIT
SafetyLevel Vector1RegisterImmediate_BIT::safety(Instruction i) const {
  if (((cmode.value(i) & 0x1) == 0) || ((cmode.value(i) & 0xC) == 0xC))
    return DECODER_ERROR;
  SafetyLevel level = Vector1RegisterImmediate::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (q.IsDefined(i) && !vd.IsEven(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// Vector1RegisterImmediate_MVN
SafetyLevel Vector1RegisterImmediate_MVN::safety(Instruction i) const {
  if ((((cmode.value(i) & 0x1) == 1) && (cmode.value(i) & 0xC) != 0xC) ||
      ((cmode.value(i) & 0xE) == 0xE))
    return DECODER_ERROR;
  SafetyLevel level = Vector1RegisterImmediate::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (q.IsDefined(i) && !vd.IsEven(i)) return UNDEFINED;
  return MAY_BE_SAFE;
}

// VectorBinary3RegisterLookupOp
SafetyLevel VectorBinary3RegisterLookupOp::safety(Instruction i) const {
  SafetyLevel level = VectorBinary3RegisterOpBase::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (n_reg_index(i) + length(i) > 32)
    return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

// VfpUsesRegOp
SafetyLevel VfpUsesRegOp::safety(Instruction i) const {
  SafetyLevel level = CondVfpOp::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (t.reg(i).Equals(Register::Pc()))
    return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

RegisterList VfpUsesRegOp::defs(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

RegisterList VfpUsesRegOp::uses(Instruction i) const {
  return RegisterList(t.reg(i));
}

// VfpMrsOp
RegisterList VfpMrsOp::defs(Instruction i) const {
  return RegisterList(t.reg(i).Equals(Register::Pc())
                      ? Register::Conditions() : t.reg(i));
}

// MoveVfpRegisterOp
SafetyLevel MoveVfpRegisterOp::safety(Instruction i) const {
  SafetyLevel level = CondVfpOp::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (t.reg(i).Equals(Register::Pc()))
    return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

RegisterList MoveVfpRegisterOp::defs(Instruction i) const {
  RegisterList defs;
  if (to_arm_reg.IsDefined(i)) defs.Add(t.reg(i));
  return defs;
}

RegisterList MoveVfpRegisterOp::uses(Instruction i) const {
  RegisterList uses;
  if (!to_arm_reg.IsDefined(i)) uses.Add(t.reg(i));
  return uses;
}

// MoveVfpRegisterOpWithTypeSel
SafetyLevel MoveVfpRegisterOpWithTypeSel::safety(Instruction i) const {
  SafetyLevel level = MoveVfpRegisterOp::safety(i);
  if (MAY_BE_SAFE != level) return level;

  // Compute type_sel = opc1(23:21):opc2(6:5).
  uint32_t type_sel = (opc1.value(i) << 2) | opc2.value(i);

  // If type_sel in { '10x00' , 'x0x10' }, the instruction is undefined.
  if (((type_sel & 0x1b) == 0x10) ||  /* type_sel == '10x00 */
      ((type_sel & 0xb) == 0x2))      /* type_sel == 'x0x10 */
    return UNDEFINED;

  return MAY_BE_SAFE;
}

// MoveDoubleVfpRegisterOp
SafetyLevel MoveDoubleVfpRegisterOp::safety(Instruction i) const {
  SafetyLevel level = MoveVfpRegisterOp::safety(i);
  if (MAY_BE_SAFE != level) return level;
  if (t2.reg(i).Equals(Register::Pc()) ||
      (IsSinglePrecision(i) &&
       (((vm.reg(i).number() << 1) | m.value(i)) == 31)) ||
      (to_arm_reg.IsDefined(i) && t.reg(i).Equals(t2.reg(i))))
    return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

RegisterList MoveDoubleVfpRegisterOp::defs(Instruction i) const {
  RegisterList defs;
  if (to_arm_reg.IsDefined(i)) defs.Add(t.reg(i)).Add(t2.reg(i));
  return defs;
}

RegisterList MoveDoubleVfpRegisterOp::uses(Instruction i) const {
  RegisterList uses;
  if (!to_arm_reg.IsDefined(i)) uses.Add(t.reg(i)).Add(t2.reg(i));
  return uses;
}

// DuplicateToAdvSIMDRegisters
SafetyLevel DuplicateToAdvSIMDRegisters::safety(Instruction i) const {
  SafetyLevel level = CondAdvSIMDOp::safety(i);
  if (MAY_BE_SAFE != level) return level;

  if (is_two_regs.IsDefined(i) && !vd.IsEven(i))
    return UNDEFINED;

  if (be_value(i) == 0x3)
    return UNDEFINED;

  if (t.reg(i).Equals(Register::Pc()))
    return UNPREDICTABLE;

  return MAY_BE_SAFE;
}

RegisterList DuplicateToAdvSIMDRegisters::uses(Instruction i) const {
  return RegisterList(t.reg(i));
}

// VectorLoadStoreMultiple
RegisterList VectorLoadStoreMultiple::defs(Instruction i) const {
  RegisterList regs;
  if (is_wback(i)) {
    regs.Add(rn.reg(i));
  }
  return regs;
}

bool VectorLoadStoreMultiple::base_address_register_writeback_small_immediate(
    Instruction i) const {
  return is_wback(i) && !is_register_index(i);
}

Register VectorLoadStoreMultiple::base_address_register(Instruction i) const {
  return rn.reg(i);
}

// VectorLoadStoreMultple1
SafetyLevel VectorLoadStoreMultiple1::safety(Instruction i) const {
  uint32_t regs = 0;
  switch (type.value(i)) {
    case 0x2:  // 0010
      regs = 4;
      break;
    case 0x6:  // 0110
      if ((align.value(i) & 0x2) == 0x2) return UNDEFINED;
      regs = 3;
      break;
    case 0x7:  // 0111
      if ((align.value(i) & 0x2) == 0x2) return UNDEFINED;
      regs = 1;
      break;
    case 0xa:  // 1010
      if (align.value(i) == 3) return UNDEFINED;
      regs = 2;
      break;
    default:
      return DECODER_ERROR;
  }
  if (rn.reg(i).Equals(Register::Pc())) return UNPREDICTABLE;
  if (d_reg_index(i) + regs > 32) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

// VectorLoadStoreMultiple2
SafetyLevel VectorLoadStoreMultiple2::safety(Instruction i) const {
  if (size.value(i) == 3) return UNDEFINED;
  uint32_t regs = 2;
  uint32_t inc = 2;
  switch (type.value(i)) {
    case 0x3:  // 0011
      // Via initialization: regs = 2;
      // Via initialization: inc = 2;
      break;
    case 0x8:  // 1000
      if (align.value(i) == 3) return UNDEFINED;
      regs = 1;
      inc = 1;
      break;
    case 0x9:  // 1001
      if (align.value(i) == 3) return UNDEFINED;
      regs = 1;
      // Via intialization: inc = 2;
      break;
    default:
      return DECODER_ERROR;
  }
  if (rn.reg(i).Equals(Register::Pc())) return UNPREDICTABLE;
  if (d_reg_index(i) + inc + regs > 32) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

// VectorLoadStoreMultiple3
SafetyLevel VectorLoadStoreMultiple3::safety(Instruction i) const {
  if (size.value(i) == 3) return UNDEFINED;
  if ((align.value(i) & 0x2) == 0x2) return UNDEFINED;
  uint32_t inc = 2;
  switch (type.value(i)) {
    case 0x4:  // 0100
      inc = 1;
      break;
    case 0x5:  // 0101
      break;
    default:
      return DECODER_ERROR;
  }
  if (rn.reg(i).Equals(Register::Pc())) return UNPREDICTABLE;
  if (d_reg_index(i) + 2 * inc > 31) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

// VectorLoadStoreMultiple4
SafetyLevel VectorLoadStoreMultiple4::safety(Instruction i) const {
  if (size.value(i) == 3) return UNDEFINED;
  uint32_t inc = 2;
  switch (type.value(i)) {
    case 0x0:  // 0000
      inc = 1;
      break;
    case 0x1:  // 0001
      break;
    default:
      return DECODER_ERROR;
  }
  if (rn.reg(i).Equals(Register::Pc())) return UNPREDICTABLE;
  if (d_reg_index(i) + 3 * inc > 31) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

// VectorLoadStoreSingle
RegisterList VectorLoadStoreSingle::defs(Instruction i) const {
  RegisterList regs;
  if (is_wback(i)) {
    regs.Add(rn.reg(i));
  }
  return regs;
}

bool VectorLoadStoreSingle::base_address_register_writeback_small_immediate(
    Instruction i) const {
  return is_wback(i) && !is_register_index(i);
}

Register VectorLoadStoreSingle::base_address_register(Instruction i) const {
  return rn.reg(i);
}

uint32_t VectorLoadStoreSingle::inc(Instruction i) const {
  switch (size.value(i)) {
    case 0x0:  // 00
      return 1;
    case 0x1:  // 01
      if ((index_align.value(i) & 0x2) == 0) return 1;
      return 2;
    case 0x2:  // 10
      if ((index_align.value(i) & 0x4) == 0) return 1;
      return 2;
    default:
      return 0;  // I.e. error.
  }
}

// VectorLoadStoreSingle1
SafetyLevel VectorLoadStoreSingle1::safety(Instruction i) const {
  switch (size.value(i)) {
    case 0x0:  // 00
      if ((index_align.value(i) & 0x1) != 0x0) return UNDEFINED;
      break;
    case 0x1:  // 01
      if ((index_align.value(i) & 0x2) != 0x0) return UNDEFINED;
      break;
    case 0x2:  // 10
      if ((index_align.value(i) & 0x4) != 0x0) return UNDEFINED;
      if (((index_align.value(i) & 0x3) != 0x0) &&
          ((index_align.value(i) & 0x3) != 0x3)) return UNDEFINED;
      break;
    case 0x3:  // 11
      return UNDEFINED;
    default:
      return DECODER_ERROR;
  }
  if (rn.reg(i).Equals(Register::Pc())) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

// VectorLoadStoreSingle2
SafetyLevel VectorLoadStoreSingle2::safety(Instruction i) const {
  switch (size.value(i)) {
    case 0x0:  // 00
      break;
    case 0x1:  // 01
      break;
    case 0x2:  // 10
      if ((index_align.value(i) & 0x2) != 0) return UNDEFINED;
      break;
    case 0x3:  // 11
      return UNDEFINED;
    default:
      return DECODER_ERROR;
  }
  if (rn.reg(i).Equals(Register::Pc())) return UNPREDICTABLE;
  if (d_reg_index(i) + inc(i) > 31) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

// VectorLoadStoreSingle3
SafetyLevel VectorLoadStoreSingle3::safety(Instruction i) const {
  switch (size.value(i)) {
    case 0:  // 00
    case 1:  // 01
      if ((index_align.value(i) & 0x1) != 0) return UNDEFINED;
      break;
    case 2:  // 10
      if ((index_align.value(i) & 0x3) != 0) return UNDEFINED;
      break;
    case 3:  // 11
      return UNDEFINED;
    default:
      return DECODER_ERROR;
  }
  if (rn.reg(i).Equals(Register::Pc())) return UNPREDICTABLE;
  if (d_reg_index(i) + 2 * inc(i) > 31) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

// VectorLoadStoreSingle4
SafetyLevel VectorLoadStoreSingle4::safety(Instruction i) const {
  switch (size.value(i)) {
    case 0:  // 00
    case 1:  // 01
      break;
    case 2:  // 10
      if ((index_align.value(i) & 0x3) == 0x3) return UNDEFINED;
      break;
    case 3:  // 11
      return UNDEFINED;
    default:
      return DECODER_ERROR;
  }
  if (rn.reg(i).Equals(Register::Pc())) return UNPREDICTABLE;
  if (d_reg_index(i) + 3 * inc(i) > 31) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

// VectorLoadSingleAllLanes
RegisterList VectorLoadSingleAllLanes::defs(Instruction i) const {
  RegisterList regs;
  if (is_wback(i)) {
    regs.Add(rn.reg(i));
  }
  return regs;
}

bool VectorLoadSingleAllLanes::base_address_register_writeback_small_immediate(
    Instruction i) const {
  return is_wback(i) && !is_register_index(i);
}

Register VectorLoadSingleAllLanes::base_address_register(Instruction i) const {
  return rn.reg(i);
}

// VectorLoadSingle1AllLanes
SafetyLevel VectorLoadSingle1AllLanes::safety(Instruction i) const {
  switch (size.value(i)) {
    case 0x0:  // 00
      if (a.IsDefined(i)) return UNDEFINED;
      break;
    case 0x1:  // 01
    case 0x2:  // 10
      break;
    case 0x3:  // 11
      return UNDEFINED;
    default:
      return DECODER_ERROR;
  }
  if (rn.reg(i).Equals(Register::Pc())) return UNPREDICTABLE;
  if (d_reg_index(i) + num_regs(i) > 32) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

// VectorLoadSingle2AllLanes
SafetyLevel VectorLoadSingle2AllLanes::safety(Instruction i) const {
  if (size.value(i) == 0x3) return UNDEFINED;
  if (rn.reg(i).Equals(Register::Pc())) return UNPREDICTABLE;
  if (d_reg_index(i) + inc(i) > 31) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

// VectorLoadSingle3AllLanes
SafetyLevel VectorLoadSingle3AllLanes::safety(Instruction i) const {
  if (size.value(i) == 0x3) return UNDEFINED;
  if (a.IsDefined(i)) return UNDEFINED;
  if (rn.reg(i).Equals(Register::Pc())) return UNPREDICTABLE;
  if (d_reg_index(i) + 2 * inc(i) > 31) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

// VectorLoadSingle4AllLanes
SafetyLevel VectorLoadSingle4AllLanes::safety(Instruction i) const {
  if ((size.value(i) == 0x3) && (!a.IsDefined(i))) return UNDEFINED;
  if (rn.reg(i).Equals(Register::Pc())) return UNPREDICTABLE;
  if (d_reg_index(i) + 3 * inc(i) > 31) return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

// DataBarrier
SafetyLevel DataBarrier::safety(Instruction i) const {
  switch (option.value(i)) {
    case 0xF:  // 1111
    case 0xE:  // 1110
    case 0xB:  // 1011
    case 0xA:  // 1010
    case 0x7:  // 0111
    case 0x6:  // 0110
    case 0x3:  // 0011
    case 0x2:  // 0010
      return UncondDecoder::safety(i);
    default:
      return FORBIDDEN_OPERANDS;
  }
}

// InstructionBarrier
SafetyLevel InstructionBarrier::safety(Instruction i) const {
  if (option.value(i) != 0xF)
    return FORBIDDEN_OPERANDS;
  return UncondDecoder::safety(i);
}

}  // namespace nacl_arm_dec
