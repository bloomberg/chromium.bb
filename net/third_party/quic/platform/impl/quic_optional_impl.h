// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_OPTIONAL_IMPL_H_
#define NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_OPTIONAL_IMPL_H_

#include "base/optional.h"

namespace net {

template <typename T>
using QuicOptionalImpl = base::Optional<T>;

}  // namespace net

#endif  // NET_THIRD_PARTY_QUIC_PLATFORM_IMPL_QUIC_OPTIONAL_IMPL_H_
