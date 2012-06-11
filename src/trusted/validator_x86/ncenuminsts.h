/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Helper routines for testing instructions. Used by private_tests/enuminsts. */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCEMNUMINSTS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCEMNUMINSTS_H_

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator/types_memory_model.h"

/* Parameterize the decoder state by architecture. */
#if NACL_TARGET_SUBARCH == 64
struct NaClInstState;
#define NaClInstStruct struct NaClInstState
#else
struct NCDecoderState;
#define NaClInstStruct struct NCDecoderState
#endif

/* Decodes the first instruction in the given buffer, assuming the
 * given vbase.
 * WARNING: This function is not thread safe. The resulting instruction
 * is only guaranteed to be defined until the next call to this function.
 * NOTE: Since this is only used by private_tests/enuminsts, which is a
 * singly threaded application, not being thread safe is ok.
 */
NaClInstStruct *NaClParseInst(uint8_t *ibytes, size_t isize,
                              const NaClPcAddress vbase);

/* Returns the number of bytes in the given (parsed) instruction. */
uint8_t NaClInstLength(NaClInstStruct *inst);

/* Returns the printed text for the given instruction (including parsed bytes)
 * as a (malloc allocated) char*.
 */
char* NaClInstToStr(NaClInstStruct *inst);

/* Return true if the instruction in num_bytes (of bytes) validates. */
Bool NaClValidateAnalyzeBytes(uint8_t *bytes,
                              NaClMemorySize num_bytes,
                              NaClPcAddress base);

/* Returns the name of the opcode for the given instruction
 * WARNING: This function is not thread safe. The resulting string
 * is only guaranteed to be defined until the next call to this function.
 * NOTE: Since this is only used by private_tests/enuminsts, which is a
 * singly threaded application, not being thread safe is ok.
 */
const char *NaClOpcodeName(NaClInstStruct *inst);

/* Returns true if the instruction was properly decoded by the NaCl
 * disassembler.
 */
Bool NaClInstDecodesCorrectly(NaClInstStruct *inst);

/* Returns true if the instruction, defined by SIZE bytes
 * in the given base, validates as a legal NACL instruction,
 * to the best we can tell without surrounding context. That is,
 * we only do static checks on the instruction, and do not check
 * preconditions/postconditions of the instruction.
 *
 * Parameters are:
 *    mbase - The memory containing the bytes of the instruction to decode.
 *    size - The number of bytes defining the instruction.
 *    vbase - The virtual address associated with the instruction.
 *    inst - The decoded instruction from NaClParseInst above.
 */
Bool NaClInstValidates(uint8_t* mbase,
                       uint8_t size,
                       NaClPcAddress vbase,
                       NaClInstStruct *inst);

/* Runs the validator on the instruction sequence in the given code segment,
 * and returns true if it validates. Assumes that the supporting architecture
 * supports all known x86 architectures.
 *
 * Parameters are:
 *    mbase - The memory containing the bytes of the instruction to decode.
 *    size - The number of bytes defining the instruction.
 *    vbase - The virtual address associated with the instruction.
 */
Bool NaClSegmentValidates(uint8_t* mbase,
                          size_t size,
                          NaClPcAddress vbase);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCEMNUMINSTS_H_ */
