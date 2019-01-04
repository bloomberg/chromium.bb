// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_SPDY_PLATFORM_API_SPDY_CONTAINERS_H_
#define NET_THIRD_PARTY_SPDY_PLATFORM_API_SPDY_CONTAINERS_H_

#include "net/third_party/spdy/platform/impl/spdy_containers_impl.h"

namespace spdy {

template <typename KeyType>
using SpdyHash = SpdyHashImpl<KeyType>;

// SpdyHashMap does not guarantee pointer stability.
template <typename KeyType,
          typename ValueType,
          typename Hash = SpdyHash<KeyType>>
using SpdyHashMap = SpdyHashMapImpl<KeyType, ValueType, Hash>;

// SpdyHashSet does not guarantee pointer stability.
template <typename ElementType, typename Hasher, typename Eq>
using SpdyHashSet = SpdyHashSetImpl<ElementType, Hasher, Eq>;

}  // namespace spdy

#endif  // NET_THIRD_PARTY_SPDY_PLATFORM_API_SPDY_CONTAINERS_H_
