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

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_ITER_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_ITER_H__

/*
 * ncvalidate_iter.h
 * Type declarations of the iterator state etc.
 *
 * This is the interface to the iterator form of the NaCL validator.
 * Basic usage:
 *   -- base is initial address of ELF file.
 *   -- limit is the size of the ELF file.
 *   -- maddr is the address to the memory of a section.
 *   -- vaddr is the starting virtual address associated with a section.
 *   -- size is the number of bytes in a section.
 *
 *   NcValidatorState* state = NcValidatorStateCreate(base, limit, 16);
 *   if (state == NULL) fail;
 *   for each section:
 *     NcValidateSegment(maddr, vaddr, size, state);
 *   if (!NcValidatesOk(state)) fail;
 *   NcValidatorStatePrintStats(stdout, state);
 *   NcValidatorStateDestroy(state);
 */

#include <stdio.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/validator_x86/nc_inst_iter.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state.h"

/* The model of a validator state. */
typedef struct NcValidatorState NcValidatorState;

/* Create a validator state to validate the ELF file with the given parameters.
 * Parameters.
 *   vbase - The virtual address for the contents of the ELF file.
 *   sz - The number of bytes in the ELF file.
 *   alignment: 16 or 32, specifying alignment.
 *   base_register - OperandKind defining value for base register (or
 *     RegUnknown if not defined).
 *   log_file - The file to log messages to.
 * Returns:
 *   A pointer to an initialized validator state if everything is ok, NULL
 *  otherwise.
 */
NcValidatorState* NcValidatorStateCreate(const PcAddress vbase,
                                         const MemorySize sz,
                                         const uint8_t alignment,
                                         const OperandKind base_register,
                                         FILE* log_file);

/* Returns the file that messages are being logged to. */
FILE* NcValidatorStateLogFile(NcValidatorState* state);

/* Validate a code segment.
 * Parameters:
 *   mbase - The address of the beginning of the code segment.
 *   vbase - The virtual address associated with the beginning of the code
 *       segment.
 *   sz - The number of bytes in the code segment.
 *   state - The validator state to use while validating.
 */
void NcValidateSegment(uint8_t* mbase,
                       PcAddress vbase,
                       MemorySize sz,
                       NcValidatorState* state);

/* Returns true if the validator hasn't found any problems with the validated
 * code segments.
 * Parameters:
 *   state - The validator state used to validate code segments.
 * Returns:
 *   true only if no problems have been found.
 */
Bool NcValidatesOk(NcValidatorState* state);

/* Print out statistics on the applied validation. */
void NcValidatorStatePrintStats(FILE* file, NcValidatorState* state);

/* Cleans up and returns the memory created by the corresponding
 * call to NcValidatorStateCreate.
 */
void NcValidatorStateDestroy(NcValidatorState* state);

/* Defines a function to create local memory to be used by a validator
 * function, should it need it.
 * Parameters:
 *   state - The state of the validator.
 * Returns:
 *   Allocated space for local data associated with a validator function.
 */
typedef void* (*NcValidatorMemoryCreate)(NcValidatorState* state);

/* Defines a validator function to be called on each instruction.
 * Parameters:
 *   state - The state of the validator.
 *   iter - The instruction iterator's current position in the segment.
 *   local_memory - Pointer to local memory generated by the corresponding
 *          NcValidatorMemoryCreate (or NULL if not specified).
 */
typedef void (*NcValidator)(NcValidatorState* state,
                            NcInstIter* iter,
                            void* local_memory);

/* Defines a statistics print routine for a validator function.
 * Parameters:
 *   file - The file to print statistics to.
 *   state - The state of the validator,
 *   local_memory - Pointer to local memory generated by the corresponding
 *          NcValidatorMemoryCreate (or NULL if not specified).
 */
typedef void (*NcValidatorPrintStats)(FILE* file,
                                      NcValidatorState* state,
                                      void* local_memory);

/* Defines a function to destroy local memory used by a validator function,
 * should it need to do so.
 * Parameters:
 *   state - The state of the validator.
 *   local_memory - Pointer to local memory generated by the corresponding
 *         NcValidatorMemoryCreate (or NULL if not specified).
 */
typedef void (*NcValidatorMemoryDestroy)(NcValidatorState* state,
                                         void* local_memory);

/* Registers a validator function to be called during validation.
 * Parameters are:
 *   validator - The validator function to register.
 *   print_stats - The print function to print statistics about the applied
 *     validator.
 *   memory_create - The function to call to generate local memory for
 *     the validator function (or NULL if no local memory is needed).
 *   memory_destroy - The function to call to reclaim local memory when
 *     the validator state is destroyed (or NULL if reclamation is not needed).
 */
void NcRegisterNcValidator(NcValidator validator,
                           NcValidatorPrintStats print_stats,
                           NcValidatorMemoryCreate memory_create,
                           NcValidatorMemoryDestroy memory_destroy);

/* Returns the local memory associated with the given validator function,
 * or NULL if no such memory exists. Allows validators to communicate
 * shared collected information.
 * Parameters:
 *   validator - The validator function's memory you want access to.
 *   state - The current state of the validator.
 * Returns:
 *   The local memory associated with the validator (or NULL  if no such
 *   validator is known).
 */
void* NcGetValidatorLocalMemory(NcValidator validator,
                                const NcValidatorState* state);

/* Prints out a validator message for the given level.
 * Parameters:
 *   level - The level of the message, as defined in nacl_log.h
 *   state - The validator state that detected the error.
 *   format - The format string of the message to print.
 *   ... - arguments to the format string.
 */
void NcValidatorMessage(int level,
                        NcValidatorState* state,
                        const char* format,
                        ...) ATTRIBUTE_FORMAT_PRINTF(3, 4);

/* Prints out a validator message for the given address.
 * Parameters:
 *   level - The level of the message, as defined in nacl_log.h
 *   state - The validator state that detected the error.
 *   addr - The address where the error occurred.
 *   format - The format string of the message to print.
 *   ... - arguments to the format string.
 */
void NcValidatorPcAddressMessage(int level,
                                 NcValidatorState* state,
                                 PcAddress addr,
                                 const char* format,
                                 ...) ATTRIBUTE_FORMAT_PRINTF(4, 5);

/* Prints out a validator message for the given instruction.
 * Parameters:
 *   level - The level of the message, as defined in nacl_log.h
 *   state - The validator state that detected the error.
 *   inst - The instruction that caused the vaidator error.
 *   format - The format string of the message to print.
 *   ... - arguments to the format string.
 */
void NcValidatorInstMessage(int level,
                            NcValidatorState* state,
                            NcInstState* inst,
                            const char* format,
                            ...) ATTRIBUTE_FORMAT_PRINTF(4, 5);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_ITER_H__ */
