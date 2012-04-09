/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Defines decoder testers for decoder classes.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_INST_CLASSES_TESTERS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_INST_CLASSES_TESTERS_H_

#include "native_client/src/trusted/validator_arm/decoder_tester.h"

namespace nacl_arm_test {

// Implements a decoder tester for decoder Binary4RegisterShiftedOp.
// Op(S)<c> <Rd>, <Rn>, <Rm>,  <type> <Rs>
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// |31302928|27262524232221|20|19181716|15141312|1110 9 8| 7| 6 5| 4| 3 2 1 0|
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// |  cond  |              | S|   Rn   |   Rd   |   Rs   |  |type|  |   Rm   |
// +--------+--------------+--+--------+--------+--------+--+----+--+--------+
// Note: if Rn, Rd, Rs, or Rm is R15, the instruction is unpredictable.
// Definitions:
//    Rd - The destination register.
//    Rn - The first operand register.
//    Rm - The register that is shifted and used as the second operand.
//    Rs - The regsiter whose bottom byte contains the amount to shift by.
//    type - The type of shift to apply (not modeled).
//    S - Defines if the flags regsiter is updated.
class Binary4RegisterShiftedOpTester : public Arm32DecoderTester {
 public:
  explicit Binary4RegisterShiftedOpTester();
  virtual void ApplySanityChecks(nacl_arm_dec::Instruction inst,
                                 const nacl_arm_dec::ClassDecoder& decoder);
};

}  // namespace

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_INST_CLASSES_TESTERS_H_
