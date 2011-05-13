// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/crypto_impl.h"

#include "base/rand_util.h"

namespace ppapi {

// static
void CryptoImpl::GetRandomBytes(char* buffer, uint32_t num_bytes) {
  base::RandBytes(buffer, num_bytes);
}

}  // namespace ppapi
