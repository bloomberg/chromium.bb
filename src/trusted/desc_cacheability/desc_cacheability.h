/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_CACHEABILITY_DESC_CACHEABILITY_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_CACHEABILITY_DESC_CACHEABILITY_H_

#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

struct NaClDesc;
struct NaClFileToken;
struct NaClValidationCache;

/*
 * NaClExchangeFileTokenForMappableDesc checks the |file_token| with
 * the |validation_cache| and, if the file token is valid, i.e., for a
 * file that should be safe to map (immutable for the duration of our
 * execution), returns a NaClDesc for that file that should be used
 * for the memory mapping.
 */
struct NaClDesc *NaClExchangeFileTokenForMappableDesc(
    struct NaClFileToken *file_token,
    struct NaClValidationCache *validation_cache);

/*
 * NaClReplaceDescIfValidationCacheAssertsMappable checks
 * |*desc_in_out| to see if it contains a NaClFileToken as metadata,
 * and if so, queries the |validation_cache| with it. If the
 * validation cache says that this is a "safe" file by returning what
 * should be an equivalent file descriptor (in the Chrome embedding,
 * via a separate, trusted communication link) the |*desc_in_out|
 * reference count is decremented and then the NaClDesc pointer is
 * replaced with one that wraps the validator-supplied file
 * descriptor.
 */
void NaClReplaceDescIfValidationCacheAssertsMappable(
    struct NaClDesc **desc_in_out,
    struct NaClValidationCache *validation_cache);

EXTERN_C_END

#endif
