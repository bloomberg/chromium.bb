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

// Unary1RegisterBitRange
SafetyLevel Unary1RegisterBitRange::safety(Instruction i) const {
  if (d.reg(i).Equals(kRegisterPc)) return UNPREDICTABLE;
  // Note: We would restrict out PC as well for Rd in NaCl, but no need
  // since the ARM restriction doesn't allow it anyway.
  return MAY_BE_SAFE;
}

RegisterList Unary1RegisterBitRange::defs(Instruction i) const {
  return RegisterList(d.reg(i));
}

bool Unary1RegisterBitRange::clears_bits(Instruction i, uint32_t mask) const {
  int msbit = msb.value(i);
  int lsbit = lsb.value(i);
  int width = msbit + 1 - lsbit;
  if (width == 32) {
    return mask == 0;
  } else {
    uint32_t bit_mask = (((1 << width) - 1) << lsbit);
    return (bit_mask & mask) == mask;
  }
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
      (n.reg(i).Equals(kRegisterPc) || n.reg(i).Equals(t.reg(i)))) {
    return UNPREDICTABLE;
  }

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
  // Since we don't allow preindexing, only one form is allowed:
  //   ldr Rt, [Rn], +/-Rm
  // This form implies that HasWriteBack(i) is true.
  return RegisterList(t.reg(i)).Add( base_address_register(i));
}

// Store3RegisterOp
RegisterList Store3RegisterOp::defs(Instruction i) const {
  // Since we don't allow preindexing, only one form is allowed:
  //   str Rt, [Rn], +/-Rm
  // This form implies that HasWriteBack(i) is true
  return RegisterList(base_address_register(i));
}

// LoadStore3RegisterDoubleOp
SafetyLevel LoadStore3RegisterDoubleOp::safety(const Instruction i) const {
  if (indexing.IsPreIndexing(i)) {
    // If pre-indexing is set, the address of the load is computed as the sum
    // of the two register parameters.  We have checked that the first register
    // is within the sandbox, but this would allow adding an arbitrary value
    // to it, so it is not safe. (NaCl constraint).
    return FORBIDDEN;
  }

  // Arm restrictions for this instruction.
  if (!t.IsEven(i)) {
    return UNDEFINED;
  }

  if (RegisterList(t2.reg(i)).Add(m.reg(i)).Contains(kRegisterPc)) {
    return UNPREDICTABLE;
  }

  if (HasWriteBack(i) &&
      (n.reg(i).Equals(kRegisterPc) || n.reg(i).Equals(t.reg(i)) ||
       n.reg(i).Equals(t2.reg(i)))) {
    return UNPREDICTABLE;
  }

  // Don't let addressing writeback alter PC (NaCl constraint).
  if (defs(i).Contains(kRegisterPc)) return FORBIDDEN_OPERANDS;

  return MAY_BE_SAFE;
}

// Load3RegisterDoubleOp
RegisterList Load3RegisterDoubleOp::defs(const Instruction i) const {
  // Since we don't allow preindexing, only one form is allowed:
  //   ldr Rt, Rt2, [Rn], +/-Rm
  // This form implies that HasWriteBack(i) is true.
  return RegisterList(t.reg(i)).Add(t2.reg(i)).Add(base_address_register(i));
}

// Store3RegisterDoubleOp
RegisterList Store3RegisterDoubleOp::defs(Instruction i) const {
  // Since we don't allow preindexing, only one form is allowed:
  //   str Rt, [Rn], +/-Rm
  // This form implies that HasWriteBack(i) is true.
  return RegisterList(base_address_register(i));
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

}  // namespace
