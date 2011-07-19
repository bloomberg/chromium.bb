// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_CRYPTO_IMPL_H_
#define PPAPI_SHARED_IMPL_CRYPTO_IMPL_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_stdint.h"

namespace ppapi {

class CryptoImpl {
 public:
  static void GetRandomBytes(char* buffer, uint32_t num_bytes);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_CRYPTO_IMPL_H_
