/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_REG_SFI_NC_JUMPS_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_REG_SFI_NC_JUMPS_H__

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
#include "native_client/src/trusted/validator/types_memory_model.h"

/* The model of a validator state. */
struct NaClValidatorState;

/* The model of an iterator through instructions in a code segment. */
struct NaClInstIter;

/* The model of a parsed instruction. */
struct NaClInstState;

/* Implements the possible/actual sets of jumps. */
struct NaClJumpSets;

/* When true, changes the behaviour of NcAddJump to use mask 0xFF for
 * indirect jumps (which is a nop). This allows performance tests for
 * compiled libraries without having to hand tweak the source code.
 */
extern Bool NACL_FLAGS_identity_mask;

/* Creates jump sets to track the set of possible and actual (explicit)
 * address.
 */
struct NaClJumpSets* NaClJumpValidatorCreate(struct NaClValidatorState* state);

/* Collects information on instruction addresses, and where explicit jumps
 * go to.
 */
void NaClJumpValidator(struct NaClValidatorState* state,
                       struct NaClInstIter* iter,
                       struct NaClJumpSets* jump_sets);

/* Don't record anything but the instruction address, in order to validate
 * basic block alignment at the end of validation.
 */
void NaClJumpValidatorRememberIpOnly(struct NaClValidatorState* state,
                       struct NaClInstIter* iter,
                       struct NaClJumpSets* jump_sets);

/* Compares the collected actual jumps and the set of possible jump points,
 * and reports any descrepancies that don't follow NACL rules.
 */
void NaClJumpValidatorSummarize(struct NaClValidatorState* state,
                                struct NaClInstIter* iter,
                                struct NaClJumpSets* jump_sets);

/* Cleans up memory used by the jump validator. */
void NaClJumpValidatorDestroy(struct NaClValidatorState* state,
                              struct NaClJumpSets* jump_sets);

/* Record that the given instruciton can't be a possible target of a jump,
 * because it appears as the non-first
 * instruciton in a NACL pattern. This should be called on all such non-first
 * instructions (for NACL patterns) so that the instuction sequence is
 * checked to be atomic.
 */
void NaClMarkInstructionJumpIllegal(struct NaClValidatorState* state,
                                    struct NaClInstState* inst);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_REG_SFI_NC_JUMPS_H__ */
