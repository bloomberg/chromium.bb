/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Helper routines for testing instructions. */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCEMNUMINSTS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCEMNUMINSTS_H_

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator/types_memory_model.h"

/* Returns true if the instruction, defined by SIZE bytes
 * in the given base, is a legal NACL instruction (other than
 * SFI issues).
 *
 * Parameters are:
 *    mbase - The memory containing the bytes of the instruction to decode.
 *    size - The number of bytes defining the instruction.
 *    vbase - The virtual address associated with the instruction.
 */
Bool NaClInstructionIsLegal(uint8_t* mbase,
                            uint8_t size,
                            NaClPcAddress vbase);


/* Changes opcode encodings to match those used by xed. */
void NaClChangeOpcodesToXedsModel();

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCEMNUMINSTS_H_ */
