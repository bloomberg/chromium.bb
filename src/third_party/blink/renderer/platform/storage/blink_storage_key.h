// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_H_

#include <iosfwd>

#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/network/blink_schemeful_site.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// This class represents the key by which DOM Storage keys its
// CachedStorageAreas.
// It is typemapped to blink.mojom.StorageKey, and should stay in sync with
// blink::StorageKey (third_party/blink/public/common/storage_key/storage_key.h)
class PLATFORM_EXPORT BlinkStorageKey {

 public:
  // Creates a BlinkStorageKey with a unique opaque origin and top-level site.
  BlinkStorageKey();

  // Creates a BlinkStorageKey with the given origin. `origin` must not be null.
  // `origin` can be opaque. This implicitly sets `top_level_site_` to the same
  // origin.
  // TODO(https://crbug.com/1271615): Remove or mark as test-only most of these
  // constructors and factory methods.
  explicit BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin);

  // Creates a BlinkStorageKey with the given origin and top-level site.
  // `origin` must not be null. `origin` can be opaque.
  BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin,
                  const BlinkSchemefulSite& top_level_site);

  // Creates a BlinkStorageKey with the given origin, top-level site and nonce.
  // `origin` must not be null. `origin` can be opaque.
  // `nonce` can be null to create a key without a nonce.
  BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin,
                  const BlinkSchemefulSite& top_level_site,
                  const base::UnguessableToken* nonce);

  // Creates a BlinkStorageKey with the given origin, top-level site and nonce.
  // `origin` must not be null. `origin` can be opaque.
  // `nonce` can be null to create a key without a nonce.
  // `ancestor_chain_bit` must not be null, if it cannot be determined, default
  // to kSameSite.
  BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin,
                  const BlinkSchemefulSite& top_level_site,
                  const base::UnguessableToken* nonce,
                  mojom::blink::AncestorChainBit ancestor_chain_bit);

  // Creates a BlinkStorageKey converting the given StorageKey `storage_key`.
  BlinkStorageKey(const blink::StorageKey& storage_key);

  // Converts this BlinkStorageKey into a StorageKey.
  operator blink::StorageKey() const;

  ~BlinkStorageKey() = default;

  BlinkStorageKey(const BlinkStorageKey& other) = default;
  BlinkStorageKey& operator=(const BlinkStorageKey& other) = default;
  BlinkStorageKey(BlinkStorageKey&& other) = default;
  BlinkStorageKey& operator=(BlinkStorageKey&& other) = default;

  static BlinkStorageKey CreateWithNonce(
      scoped_refptr<const SecurityOrigin> origin,
      const base::UnguessableToken& nonce);

  static BlinkStorageKey CreateFromStringForTesting(const WTF::String& origin);

  const scoped_refptr<const SecurityOrigin>& GetSecurityOrigin() const {
    return origin_;
  }

  const BlinkSchemefulSite& GetTopLevelSite() const { return top_level_site_; }

  const absl::optional<base::UnguessableToken>& GetNonce() const {
    return nonce_;
  }

  mojom::blink::AncestorChainBit GetAncestorChainBit() const {
    return ancestor_chain_bit_;
  }

  String ToDebugString() const;

 private:
  BlinkStorageKey(scoped_refptr<const SecurityOrigin> origin,
                  const base::UnguessableToken* nonce);

  scoped_refptr<const SecurityOrigin> origin_;
  BlinkSchemefulSite top_level_site_;
  absl::optional<base::UnguessableToken> nonce_;
  mojom::blink::AncestorChainBit ancestor_chain_bit_;
};

PLATFORM_EXPORT
bool operator==(const BlinkStorageKey&, const BlinkStorageKey&);
PLATFORM_EXPORT
bool operator!=(const BlinkStorageKey&, const BlinkStorageKey&);
PLATFORM_EXPORT
std::ostream& operator<<(std::ostream&, const BlinkStorageKey&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_STORAGE_BLINK_STORAGE_KEY_H_
