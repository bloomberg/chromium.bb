// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_CONTAINERS_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_CONTAINERS_IMPL_H_

#include <vector>

#include "base/containers/flat_set.h"

namespace quic {

// TODO(wub): Switch to absl::InlinedVector once it is allowed.
template <typename T, size_t N, typename A = std::allocator<T>>
using QuicInlinedVectorImpl = std::vector<T, A>;

template <typename Key, typename Compare>
using QuicSmallOrderedSetImpl = base::flat_set<Key, Compare>;

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_CONTAINERS_IMPL_H_
