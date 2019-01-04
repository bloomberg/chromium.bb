// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef NET_THIRD_PARTY_SPDY_PLATFORM_IMPL_SPDY_CONTAINERS_IMPL_H_
#define NET_THIRD_PARTY_SPDY_PLATFORM_IMPL_SPDY_CONTAINERS_IMPL_H_

#include <unordered_map>
#include <unordered_set>

namespace spdy {

template <typename KeyType>
using SpdyHashImpl = std::hash<KeyType>;

template <typename KeyType, typename ValueType, typename Hash>
using SpdyHashMapImpl = std::unordered_map<KeyType, ValueType, Hash>;

template <typename ElementType, typename Hasher, typename Eq>
using SpdyHashSetImpl = std::unordered_set<ElementType, Hasher, Eq>;

}  // namespace spdy

#endif  // NET_THIRD_PARTY_SPDY_PLATFORM_IMPL_SPDY_CONTAINERS_IMPL_H_
