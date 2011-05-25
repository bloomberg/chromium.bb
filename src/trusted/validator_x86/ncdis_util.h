/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncdis.c - disassemble using NaCl decoder.
 * Mostly for testing.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDIS_UTIL_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDIS_UTIL_H_

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include <stdio.h>
#include "native_client/src/trusted/validator_x86/ncdecode.h"

/*
 * Print out the instruction stored in the given decoder state.
 */
extern void PrintInst(const NCDecoderInst *dinst, FILE* fp);

/*
 * Run the decoder and print out the decoded instructions.
 *
 * Parameters are:
 *   mbase - The beging of the memory segment to decode.
 *   vbase - The (virtual) base address of the memory segment.
 *   sz - The number of bytes in the memory segment.
 *   vstate - validator state (or NULL) to use with callbacks.
 */
extern void NCDecodeSegment(uint8_t* mbase, NaClPcAddress vbase,
                            NaClMemorySize sz);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDIS_UTIL_H_ */
