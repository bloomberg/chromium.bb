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
