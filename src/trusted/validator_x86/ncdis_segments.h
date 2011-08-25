/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncdis_segments.h - Common routine for disassembling a block of code.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDIS_SEGMENTS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDIS_SEGMENTS_H_

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator/types_memory_model.h"

/* Flags that can be passed to the disassembler */
typedef enum NaClDisassembleFlag {
  /* Use the full decoder to disassemble the instruction. */
  NaClDisassembleFull,
  /* Use the dissassembler associated with the corresponding decoder. */
  NaClDisassembleValidatorDecoder,
  /* If additional internal information is available about the disassembled
   * instruction, print it also.
   */
  NaClDisassembleAddInternals,
} NaClDisassembleFlag;

/* Defines an integer to represent sets of possible disassembler flags. */
typedef uint8_t NaClDisassembleFlags;

/* Converts a NaClDisssembleFlag to the corresponding bit in
 * NaClDisassembleFlags.
 */
#define NACL_DISASSEMBLE_FLAG(x) (((NaClDisassembleFlags) 1) << (x))

/* Returns Bool flag defining if given flag is in the set of
 * disassemble flags.
 */
Bool NaClContainsDisassembleFlag(NaClDisassembleFlags flags,
                                 NaClDisassembleFlag flag);

/* Disassemble the code segment, following the rules specified by
 * the given set of flags.
 *
 * Parameters:
 *    mbase - Memory region containing code segment.
 *    vbase - PC address associated with first byte of memory region.
 *    size - Number of bytes in memory region.
 *    flags - Flags to use when decoding.
 */
void NaClDisassembleSegment(uint8_t* mbase, NaClPcAddress vbase,
                            NaClMemorySize size, NaClDisassembleFlags flags);

#endif   /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDIS_SEGMENTS_H_ */
