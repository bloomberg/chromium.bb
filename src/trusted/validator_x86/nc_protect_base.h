/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_PROTECT_BASE_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_PROTECT_BASE_H__

#include <stdio.h>

/* nc_protect_base.h - For 64-bit mode, verifies that no instruction
 * changes the value of the base register.
 */

/*
 * Note: The function BaseRegisterValidator is used as a validator
 * function to be applied to a validated segment, as defined in
 * ncvalidate_iter.h.
 */

/* The model of a validator state. */
struct NcValidatorState;

/* The model of an iterator through instructions in a code segment. */
struct NcInstIter;

/* Defines a data structure that holds data local to function
 * the validator function NcBaseRegisterValidator.
 */
struct NcBaseRegisterLocals;

/* Create memory to hold local information for validator
 * NcBaseRegisterValidator.
 */
struct NcBaseRegisterLocals* NcBaseRegisterMemoryCreate(
    struct NcValidatorState* state);

/* Create memory to hold local information for validator
 * NcBaseRegisterValidator.
 */
void NcBaseRegisterMemoryDestroy(struct NcValidatorState*state,
                                 struct NcBaseRegisterLocals* locals);

/* Validator function to check that the base register is never set. */
void NcBaseRegisterValidator(struct NcValidatorState* state,
                             struct NcInstIter* iter,
                             struct NcBaseRegisterLocals* locals);

/* Validator summarization function. */
void NcBaseRegisterSummarize(FILE* f,
                             struct NcValidatorState* state,
                             struct NcBaseRegisterLocals* locals);


#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_PROTECT_BASE_H__ */
