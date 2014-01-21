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
 * NaClDescSetFileToken associates the contents of |token| with |desc|
 * as NaClDesc metadata.  Any other metadata previously associated
 * with |desc| is released / destroyed.  Returns non-zero value for
 * success.  (Boolean function.)
 */
int NaClDescSetFileToken(struct NaClDesc *desc,
                         struct NaClFileToken const *token);

/*
 * NaClDescGetFileToken checks if there is metadata associated with
 * |desc| that contains a previously serialized NaClFileToken object.
 * If so, it writes the NaClFileToken pointed to by |out_token| with
 * the file token, and returns a non-zero value.  (Boolean function.)
 */
int NaClDescGetFileToken(struct NaClDesc *desc,
                         struct NaClFileToken *out_token);

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
