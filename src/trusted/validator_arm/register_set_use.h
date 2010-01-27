/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Models the register set/use of ARM instructions.
 */

#ifndef NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_REGISTER_SET_USE_H__
#define NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_REGISTER_SET_USE_H__

#include "native_client/src/trusted/validator_arm/arm_insts.h"

EXTERN_C_BEGIN

/* Model a set of registers (registers R0 through R15, CPSR, and SPSR).
 *Bit n is set only if it is in the set.
 */
typedef uint32_t ArmRegisterSet;

/* Models the index for the CPSR register. */
#define CPSR_INDEX 16

/* Models the index for the SPSR register. */
#define SPSR_INDEX 17

/* Models the number of user (R) registers. */
#define NUMBER_REGISTERS 16

/* Returns the set of registers used by the given instruction. */
ArmRegisterSet RegisterUses(const NcDecodedInstruction* inst);

/* Returns the set of registers whose value was set (i.e. modified)
 * by the given instruction.
 */
ArmRegisterSet RegisterSets(const NcDecodedInstruction* inst);

/* Returns the set of registers, for the given instruction,
 * that is incremented/decremented by a small constant.
 */
ArmRegisterSet RegisterSetIncremented(const NcDecodedInstruction* inst);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_REGISTER_SET_USE_H__ */
