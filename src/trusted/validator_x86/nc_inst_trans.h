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

#endif   /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_INST_TRANS_H_ */
