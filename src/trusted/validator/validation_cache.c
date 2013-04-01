/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator/validation_cache.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/validator/validation_metadata.h"

#define ADD_LITERAL(cache, query, data) \
  ((cache)->AddData((query), (uint8_t*)&(data), sizeof(data)))

void AddCodeIdentity(uint8_t *data,
                     size_t size,
                     const struct NaClValidationMetadata *metadata,
                     struct NaClValidationCache *cache,
                     void *query) {
  NaClCodeIdentityType identity_type;
  if (NULL != metadata) {
    identity_type = metadata->identity_type;
  } else {
    /* Fallback: identity unknown, treat it as anonymous data. */
    identity_type = NaClCodeIdentityData;
  }
  CHECK(identity_type < NaClCodeIdentityMax);

  /*
   * Explicitly record the type of identity being used to prevent attacks
   * that confuse the payload of different identity types.
   */
  ADD_LITERAL(cache, query, identity_type);

  if (identity_type == NaClCodeIdentityFile) {
    /* Sanity checks. */
    CHECK(metadata->file_name);
    CHECK(metadata->file_name_length);
    CHECK(metadata->code_offset + (int64_t) size <= metadata->file_size);

    /* The location of the code in the file. */
    ADD_LITERAL(cache, query, metadata->code_offset);
    ADD_LITERAL(cache, query, size);

    /* The identity of the file. */
    cache->AddData(query, (uint8_t *) metadata->file_name,
                   metadata->file_name_length);
    ADD_LITERAL(cache, query, metadata->file_size);
    ADD_LITERAL(cache, query, metadata->mtime);
  } else {
    /* Hash all the code. */
    cache->AddData(query, data, size);
  }
}
