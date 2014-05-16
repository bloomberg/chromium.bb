/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_FILE_INFO_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_FILE_INFO_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/public/nacl_file_info.h"

EXTERN_C_BEGIN

struct NaClDesc;
struct NaClFileToken;

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

EXTERN_C_END

#endif
