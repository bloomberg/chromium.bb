/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_VALIDATOR_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_VALIDATOR_H_

#include "native_client/src/trusted/validator_ragel/unreviewed/decoder.h"

EXTERN_C_BEGIN

typedef void (*process_validation_error_func) (const uint8_t *ptr,
                                               void *userdata);

int ValidateChunkAMD64(const uint8_t *data, size_t size,
                       const NaClCPUFeaturesX86 *cpu_features,
                       process_validation_error_func process_error,
                       void *userdata);

int ValidateChunkIA32(const uint8_t *data, size_t size,
                      const NaClCPUFeaturesX86 *cpu_features,
                      process_validation_error_func process_error,
                      void *userdata);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_VALIDATOR_H_ */
