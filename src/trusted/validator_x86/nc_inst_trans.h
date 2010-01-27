/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Defines the API to converting the recognized opcode (instruction),
 * in the instruction state, to the corresponding opcode expression.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_INST_TRANS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_INST_TRANS_H_

#include "native_client/src/trusted/validator_x86/ncopcode_desc.h"

/* Defines the state used to match an instruction, while walking
 * instructions using the NcInstIter.
 */
struct NcInstState;

/* Constructs the corresponding ExprNodeVector from the matched
 * Opcode of the instruction state.
 */
void BuildExprNodeVector(struct NcInstState* state);

/* Returns true iff the given 32 bit register is the base part of the
 * corresponding given 64-bit register.
 */
Bool Is32To64RegisterPair(OperandKind reg32, OperandKind reg64);

/* Returns true iff the given (non-64 bit) subregister is a subpart
 * of the corresponding 64-bit register. Note: state is passed in
 * because different look ups are used for 8 bit registers, depending
 * on whether a REX prefix is found.
 */
Bool NcIs64Subregister(struct NcInstState* state,
                       OperandKind subreg, OperandKind reg64);

/* Given a 32-bit register, return the corresponding 64-bit register.
 * Returns RegUnknown if no such register exists.
 */
OperandKind NcGet64For32BitRegister(OperandKind reg32);

/* Given a 64-bit register, return the corresponding 32-bit register.
 * Returns RegUnknown if no such register exists.
 */
OperandKind NcGet32For64BitRegister(OperandKind reg64);

#endif   /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_INST_TRANS_H_ */
