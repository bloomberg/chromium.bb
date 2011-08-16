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

/* When true, use an instruction iterator instead of NCDecodeSegment.
 */
extern Bool NACL_FLAGS_use_iter;

/* When true (and NACL_FLAGS_use_iter is true), prints out the internal
 * representation of the disassembled instruction.
 */
extern Bool NACL_FLAGS_internal;

/* When true, use the validator decoder tables instead of the regular
 * decoder tables.
 */
extern Bool NACL_FLAGS_validator_decoder;

void NaClDisassembleSegment(uint8_t* mbase, NaClPcAddress vbase,
                            NaClMemorySize size);

#endif   /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCDIS_SEGMENTS_H_ */
