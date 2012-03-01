/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_SEL_MAIN_CHROME_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_SEL_MAIN_CHROME_H_ 1

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"

EXTERN_C_BEGIN

struct NaClValidationCache;

/*
 * Register the integrated runtime (IRT) library file for use by
 * NaClMainForChromium().  This takes a file descriptor, even on
 * Windows (where file descriptors are emulated by the C runtime
 * library).
 */
void NaClSetIrtFileDesc(int fd);

void NaClSetValidationCache(struct NaClValidationCache *cache);

void NaClMainForChromium(int handle_count, const NaClHandle *handles,
                         int debug);

EXTERN_C_END

#endif
