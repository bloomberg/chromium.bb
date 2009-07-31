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
 * Defines the user API to the state associated with matching instructions.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_INST_STATE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_INST_STATE_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_x86/types_memory_model.h"

/* The meta model of an x86 opcode instruction. */
struct Opcode;

/* The (user) representation of the parsed x86 instruction. */
struct ExprNodeVector;

/* Defines the state used to match an instruction, while walking
 * instructions using the NcInstIter.
 */
typedef struct NcInstState NcInstState;

/* Returns the address (i.e. program counter) associated with the
 * currently matched instruction.
 */
PcAddress NcInstStateVpc(NcInstState* state);

/* Given an iterator state, return the corresponding opcode (instruction)
 * that matches the currently matched instruction of the corresponding
 * instruction iterator.
 */
struct Opcode* NcInstStateOpcode(NcInstState* state);

/* Given an iterator state, return the corresponding expression tree
 * denoting the currently matched instruction of the corresponding
 * instruction iterator.
 */
struct ExprNodeVector* NcInstStateNodeVector(NcInstState* state);

Bool NcInstStateIsNaclLegal(NcInstState* state);

/* Given an iterator state, return the number of bytes matched
 * by the currently matched instruction of the corresponding
 * instruction iterator.
 */
uint8_t NcInstStateLength(NcInstState* state);

/* Given an iterator state, return the index-th byte of the
 * currently matched instruction. Index must be less than
 * the value of the corresponding call to NcInstStateLength.
 */
uint8_t NcInstStateByte(NcInstState* state, uint8_t index);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_INST_STATE_H_ */
