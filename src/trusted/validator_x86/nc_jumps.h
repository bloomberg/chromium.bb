/* Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_JUMPS_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_JUMPS_H__

/*
 * nc_jumps.h - Implements set of possible jump points, and set of
 * actual jump points, and the verification that the possible
 * (explicit) jumps only apply to valid actual jumps.
 *
 * Note: The functions JumpValidatorCreate, JumpValidator,
 * JumpValidatorSummarize, and JumValidatorDestroy are used to
 * register JumpValidator as a validator function to be applied
 * to a validated segment, as defined in ncvalidate_iter.h.
 */

#include <stdio.h>

#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator_x86/types_memory_model.h"

/* The model of a validator state. */
struct NcValidatorState;

/* The model of an iterator through instructions in a code segment. */
struct NcInstIter;

/* The model of a parsed instruction. */
struct NcInstState;

/* Implements the possible/actual sets of jumps. */
struct JumpSets;

/* When true, chnages the behaviour of NcAddJump to use mask 0xFF for
 * indirect jumps (which is a nop). This allows performance tests for
 * compiled libraries without having to hand tweak the source code.
 */
extern Bool FLAGS_identity_mask;

/* Creates jump sets to track the set of possible and actual (explicit)
 * address.
 */
struct JumpSets* NcJumpValidatorCreate(struct NcValidatorState* state);

/* Collects information on instruction addresses, and where explicit jumps
 * go to.
 */
void NcJumpValidator(struct NcValidatorState* state,
                     struct NcInstIter* iter,
                     struct JumpSets* jump_sets);

/* Compares the collected actual jumps and the set of possible jump points,
 * and reports any descrepancies that don't follow NACL rules.
 */
void NcJumpValidatorSummarize(FILE* file,
                              struct NcValidatorState* state,
                              struct JumpSets* jump_sets);

/* Cleans up memory used by the jump validator. */
void NcJumpValidatorDestroy(struct NcValidatorState* state,
                            struct JumpSets* jump_sets);

/* Record that there is an explicit jump from the from_address to the
 * to_address, for the validation defined by the validator state.
 */
void NcAddJump(struct NcValidatorState* state,
               PcAddress from_address,
               PcAddress to_address);

/* Record that the given instruciton can't be a possible target of a jump,
 * because it appears as the non-first
 * instruciton in a NACL pattern. This should be called on all such non-first
 * instructions (for NACL patterns) so that the instuction sequence is
 * checked to be atomic.
 */
void NcMarkInstructionJumpIllegal(struct NcValidatorState* state,
                                  struct NcInstState* inst);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_JUMPS_H__ */
