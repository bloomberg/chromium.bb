/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_NACL_FILE_INFO_H_
#define NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_NACL_FILE_INFO_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

/*
 * NaClFileToken is a single-use nonce that the NaCl process can use to query
 * the browser process for trusted information about a file.  lo == 0 && hi == 0
 * indicates the token is invalid and no additional information is available.
 */
struct NaClFileToken {
  uint64_t lo;
  uint64_t hi;
};

struct NaClFileInfo {
  /* desc is either a Unix file descriptor or a Windows file handle. */
  int32_t desc;
  struct NaClFileToken file_token;
};

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_NACL_FILE_INFO_H_ */
