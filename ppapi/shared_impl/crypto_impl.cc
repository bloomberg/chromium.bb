// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/crypto_impl.h"

#include "base/rand_util.h"

namespace pp {
namespace shared_impl {

// static
void CryptoImpl::GetRandomBytes(char* buffer, uint32_t num_bytes) {
  // Note: this is a copy of WebKitClientImpl::cryptographicallyRandomValues.
  uint64 bytes = 0;
  for (uint32_t i = 0; i < num_bytes; ++i) {
    uint32_t offset = i % sizeof(bytes);
    if (!offset)
      bytes = base::RandUint64();
    buffer[i] = reinterpret_cast<char*>(&bytes)[offset];
  }
}

}  // namespace shared_impl
}  // namespace pp
