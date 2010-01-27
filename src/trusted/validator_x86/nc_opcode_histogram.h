/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
