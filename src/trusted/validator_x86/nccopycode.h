/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCCOPYCODE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCCOPYCODE_H_

#include "native_client/src/trusted/validator/x86/types_memory_model.h"

#if NACL_TARGET_SUBARCH == 32
/* Copies code from src to dest in a thread safe way, returns 1 on success,
 * returns 0 on error. This will likely assert on error to avoid partially
 * copied code or undefined state.
 */
int NCCopyCode(uint8_t *dst, uint8_t *src, NaClPcAddress vbase,
                size_t sz, int bundle_size);

#elif NACL_TARGET_SUBARCH == 64
/* Copies code from src to dest in a thread safe way, returns 1 on success,
 * returns 0 on error. This will likely assert on error to avoid partially
 * copied code or undefined state.
 */
int NaClCopyCodeIter(uint8_t *dst, uint8_t *src,
                     NaClPcAddress vbase, size_t size);
#else
#error "Unknown Platform"
#endif

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_COPYCODE_H_ */
