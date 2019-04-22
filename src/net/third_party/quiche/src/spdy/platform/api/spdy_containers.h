// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_SPDY_PLATFORM_API_SPDY_CONTAINERS_H_
#define QUICHE_SPDY_PLATFORM_API_SPDY_CONTAINERS_H_

#include "net/third_party/quiche/src/spdy/platform/api/spdy_string_piece.h"
#include "net/spdy/platform/impl/spdy_containers_impl.h"

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

// A map which offers insertion-ordered iteration.
template <typename Key, typename Value, typename Hash = SpdyHash<Key>>
using SpdyLinkedHashMap = SpdyLinkedHashMapImpl<Key, Value, Hash>;

// A vector optimized for small sizes. Provides the same APIs as a std::vector.
template <typename T, size_t N, typename A = std::allocator<T>>
using SpdyInlinedVector = SpdyInlinedVectorImpl<T, N, A>;

using SpdyStringPieceHash = SpdyStringPieceHashImpl;

inline size_t SpdyHashStringPair(SpdyStringPiece a, SpdyStringPiece b) {
  return SpdyHashStringPairImpl(a, b);
}

}  // namespace spdy

#endif  // QUICHE_SPDY_PLATFORM_API_SPDY_CONTAINERS_H_
