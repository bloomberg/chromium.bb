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
SafetyLevel UncondDecoder::safety(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList UncondDecoder::defs(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

// CondDecoder
SafetyLevel CondDecoder::safety(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList CondDecoder::defs(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

// CondAdvSIMDOp
SafetyLevel CondAdvSIMDOp::safety(const Instruction i) const {
  return (cond.value(i) == Instruction::AL)
      ? MAY_BE_SAFE
      : DEPRECATED;
}

// MoveImmediate12ToApsr
SafetyLevel MoveImmediate12ToApsr::safety(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList MoveImmediate12ToApsr::defs(const Instruction i) const {
  RegisterList regs;
  if (UpdatesConditions(i)) {
    regs.Add(kConditions);
  }
  return regs;
}

// Immediate16Use
SafetyLevel Immediate16Use::safety(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList Immediate16Use::defs(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

// BranchImmediate24
SafetyLevel BranchImmediate24::safety(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList BranchImmediate24::defs(const Instruction i) const {
  return RegisterList(kRegisterPc).
      Add(link_flag.IsDefined(i) ? kRegisterLink : kRegisterNone);
}

bool BranchImmediate24::is_relative_branch(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return true;
}

int32_t BranchImmediate24::branch_target_offset(Instruction i) const {
  return imm24.relative_address(i);
}

// BreakPointAndConstantPoolHead
SafetyLevel BreakPointAndConstantPoolHead::safety(const Instruction i) const {
  return i.GetCondition() == Instruction::AL
      ? MAY_BE_SAFE
      : UNPREDICTABLE;
}
bool BreakPointAndConstantPoolHead::
is_literal_pool_head(const Instruction i) const {
  return i.Bits(31, 0) == kLiteralPoolHeadInstruction;
}

// BranchToRegister
SafetyLevel BranchToRegister::safety(const Instruction i) const {
  // Extra NaCl constraint: can't branch to PC. This would branch to 8 bytes
  // after the current instruction. This instruction should be in an instruction
  // pair, the mask should therefore be to PC and fail checking, but there's
  // little harm in checking.
  if (m.reg(i).Equals(kRegisterPc)) return FORBIDDEN_OPERANDS;

  // Redundant with the above, but this is actually UNPREDICTABLE. Expect DCE.
  if (link_register.IsUpdated(i) && m.reg(i).Equals(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  return MAY_BE_SAFE;
}

RegisterList BranchToRegister::defs(const Instruction i) const {
  return RegisterList(kRegisterPc).
      Add(link_register.IsUpdated(i) ? kRegisterLink : kRegisterNone);
}

Register BranchToRegister::branch_target_register(const Instruction i) const {
  return m.reg(i);
}

// Unary1RegisterImmediateOp
SafetyLevel Unary1RegisterImmediateOp::safety(const Instruction i) const {
  if (d.reg(i).Equals(kRegisterPc)) return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Unary1RegisterImmediateOp::defs(const Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

// Unary1RegisterImmediateOpDynCodeReplace
Instruction Unary1RegisterImmediateOpDynCodeReplace::
dynamic_code_replacement_sentinel(Instruction i) const {
  if (!kDynCodeReplaceFrozenRegs.Contains(d.reg(i))) {
    imm4.set_value(&i, 0);
    imm12.set_value(&i, 0);
  }
  return i;
}

// Binary2RegisterBitRangeMsbGeLsb
SafetyLevel Binary2RegisterBitRangeMsbGeLsb::safety(Instruction i) const {
  return (d.reg(i).Equals(kRegisterPc) ||
          (msb.value(i) < lsb.value(i)))
      ? UNPREDICTABLE
      : MAY_BE_SAFE;
}

RegisterList Binary2RegisterBitRangeMsbGeLsb::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

// Binary2RegisterBitRangeNotRnIsPcBitfieldExtract
SafetyLevel Binary2RegisterBitRangeNotRnIsPcBitfieldExtract
::safety(Instruction i) const {
  return (RegisterList(d.reg(i)).Add(n.reg(i)).Contains(kRegisterPc) ||
          (lsb.value(i) + widthm1.value(i) > 31))
      ? UNPREDICTABLE
      : MAY_BE_SAFE;
}

RegisterList Binary2RegisterBitRangeNotRnIsPcBitfieldExtract
::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

// Binary2RegisterImmediateOp
SafetyLevel Binary2RegisterImmediateOp::safety(Instruction i) const {
  // NaCl Constraint.
  if (d.reg(i).Equals(kRegisterPc)) return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

RegisterList Binary2RegisterImmediateOp::defs(Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

// TODO(karl): find out why we added this so that we allowed an
// override on NaCl restriction that one can write to r15.
// MaskedBinary2RegisterImmediateOp
// SafetyLevel MaskedBinary2RegisterImmediateOp::safety(Instruction i) const {
//   UNREFERENCED_PARAMETER(i);
//   return MAY_BE_SAFE;
// }

bool MaskedBinary2RegisterImmediateOp::clears_bits(
    Instruction i, uint32_t mask) const {
  return (imm.get_modified_immediate(i) & mask) == mask;
}

// Binary2RegisterImmediateOpDynCodeReplace
Instruction Binary2RegisterImmediateOpDynCodeReplace::
dynamic_code_replacement_sentinel(Instruction i) const {
  if (!kDynCodeReplaceFrozenRegs.Contains(d.reg(i))) {
    imm.set_value(&i, 0);
  }
  return i;
}

// BinaryRegisterImmediateTest::
SafetyLevel BinaryRegisterImmediateTest::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList BinaryRegisterImmediateTest::defs(Instruction i) const {
  return RegisterList(conditions.conds_if_updated(i));
}

// MaskedBinaryRegisterImmediateTest
bool MaskedBinaryRegisterImmediateTest::sets_Z_if_bits_clear(
    Instruction i, Register r, uint32_t mask) const {
  return n.reg(i).Equals(r) &&
      (imm.get_modified_immediate(i) & mask) == mask &&
      defs(i).Contains(kConditions);
}

// Unary2RegisterOp
SafetyLevel Unary2RegisterOp::safety(const Instruction i) const {
  // NaCl Constraint.
  if (d.reg(i).Equals(kRegisterPc)) return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

RegisterList Unary2RegisterOp::defs(const Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

// Unary2RegsiterOpNotRmIsPc
SafetyLevel Unary2RegisterOpNotRmIsPc::safety(const Instruction i) const {
  if (m.reg(i).Equals(kRegisterPc))
    return UNPREDICTABLE;
  return Unary2RegisterOp::safety(i);
}

// Unary2RegisterOpNotRmIsPcNoCondUpdates
RegisterList Unary2RegisterOpNotRmIsPcNoCondUpdates
::defs(const Instruction i) const {
  return RegisterList(d.reg(i));
}

// Binary3RegisterOp
SafetyLevel Binary3RegisterOp::safety(const Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(d.reg(i)).Add(m.reg(i)).Add(n.reg(i)).
      Contains(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Binary3RegisterOp::defs(const Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

// Binary3RegisterOpAltA
SafetyLevel Binary3RegisterOpAltA::safety(const Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(d.reg(i)).Add(m.reg(i)).Add(n.reg(i)).
      Contains(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Binary3RegisterOpAltA::defs(const Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}


// Binary3RegisterOpAltBNoCondUpdates
SafetyLevel Binary3RegisterOpAltBNoCondUpdates::
safety(const Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(d.reg(i)).Add(m.reg(i)).Add(n.reg(i)).
      Contains(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Binary3RegisterOpAltBNoCondUpdates
::defs(const Instruction i) const {
  return RegisterList(d.reg(i));
}

// Binary4RegisterDualOp
SafetyLevel Binary4RegisterDualOp::safety(const Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(d.reg(i)).Add(m.reg(i)).Add(n.reg(i)).Add(a.reg(i)).
      Contains(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Binary4RegisterDualOp::defs(const Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

// Binary4RegisterDualResult
SafetyLevel Binary4RegisterDualResult::safety(const Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(d_lo.reg(i)).Add(d_hi.reg(i)).Add(n.reg(i)).Add(m.reg(i)).
      Contains(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  // Unsafe if RdHi == RdLo
  if (d_hi.reg(i).Equals(d_lo.reg(i))) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Binary4RegisterDualResult::defs(const Instruction i) const {
  return RegisterList(d_hi.reg(i)).Add(d_lo.reg(i)).
      Add(conditions.conds_if_updated(i));
}

// LoadExclusive2RegisterOp
SafetyLevel LoadExclusive2RegisterOp::safety(const Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(n.reg(i)).Add(t.reg(i)).Contains(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList LoadExclusive2RegisterOp::defs(const Instruction i) const {
  return RegisterList(t.reg(i));
}

Register LoadExclusive2RegisterOp::base_address_register(
    const Instruction i) const {
  return n.reg(i);
}

// LoadExclusive2RegisterDoubleOp
SafetyLevel LoadExclusive2RegisterDoubleOp::safety(const Instruction i) const {
  // Arm restrictions for this instruction, based on double width.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (t2.reg(i).Equals(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  return LoadExclusive2RegisterOp::safety(i);
}

RegisterList LoadExclusive2RegisterDoubleOp::defs(const Instruction i) const {
  return RegisterList(t.reg(i)).Add(t2.reg(i));
}

// LoadStore2RegisterImm8Op
SafetyLevel LoadStore2RegisterImm8Op::safety(const Instruction i) const {
  // Arm restrictions for this instruction.
  if (t.reg(i).Equals(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(kRegisterPc) ||
       // NOTE: The manual states that that it is also unpredictable
       // when HasWriteBack(i) and Rn=Rt. However, the compilers
       // may not check for this. For the moment, we are changing
       // the code to ignore this case for stores.
       // TODO(karl): Should we not allow this?
       // TODO(jfb) Fix this.
       (is_load_ && n.reg(i).Equals(t.reg(i))))) {
    return UNPREDICTABLE;
  }

  // Above implies literal loads can't writeback, the following checks the
  // ARM restriction that literal loads can't have P == W.
  // This should always decode to another instruction, but checking it is good.
  if (n.reg(i).Equals(kRegisterPc) &&
      (indexing.IsDefined(i) == writes.IsDefined(i))) {
    return UNPREDICTABLE;
  }

  // Don't allow modification of PC (NaCl constraint).
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

RegisterList LoadStore2RegisterImm8Op::immediate_addressing_defs(
    const Instruction i) const {
  return RegisterList(HasWriteBack(i) ? n.reg(i) : kRegisterNone);
}

Register LoadStore2RegisterImm8Op::base_address_register(
    const Instruction i) const {
  return n.reg(i);
}

// Load2RegisterImm8Op
RegisterList Load2RegisterImm8Op::defs(Instruction i) const {
  return immediate_addressing_defs(i).Add(t.reg(i));
}

bool Load2RegisterImm8Op::is_literal_load(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return base_address_register(i).Equals(kRegisterPc);
}

// Store2RegisterImm8Op
RegisterList Store2RegisterImm8Op::defs(Instruction i) const {
  return immediate_addressing_defs(i);
}

// LoadStore2RegisterImm8DoubleOp
SafetyLevel LoadStore2RegisterImm8DoubleOp::
safety(const Instruction i) const {
  // Arm restrictions for this instruction, based on double width.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (t2.reg(i).Equals(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  // NOTE: The manual states that that it is also unpredictable
  // when HasWriteBack(i) and Rn=Rt2. However, the compilers
  // may not check for this. For the moment, we are changing
  // the code to ignore this case for stores.
  // TODO(karl): Should we not allow this?
  if (is_load_ && HasWriteBack(i) && n.reg(i).Equals(t2.reg(i))) {
    return UNPREDICTABLE;
  }

  // Now apply non-double width restrictions for this instruction.
  return LoadStore2RegisterImm8Op::safety(i);
}

// Load2RegisterImm8DoubleOp
RegisterList Load2RegisterImm8DoubleOp::defs(Instruction i) const {
  return immediate_addressing_defs(i).Add(t.reg(i)).Add(t2.reg(i));
}

bool Load2RegisterImm8DoubleOp::is_literal_load(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return base_address_register(i).Equals(kRegisterPc);
}

// Store2RegisterImm8DoubleOp
RegisterList Store2RegisterImm8DoubleOp::defs(Instruction i) const {
  return immediate_addressing_defs(i);
}

// PreloadRegisterImm12Op
SafetyLevel PreloadRegisterImm12Op::safety(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList PreloadRegisterImm12Op::defs(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return kCondsDontCare;
}

// PreloadRegisterPairOp
SafetyLevel PreloadRegisterPairOp::safety(Instruction i) const {
  if (m.reg(i).Equals(kRegisterPc))
    return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

RegisterList PreloadRegisterPairOp::defs(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return kCondsDontCare;
}

// PreloadRegisterPairOpWAndRnNotPc
SafetyLevel PreloadRegisterPairOpWAndRnNotPc::safety(Instruction i) const {
  // Defined in version C of ARM ARM v7 manual.
  // if m==15 || (n==15 && is_pldw) then UNPREDICTABLE;
  if (!read.IsDefined(i) && n.reg(i).Equals(kRegisterPc))
    return FORBIDDEN_OPERANDS;
  return PreloadRegisterPairOp::safety(i);
}

// LoadStore2RegisterImm12Op
SafetyLevel LoadStore2RegisterImm12Op::safety(const Instruction i) const {
  // Arm restrictions for this instruction.
  if (HasWriteBack(i) &&
      (n.reg(i).Equals(kRegisterPc) ||
       // NOTE: The manual states that that it is also unpredictable
       // when HasWriteBack(i) and Rn=Rt. However, the compilers
       // may not check for this. For the moment, we are changing
       // the code to ignore this case for stores.
       // TODO(karl): Should we not allow this?
       // TODO(jfb) Fix this. Once this is fixed,
       //           Store2RegisterImm12OpRnNotRtOnWriteback becomes redundant
       //           with this class and can be removed.
       (is_load_ && n.reg(i).Equals(t.reg(i))))) {
    return UNPREDICTABLE;
  }

  // Don't allow modification of PC (NaCl constraint).
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;

  // NaCl special restriction to make all load/store immediates behave
  // the same.
  if (t.reg(i).Equals(kRegisterPc)) {
    return FORBIDDEN_OPERANDS;
  }

  return MAY_BE_SAFE;
}

RegisterList LoadStore2RegisterImm12Op::immediate_addressing_defs(
    const Instruction i) const {
  return RegisterList(HasWriteBack(i) ? n.reg(i) : kRegisterNone);
}

Register LoadStore2RegisterImm12Op::base_address_register(
    const Instruction i) const {
  return n.reg(i);
}

// Load2RegisterImm12Op
RegisterList Load2RegisterImm12Op::defs(Instruction i) const {
  return immediate_addressing_defs(i).Add(t.reg(i));
}

bool Load2RegisterImm12Op::is_literal_load(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return base_address_register(i).Equals(kRegisterPc);
}

// Store2RegisterImm12Op
RegisterList Store2RegisterImm12Op::defs(Instruction i) const {
  return immediate_addressing_defs(i);
}

// Store2RegisterImm12OpRnNotRtOnWriteback
SafetyLevel Store2RegisterImm12OpRnNotRtOnWriteback::
safety(Instruction i) const {
  // TODO(jfb) Redundant with LoadStore2RegisterImm12Op, once it is fixed.
  if (HasWriteBack(i) && (n.reg(i).Equals(t.reg(i))))
    return UNPREDICTABLE;
  return Store2RegisterImm12Op::safety(i);
}

// LoadStoreRegisterList
SafetyLevel LoadStoreRegisterList::safety(const Instruction i) const {
  if (n.reg(i).Equals(kRegisterPc) || (register_list.value(i) == 0)) {
    return UNPREDICTABLE;
  }
  return MAY_BE_SAFE;
}

RegisterList LoadStoreRegisterList::defs(const Instruction i) const {
  return immediate_addressing_defs(i);
}

Register LoadStoreRegisterList::
base_address_register(const Instruction i) const {
  return n.reg(i);
}

RegisterList LoadStoreRegisterList::
immediate_addressing_defs(const Instruction i) const {
  return RegisterList(wback.IsDefined(i) ? n.reg(i) : kRegisterNone);
}

// LoadRegisterList
SafetyLevel LoadRegisterList::safety(const Instruction i) const {
  if (wback.IsDefined(i) && register_list.registers(i).Contains(n.reg(i))) {
    return UNPREDICTABLE;
  }
  if (register_list.registers(i).Contains(kRegisterPc)) {
    return FORBIDDEN_OPERANDS;
  }
  return LoadStoreRegisterList::safety(i);
}

RegisterList LoadRegisterList::defs(const Instruction i) const {
  return register_list.registers(i).Union(LoadStoreRegisterList::defs(i));
}

// StoreRegisterList
SafetyLevel StoreRegisterList::safety(const Instruction i) const {
  if (wback.IsDefined(i) && register_list.registers(i).Contains(n.reg(i)) &&
      ((register_list.value(i) & (0xFFFFFFFFu << n.reg(i).number())) !=
       register_list.value(i))) {
    // Stored value is unknown if there's writeback and the base register isn't
    // at least the first register that is stored.
    return UNPREDICTABLE;
  }
  return LoadStoreRegisterList::safety(i);
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
base_address_register(const Instruction i) const {
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

SafetyLevel LoadStoreVectorRegisterList::safety(const Instruction i) const {
  // ARM constraints.
  if (wback.IsDefined(i) && (indexing.IsDefined(i) == direction.IsAdd(i)))
    return UNDEFINED;

  if (wback.IsDefined(i) && n.reg(i).Equals(kRegisterPc))
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
  return LoadStoreVectorOp::safety(i);
}

RegisterList LoadStoreVectorRegisterList::defs(const Instruction i) const {
  return immediate_addressing_defs(i);
}

RegisterList LoadStoreVectorRegisterList::
immediate_addressing_defs(const Instruction i) const {
  return RegisterList(wback.IsDefined(i) ? n.reg(i) : kRegisterNone);
}

// LoadVectorRegisterList
bool LoadVectorRegisterList::is_literal_load(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return base_address_register(i).Equals(kRegisterPc);
}

// StoreVectorRegisterList
SafetyLevel StoreVectorRegisterList::safety(const Instruction i) const {
  if (n.reg(i).Equals(kRegisterPc))
    return FORBIDDEN_OPERANDS;
  return LoadStoreVectorRegisterList::safety(i);
}

// LoadStoreVectorRegister

// LoadVectorRegister
bool LoadVectorRegister::is_literal_load(Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return base_address_register(i).Equals(kRegisterPc);
}

// StoreVectorRegister
SafetyLevel StoreVectorRegister::safety(const Instruction i) const {
  if (n.reg(i).Equals(kRegisterPc)) {
    // Note: covers ARM and NaCl constraints:
    //   Rn!=PC
    //   Rn=PC  && CurrentInstSet() != ARM then UNPREDICTABLE.
    return UNPREDICTABLE;
  }

  // Make sure coprocessor value is safe.
  return LoadStoreVectorRegister::safety(i);
}

// LoadStore3RegisterOp
SafetyLevel LoadStore3RegisterOp::safety(const Instruction i) const {
  if (indexing.IsPreIndexing(i)) {
    // If pre-indexing is set, the address of the load is computed as the sum
    // of the two register parameters.  We have checked that the first register
    // is within the sandbox, but this would allow adding an arbitrary value
    // to it, so it is not safe. (NaCl constraint).
    return FORBIDDEN;
  }

  // Arm restrictions for this instruction.
  if (RegisterList(t.reg(i)).Add(m.reg(i)).Contains(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(kRegisterPc) ||
       // NOTE: The manual states that that it is also unpredictable
       // when HasWriteBack(i) and Rn=Rt. However, the compilers
       // may not check for this. For the moment, we are changing
       // the code to ignore this case for stores.
       // TODO(karl): Should we not allow this?
       // TODO(jfb) Fix this.
       (is_load_ && n.reg(i).Equals(t.reg(i))))) {
    return UNPREDICTABLE;
  }

  // TODO(jfb) if ArchVersion() < 6 && wback && m == n then UNPREDICTABLE;

  // Don't let addressing writeback alter PC (NaCl constraint).
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

Register LoadStore3RegisterOp::base_address_register(
    const Instruction i) const {
  return n.reg(i);
}

// Load3RegisterOp
RegisterList Load3RegisterOp::defs(const Instruction i) const {
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
SafetyLevel LoadStore3RegisterDoubleOp::safety(const Instruction i) const {
  // Arm restrictions for this instruction, based on double width.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (RegisterList(t2.reg(i)).Add(m.reg(i)).Contains(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  // NOTE: The manual states that that it is also unpredictable
  // when HasWriteBack(i) and Rn=Rt2. However, the compilers
  // may not check for this. For the moment, we are changing
  // the code to ignore this case for stores.
  // TODO(karl): Should we not allow this?
  // TODO(jfb) Fix this.
  if (is_load_ && HasWriteBack(i) && n.reg(i).Equals(t2.reg(i))) {
    return UNPREDICTABLE;
  }

  if (is_load_ && (m.reg(i).Equals(t.reg(i)) || m.reg(i).Equals(t2.reg(i)))) {
    return UNPREDICTABLE;
  }

  // Now apply non-double width restrictions for this instruction.
  return LoadStore3RegisterOp::safety(i);
}

// StoreExclusive3RegisterOp
SafetyLevel StoreExclusive3RegisterOp::safety(const Instruction i) const {
  // Arm restrictions for this instruction.
  if (RegisterList(d.reg(i)).Add(t.reg(i)).Add(n.reg(i)).
      Contains(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  if (d.reg(i).Equals(n.reg(i)) || d.reg(i).Equals(t.reg(i))) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList StoreExclusive3RegisterOp::defs(const Instruction i) const {
  return RegisterList(d.reg(i));
}

Register StoreExclusive3RegisterOp::base_address_register(
    const Instruction i) const {
  return n.reg(i);
}

// StoreExclusive3RegisterDoubleOp
SafetyLevel StoreExclusive3RegisterDoubleOp::safety(const Instruction i) const {
  // Arm restrictions for this instruction, based on double width.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (t2.reg(i).Equals(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  if (d.reg(i).Equals(t2.reg(i))) {
    return UNPREDICTABLE;
  }

  // Now apply non-double width restrictions for this instruction.
  return StoreExclusive3RegisterOp::safety(i);
}

// Load3RegisterDoubleOp
RegisterList Load3RegisterDoubleOp::defs(const Instruction i) const {
  RegisterList defines;
  defines.Add(t.reg(i)).Add(t2.reg(i));
  if (HasWriteBack(i)) {
    defines.Add(n.reg(i));
  }
  return defines;
}

// Store3RegisterDoubleOp
RegisterList Store3RegisterDoubleOp::defs(Instruction i) const {
  RegisterList defines;
  if (HasWriteBack(i)) {
    defines.Add(n.reg(i));
  }
  return defines;
}

// LoadStore3RegisterImm5Op
SafetyLevel LoadStore3RegisterImm5Op::safety(const Instruction i) const {
  if (indexing.IsPreIndexing(i)) {
    // If pre-indexing is set, the address of the load is computed as the sum
    // of the two register parameters.  We have checked that the first register
    // is within the sandbox, but this would allow adding an arbitrary value
    // to it, so it is not safe. (NaCl constraint).
    return FORBIDDEN;
  }

  // Arm restrictions for this instruction.
  if (m.reg(i).Equals(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(kRegisterPc) ||
       // NOTE: The manual states that that it is also unpredictable
       // when HasWriteBack(i) and Rn=Rt2. However, the compilers
       // may not check for this. For the moment, we are changing
       // the code to ignore this case for stores.
       // TODO(karl): Should we not allow this?
       // TODO(jfb) Fix this.
       (is_load_ && n.reg(i).Equals(t.reg(i))))) {
    return UNPREDICTABLE;
  }

  // Don't let Rt be Pc (NaCl constraint -- See header file for special
  // note).
  if (t.reg(i).Equals(kRegisterPc)) {
    return FORBIDDEN_OPERANDS;
  }

  // Don't let addressing writeback alter PC (NaCl constraint).
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;

  // TODO(jfb) if ArchVersion() < 6 && wback && m == n then UNPREDICTABLE;

  return MAY_BE_SAFE;
}

Register LoadStore3RegisterImm5Op::base_address_register(
    const Instruction i) const {
  return n.reg(i);
}

// Load3RegisterImm5Op
RegisterList Load3RegisterImm5Op::defs(const Instruction i) const {
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

// Unary2RegisterImmedShiftedOp
SafetyLevel Unary2RegisterImmedShiftedOp::safety(const Instruction i) const {
  // NaCl Constraint.
  if (d.reg(i).Equals(kRegisterPc)) return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

RegisterList Unary2RegisterImmedShiftedOp::defs(const Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

// Unary2RegisterImmediateShiftedOpRegsNotPc
SafetyLevel Unary2RegisterImmedShiftedOpRegsNotPc::
safety(const Instruction i) const {
  if (RegisterList(d.reg(i)).Add(m.reg(i)).Contains(kRegisterPc))
      return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

// Unary2RegisterSatImmedShiftedOp
SafetyLevel Unary2RegisterSatImmedShiftedOp::safety(const Instruction i) const {
  if (RegisterList(d.reg(i)).Add(n.reg(i)).Contains(kRegisterPc))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Unary2RegisterSatImmedShiftedOp::defs(const Instruction i) const {
  return RegisterList(d.reg(i));
}

// Unary3RegisterShiftedOp
SafetyLevel Unary3RegisterShiftedOp::safety(Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(d.reg(i)).Add(s.reg(i)).Add(m.reg(i)).Contains(kRegisterPc))
    return UNPREDICTABLE;

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Unary3RegisterShiftedOp::defs(const Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

// Binary3RegisterImmedShiftedOp
SafetyLevel Binary3RegisterImmedShiftedOp::safety(const Instruction i) const {
  // NaCl Constraint.
  if (d.reg(i).Equals(kRegisterPc)) return FORBIDDEN_OPERANDS;
  return MAY_BE_SAFE;
}

RegisterList Binary3RegisterImmedShiftedOp::defs(const Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

// Binary3RegisterImmedShiftedOpRegsNotPc
SafetyLevel Binary3RegisterImmedShiftedOpRegsNotPc::
safety(const Instruction i) const {
  if (RegisterList(d.reg(i)).Add(n.reg(i)).Add(m.reg(i))
      .Contains(kRegisterPc)) {
    return UNPREDICTABLE;
  }
  return MAY_BE_SAFE;
}

// Binary4RegisterShiftedOp
SafetyLevel Binary4RegisterShiftedOp::safety(Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(d.reg(i)).Add(n.reg(i)).Add(s.reg(i)).Add(m.reg(i)).
      Contains(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Binary4RegisterShiftedOp::defs(const Instruction i) const {
  return RegisterList(d.reg(i)).Add(conditions.conds_if_updated(i));
}

// Binary2RegisterImmedShiftedTest
SafetyLevel Binary2RegisterImmedShiftedTest::safety(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return MAY_BE_SAFE;
}

RegisterList Binary2RegisterImmedShiftedTest::defs(const Instruction i) const {
  return RegisterList(conditions.conds_if_updated(i));
}

// Binary3RegisterShiftedTest
SafetyLevel Binary3RegisterShiftedTest::safety(Instruction i) const {
  // Unsafe if any register contains PC (ARM restriction).
  if (RegisterList(n.reg(i)).Add(s.reg(i)).Add(m.reg(i)).Contains(kRegisterPc))
    return UNPREDICTABLE;
  return MAY_BE_SAFE;
}

RegisterList Binary3RegisterShiftedTest::defs(const Instruction i) const {
  return RegisterList(conditions.conds_if_updated(i));
}

// VectorUnary2RegisterDup
SafetyLevel VectorUnary2RegisterDup::safety(Instruction i) const {
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

  return VectorUnary2RegisterOpBase::safety(i);
}

// VectorBinary3RegisterImmOp
SafetyLevel VectorBinary3RegisterImmOp::safety(Instruction i) const {
  if (q.IsDefined(i)) {
    if (!vd.IsEven(i) || !vn.IsEven(i) || !vm.IsEven(i))
      return UNDEFINED;
  } else if (imm.value(i) > 0x7) {
    return UNDEFINED;
  }
  return VectorBinary3RegisterOpBase::safety(i);
}

// VectorBinary3RegisterLookupOp
SafetyLevel VectorBinary3RegisterLookupOp::safety(Instruction i) const {
  if (n_reg_index(i) + length(i) > 32)
    return UNPREDICTABLE;
  return VectorBinary3RegisterOpBase::safety(i);
}

// VfpUsesRegOp
SafetyLevel VfpUsesRegOp::safety(Instruction i) const {
  if (t.reg(i).Equals(kRegisterPc))
    return UNPREDICTABLE;
  return CondVfpOp::safety(i);
}

RegisterList VfpUsesRegOp::defs(const Instruction i) const {
  UNREFERENCED_PARAMETER(i);
  return RegisterList();
}

// VfpMsrOp
SafetyLevel VfpMrsOp::safety(Instruction i) const {
  return CondVfpOp::safety(i);
}

RegisterList VfpMrsOp::defs(const Instruction i) const {
  return RegisterList(t.reg(i).Equals(kRegisterPc)
                      ? kConditions : t.reg(i));
}

// MoveVfpRegisterOp
SafetyLevel MoveVfpRegisterOp::safety(Instruction i) const {
  if (t.reg(i).Equals(kRegisterPc))
    return UNPREDICTABLE;
  return CondVfpOp::safety(i);
}

RegisterList MoveVfpRegisterOp::defs(const Instruction i) const {
  RegisterList defs;
  if (to_arm_reg.IsDefined(i)) defs.Add(t.reg(i));
  return defs;
}

// MoveVfpRegisterOpWithTypeSel
SafetyLevel MoveVfpRegisterOpWithTypeSel::safety(Instruction i) const {
  // Compute type_sel = opc1(23:21):opc2(6:5).
  uint32_t type_sel = (opc1.value(i) << 2) | opc2.value(i);

  // If type_sel in { '10x00' , 'x0x10' }, the instruction is undefined.
  if (((type_sel & 0x1b) == 0x10) ||  /* type_sel == '10x00 */
      ((type_sel & 0xb) == 0x2))      /* type_sel == 'x0x10 */
    return UNDEFINED;

  return MoveVfpRegisterOp::safety(i);
}

// MoveDoubleVfpRegisterOp
SafetyLevel MoveDoubleVfpRegisterOp::safety(Instruction i) const {
  if ((t2.reg(i).Equals(kRegisterPc)) ||
      (IsSinglePrecision(i) &&
       (((vm.reg(i).number() << 1) | m.value(i)) == 31)) ||
      (to_arm_reg.IsDefined(i) && t.reg(i).Equals(t2.reg(i))))
    return UNPREDICTABLE;
  return MoveVfpRegisterOp::safety(i);
}

RegisterList MoveDoubleVfpRegisterOp::defs(const Instruction i) const {
  RegisterList defs;
  if (to_arm_reg.IsDefined(i)) defs.Add(t.reg(i)).Add(t2.reg(i));
  return defs;
}

// DuplicateToAdvSIMDRegisters
SafetyLevel DuplicateToAdvSIMDRegisters::safety(Instruction i) const {
  if (is_two_regs.IsDefined(i) && !vd.IsEven(i))
    return UNDEFINED;

  if (be_value(i) == 0x3)
    return UNDEFINED;

  if (t.reg(i).Equals(kRegisterPc))
    return UNPREDICTABLE;

  return CondAdvSIMDOp::safety(i);
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
