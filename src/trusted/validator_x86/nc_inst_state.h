/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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

/* Returns the operand size (measured in bytes) of the instruction state. */
uint8_t NcInstStateOperandSize(NcInstState* state);

/* Returns the address size (measured in bits) of the instruction state. */
uint8_t NcInstStateAddressSize(NcInstState* state);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_INST_STATE_H_ */
