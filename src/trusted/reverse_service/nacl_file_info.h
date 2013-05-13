/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_NACL_FILE_INFO_H__
#define NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_NACL_FILE_INFO_H__

#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

struct NaClFileInfo {
  int32_t desc;
  uint64_t nonce;
};

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_TRUSTED_REVERSE_SERVICE_NACL_FILE_INFO_H__ */
