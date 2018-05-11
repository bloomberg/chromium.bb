// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_PREFETCH_IMPL_H_
#define NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_PREFETCH_IMPL_H_

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace net {

inline void QuicPrefetchT0Impl(const void* addr) {
#if defined(__GNUC__)
  __builtin_prefetch(addr, 0, 3);
#elif defined(_MSC_VER)
  _mm_prefetch(reinterpret_cast<const char*>(addr), _MM_HINT_T0);
#endif
}

}  // namespace net

#endif  // NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_PREFETCH_IMPL_H_
