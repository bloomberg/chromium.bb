// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PreloadKey_h
#define PreloadKey_h

#include "platform/loader/fetch/Resource.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KURLHash.h"
#include "platform/wtf/HashTraits.h"

namespace blink {

// PreloadKey is a key type of the preloads map in a fetch group (a.k.a.
// blink::ResourceFetcher).
struct PreloadKey final {
 public:
  struct Hash {
    STATIC_ONLY(Hash);

   public:
    static unsigned GetHash(const PreloadKey& key) {
      return KURLHash::GetHash(key.url);
    }
    static bool Equal(const PreloadKey& x, const PreloadKey& y) {
      return x == y;
    }
    static constexpr bool safe_to_compare_to_empty_or_deleted = false;
  };

  PreloadKey() = default;
  PreloadKey(WTF::HashTableDeletedValueType)
      : url(WTF::kHashTableDeletedValue) {}
  PreloadKey(const KURL& url, Resource::Type type)
      : url(RemoveFragmentFromUrl(url)), type(type) {}

  bool IsHashTableDeletedValue() const { return url.IsHashTableDeletedValue(); }

  bool operator==(const PreloadKey& x) const {
    return url == x.url && type == x.type;
  }

  static KURL RemoveFragmentFromUrl(const KURL& src) {
    if (!src.HasFragmentIdentifier())
      return src;
    KURL url = src;
    url.RemoveFragmentIdentifier();
    return url;
  }

  KURL url;
  Resource::Type type = Resource::kMainResource;
};

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::PreloadKey> {
  using Hash = blink::PreloadKey::Hash;
};

template <>
struct HashTraits<blink::PreloadKey>
    : public SimpleClassHashTraits<blink::PreloadKey> {};

}  // namespace WTF

#endif  // PreloadKey_h
