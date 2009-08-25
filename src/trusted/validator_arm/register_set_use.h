/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
