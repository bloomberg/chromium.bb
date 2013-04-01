/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_VALIDATION_METADATA_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_VALIDATION_METADATA_H__

#include <stddef.h>
#include <time.h>

#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

/*
 * Note: this values in this enum are written to the cache, so changing them
 * will implicitly invalidate cache entries.
 */
typedef enum NaClCodeIdentityType {
  NaClCodeIdentityData = 0,
  NaClCodeIdentityFile = 1,
  NaClCodeIdentityMax
} NaClCodeIdentityType;

struct NaClValidationMetadata {
  NaClCodeIdentityType identity_type;
  int64_t code_offset;
  const char* file_name;
  size_t file_name_length;
  int64_t file_size;
  time_t mtime;
};

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_VALIDATION_METADATA_H__ */
