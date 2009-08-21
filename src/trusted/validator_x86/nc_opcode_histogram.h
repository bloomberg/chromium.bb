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
 * Collects histogram information as a validator function.
 *
 * Note: The following functions are used to define a validator function
 * for collecting this information. See header file ncvalidator_iter.h
 * for more information on how to register these functions as a validator
 * function.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_OPCODE_HISTOGRAPH_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_OPCODE_HISTOGRAPH_H__

#include <stdio.h>

/* Defines a validator state. */
struct NcValidatorState;

/* Defines an instruction iterator that processes a code segment. */
struct NcInstIter;

/* Defines a data structure that holds data defining the opcode histogram
 * being collected.
 */
struct OpcodeHistogram;

/* Creates memory to hold an opcode histogram. */
struct OpcodeHistogram* NcOpcodeHistogramMemoryCreate(
    struct NcValidatorState* state);

/* Destroys memory holding an opcode histogram. */
void NcOpcodeHistogramMemoryDestroy(struct NcValidatorState* state,
                                    struct OpcodeHistogram* histogram);

/* Validator function to record histgram value for current instruction
 * in instruction iterator.
 */
void NcOpcodeHistogramRecord(struct NcValidatorState* state,
                             struct NcInstIter* iter,
                             struct OpcodeHistogram* histogram);

/* Validator print function to print out collected histogram. */
void NcOpcodeHistogramPrintStats(FILE* f,
                                 struct NcValidatorState* state,
                                 struct OpcodeHistogram* histogram);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_OPCODE_HISTOGRAPH_H__ */
