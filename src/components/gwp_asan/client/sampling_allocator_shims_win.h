// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_ALLOCATOR_SHIMS_WIN_H_
#define COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_ALLOCATOR_SHIMS_WIN_H_

#include <stddef.h>
#include <windows.h>

#include "base/logging.h"

namespace gwp_asan {
namespace internal {

// Due to https://crbug.com/881352 the sampling allocator shims can not use
// base::ThreadLocalStorage to access the TLS. Instead, we use platform-specific
// TLS APIs.
//
// TODO(vtsyrklevich): This implementation is identical to code in the
// base::PoissonAllocationSampler, see if they can share code.
using TLSKey = DWORD;

void TLSInit(TLSKey* key) {
  *key = ::TlsAlloc();
  CHECK_NE(TLS_OUT_OF_INDEXES, *key);
}

size_t TLSGetValue(const TLSKey& key) {
  return reinterpret_cast<size_t>(::TlsGetValue(key));
}

void TLSSetValue(const TLSKey& key, size_t value) {
  ::TlsSetValue(key, reinterpret_cast<LPVOID>(value));
}

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CLIENT_SAMPLING_ALLOCATOR_SHIMS_WIN_H_
