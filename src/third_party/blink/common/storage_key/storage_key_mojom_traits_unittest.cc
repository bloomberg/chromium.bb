// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key_mojom_traits.h"

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/storage_key/ancestor_chain_bit.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace mojom {
class StorageKey;
}  // namespace mojom

namespace blink {
namespace {

TEST(StorageKeyMojomTraitsTest, SerializeAndDeserialize) {
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        features::kThirdPartyStoragePartitioning, toggle);
    StorageKey test_keys[] = {
        StorageKey(url::Origin::Create(GURL("https://example.com"))),
        StorageKey(url::Origin::Create(GURL("http://example.com"))),
        StorageKey(url::Origin::Create(GURL("https://example.test"))),
        StorageKey(url::Origin::Create(GURL("https://sub.example.com"))),
        StorageKey(url::Origin::Create(GURL("http://sub2.example.com"))),
        StorageKey(url::Origin::Create(GURL("https://example.com")),
                   url::Origin::Create(GURL("https://example.com"))),
        StorageKey(url::Origin::Create(GURL("http://example.com")),
                   url::Origin::Create(GURL("https://example2.com"))),
        StorageKey(url::Origin::Create(GURL("https://example.test")),
                   url::Origin::Create(GURL("https://example.com"))),
        StorageKey(url::Origin::Create(GURL("https://sub.example.com")),
                   url::Origin::Create(GURL("https://example2.com"))),
        StorageKey(url::Origin::Create(GURL("http://sub2.example.com")),
                   url::Origin::Create(GURL("https://example.com"))),
        StorageKey(url::Origin()),
        StorageKey::CreateWithNonce(
            url::Origin::Create(GURL("https://.example.com")),
            base::UnguessableToken::Create()),
        StorageKey::CreateWithNonce(url::Origin(),
                                    base::UnguessableToken::Create()),
        StorageKey::CreateWithOptionalNonce(
            url::Origin::Create(GURL("http://sub2.example.com")),
            net::SchemefulSite(
                url::Origin::Create(GURL("https://example.com"))),
            nullptr, blink::mojom::AncestorChainBit::kCrossSite)};

    for (auto& original : test_keys) {
      StorageKey copied;
      EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::StorageKey>(
          original, copied));
      EXPECT_EQ(original, copied);

      // Ensure the comparison works if `kThirdPartyStoragePartitioning` is
      // force enabled. This verifies `top_level_site_` and
      // `ancestor_chain_bit_`.
      EXPECT_EQ(original.CopyWithForceEnabledThirdPartyStoragePartitioning(),
                copied.CopyWithForceEnabledThirdPartyStoragePartitioning());
    }
  }
}

}  // namespace
}  // namespace blink
