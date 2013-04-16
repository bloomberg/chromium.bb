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

/*
 * file_name is not guaranteed to be a null-terminated string, its length is
 * explicitly specified by file_name_length.  By convention, if file_name
 * happens to be a null-terminated string, file_name_length equals
 * strlen(file_name).  In other words, the terminating null character is not
 * part of the name.
 */
struct NaClValidationMetadata {
  NaClCodeIdentityType identity_type;
  int64_t code_offset;
  const char* file_name;
  size_t file_name_length;
  uint64_t device_id;
  uint64_t file_id;
  int64_t file_size;
  time_t mtime;
  time_t ctime;
};

/*
 * Note: ownership of file_name is not transfered, the caller is still
 * responsible for managing the string's lifetime.  The validator does not make
 * a copy of the metadata structure nor does it retain a reference, so managing
 * the lifetime of the string should be straightforward.
 */
extern void ConstructMetadataForFD(int file_desc,
                                   const char* file_name,
                                   size_t file_name_length,
                                   struct NaClValidationMetadata *metadata);

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_VALIDATION_METADATA_H__ */
