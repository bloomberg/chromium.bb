// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_UINT128_H_
#define NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_UINT128_H_

#include "net/third_party/quic/platform/impl/quic_uint128_impl.h"

namespace net {

using QuicUint128 = QuicUint128Impl;
#define MakeQuicUint128(hi, lo) MakeQuicUint128Impl(hi, lo)
#define QuicUint128Low64(x) QuicUint128Low64Impl(x)
#define QuicUint128High64(x) QuicUint128High64Impl(x)

}  // namespace net

#endif  // NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_UINT128_H_
