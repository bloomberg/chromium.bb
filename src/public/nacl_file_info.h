/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_PUBLIC_NACL_FILE_INFO_H_
#define NATIVE_CLIENT_SRC_PUBLIC_NACL_FILE_INFO_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

struct NaClDesc;

/*
 * NaClFileToken is a single-use nonce that the NaCl process can use to query
 * the browser process for trusted information about a file.  This helps
 * establish that the file is known by the browser to be immutable and
 * suitable for file-identity-based validation caching.
 * lo == 0 && hi == 0 indicates the token is invalid and no additional
 * information is available (see NaClFileTokenIsValid()).
 */
struct NaClFileToken {
  uint64_t lo;
  uint64_t hi;
};

static INLINE int NaClFileTokenIsValid(struct NaClFileToken *file_token) {
  return !(file_token->lo == 0 && file_token->hi == 0);
}

struct NaClFileInfo {
  /* desc is a posix file descriptor */
  int32_t desc;
  struct NaClFileToken file_token;
};

/*
 * Creates a NaClDesc from the file descriptor/file handle and metadata
 * in |info|, and associates that desc with the metadata.
 */
struct NaClDesc *NaClDescIoFromFileInfo(struct NaClFileInfo info,
                                        int mode);


EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_PUBLIC_NACL_FILE_INFO_H_ */
