/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCCOPYCODE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCCOPYCODE_H_

#include "native_client/src/trusted/validator_x86/types_memory_model.h"

/* copies code from src to dest in a thread safe way, returns 0 on success */
int NCCopyCode(uint8_t *dst, uint8_t *src, NaClPcAddress vbase,
                size_t sz, int bundle_size);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_COPYCODE_H_ */

