/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _VALIDATOR_X86_64_H_
#define _VALIDATOR_X86_64_H_

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*process_validation_error_func) (const uint8_t *ptr,
                                               void *userdata);

int ValidateChunkAMD64(const uint8_t *data, size_t size,
                       process_validation_error_func process_error,
                       void *userdata);

int ValidateChunkIA32(const uint8_t *data, size_t size,
                      process_validation_error_func process_error,
                      void *userdata);

#ifdef __cplusplus
}
#endif

#endif
