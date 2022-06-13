// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/storage_key/storage_key.h"

#include <utility>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// Opaqueness here is used as a way of checking for "correctly constructed" in
// most tests.
//
// Why not call it IsValid()? Because some tests actually want to check for
// opaque origins.
bool IsOpaque(const blink::StorageKey& key) {
  return key.origin().opaque() && key.top_level_site().opaque();
}

}  // namespace

namespace blink {

// Test when a constructed StorageKey object should be considered valid/opaque.
TEST(StorageKeyTest, ConstructionValidity) {
  StorageKey empty = StorageKey();
  EXPECT_TRUE(IsOpaque(empty));
  // These cases will have the same origin for both `origin` and
  // `top_level_site`.
  url::Origin valid_origin = url::Origin::Create(GURL("https://example.com"));
  StorageKey valid = StorageKey(valid_origin);
  EXPECT_FALSE(IsOpaque(valid));

  url::Origin invalid_origin =
      url::Origin::Create(GURL("I'm not a valid URL."));
  StorageKey invalid = StorageKey(invalid_origin);
  EXPECT_TRUE(IsOpaque(invalid));
}

// Test that StorageKeys are/aren't equivalent as expected when storage
// partitioning is disabled.
TEST(StorageKeyTest, Equivalence) {
  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));
  url::Origin origin3 = url::Origin();
  url::Origin origin4 =
      url::Origin();  // Creates a different opaque origin than origin3.

  base::UnguessableToken nonce1 = base::UnguessableToken::Create();
  base::UnguessableToken nonce2 = base::UnguessableToken::Create();

  StorageKey key1_origin1 = StorageKey(origin1);
  StorageKey key2_origin1 = StorageKey(origin1);
  StorageKey key3_origin2 = StorageKey(origin2);

  StorageKey key4_origin3 = StorageKey(origin3);
  StorageKey key5_origin3 = StorageKey(origin3);
  StorageKey key6_origin4 = StorageKey(origin4);
  EXPECT_TRUE(IsOpaque(key4_origin3));
  EXPECT_TRUE(IsOpaque(key5_origin3));
  EXPECT_TRUE(IsOpaque(key6_origin4));

  StorageKey key7_origin1_nonce1 = StorageKey::CreateWithNonce(origin1, nonce1);
  StorageKey key8_origin1_nonce1 = StorageKey::CreateWithNonce(origin1, nonce1);
  StorageKey key9_origin1_nonce2 = StorageKey::CreateWithNonce(origin1, nonce2);
  StorageKey key10_origin2_nonce1 =
      StorageKey::CreateWithNonce(origin2, nonce1);

  StorageKey key11_origin1_origin2 = StorageKey(origin1, origin2);
  StorageKey key12_origin2_origin1 = StorageKey(origin2, origin1);

  // All are equivalent to themselves
  EXPECT_EQ(key1_origin1, key1_origin1);
  EXPECT_EQ(key2_origin1, key2_origin1);
  EXPECT_EQ(key3_origin2, key3_origin2);
  EXPECT_EQ(key4_origin3, key4_origin3);
  EXPECT_EQ(key5_origin3, key5_origin3);
  EXPECT_EQ(key6_origin4, key6_origin4);
  EXPECT_EQ(key7_origin1_nonce1, key7_origin1_nonce1);
  EXPECT_EQ(key8_origin1_nonce1, key8_origin1_nonce1);
  EXPECT_EQ(key9_origin1_nonce2, key9_origin1_nonce2);
  EXPECT_EQ(key10_origin2_nonce1, key10_origin2_nonce1);

  // StorageKeys created from the same origins are equivalent.
  EXPECT_EQ(key1_origin1, key2_origin1);
  EXPECT_EQ(key4_origin3, key5_origin3);

  // StorageKeys created from different origins are not equivalent.
  EXPECT_NE(key1_origin1, key3_origin2);
  EXPECT_NE(key4_origin3, key6_origin4);
  EXPECT_NE(key7_origin1_nonce1, key10_origin2_nonce1);

  // StorageKeys created from the same origins and nonces are equivalent.
  EXPECT_EQ(key7_origin1_nonce1, key8_origin1_nonce1);

  // StorageKeys created from different nonces are not equivalent.
  EXPECT_NE(key7_origin1_nonce1, key9_origin1_nonce2);

  // StorageKeys created from different origin and different nonces are not
  // equivalent.
  EXPECT_NE(key9_origin1_nonce2, key10_origin2_nonce1);

  // The top-level site doesn't factor in when storage partitioning is disabled.
  EXPECT_EQ(key11_origin1_origin2, key1_origin1);
  EXPECT_EQ(key12_origin2_origin1, key3_origin2);
}

// Test that StorageKeys are/aren't equivalent as expected when storage
// partitioning is enabled.
TEST(StorageKeyTest, EquivalencePartitioned) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kThirdPartyStoragePartitioning);

  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));

  // Keys should only match when both the origin and top-level site are the
  // same. Such as keys made from the single argument constructor and keys
  // created by the two argument constructor (when both arguments are the same
  // origin).

  StorageKey OneArgKey_origin1 = StorageKey(origin1);
  StorageKey OneArgKey_origin2 = StorageKey(origin2);

  StorageKey TwoArgKey_origin1_origin1 = StorageKey(origin1, origin1);
  StorageKey TwoArgKey_origin2_origin2 = StorageKey(origin2, origin2);

  EXPECT_EQ(OneArgKey_origin1, TwoArgKey_origin1_origin1);
  EXPECT_EQ(OneArgKey_origin2, TwoArgKey_origin2_origin2);

  // And when the two argument constructor gets different values
  StorageKey TwoArgKey1_origin1_origin2 = StorageKey(origin1, origin2);
  StorageKey TwoArgKey2_origin1_origin2 = StorageKey(origin1, origin2);
  StorageKey TwoArgKey_origin2_origin1 = StorageKey(origin2, origin1);

  EXPECT_EQ(TwoArgKey1_origin1_origin2, TwoArgKey2_origin1_origin2);

  // Otherwise they're not equivalent.
  EXPECT_NE(TwoArgKey1_origin1_origin2, TwoArgKey_origin1_origin1);
  EXPECT_NE(TwoArgKey_origin2_origin1, TwoArgKey_origin2_origin2);
  EXPECT_NE(TwoArgKey1_origin1_origin2, TwoArgKey_origin2_origin1);
}

// Test that StorageKeys Serialize to the expected value with partitioning
// enabled and disabled.
TEST(StorageKeyTest, SerializeFirstParty) {
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        features::kThirdPartyStoragePartitioning, toggle);

    struct {
      const char* origin_str;
      const char* expected_serialization;
    } kTestCases[] = {
        {"https://example.com/", "https://example.com/"},
        // Trailing slash is added.
        {"https://example.com", "https://example.com/"},
        // Subdomains are preserved.
        {"http://sub.test.example/", "http://sub.test.example/"},
        // file: origins all serialize to "file:///"
        {"file:///", "file:///"},
        {"file:///foo/bar", "file:///"},
        {"file://example.fileshare.com/foo/bar", "file:///"},
    };

    for (const auto& test : kTestCases) {
      SCOPED_TRACE(test.origin_str);
      StorageKey key(url::Origin::Create(GURL(test.origin_str)));
      EXPECT_EQ(test.expected_serialization, key.Serialize());
    }
  }
}

TEST(StorageKeyTest, SerializeFirstPartyForLocalStorage) {
  for (const bool toggle : {false, true}) {
    base::test::ScopedFeatureList scope_feature_list;
    scope_feature_list.InitWithFeatureState(
        features::kThirdPartyStoragePartitioning, toggle);

    struct {
      const char* origin_str;
      const char* expected_serialization;
    } kTestCases[] = {
        // Trailing slash is removed.
        {"https://example.com/", "https://example.com"},
        {"https://example.com", "https://example.com"},
        // Subdomains are preserved.
        {"http://sub.test.example/", "http://sub.test.example"},
        // file: origins all serialize to "file://"
        {"file://", "file://"},
        {"file:///foo/bar", "file://"},
        {"file://example.fileshare.com/foo/bar", "file://"},
    };

    for (const auto& test : kTestCases) {
      SCOPED_TRACE(test.origin_str);
      StorageKey key(url::Origin::Create(GURL(test.origin_str)));
      EXPECT_EQ(test.expected_serialization, key.SerializeForLocalStorage());
    }
  }
}

// Tests that the top-level site is correctly serialized for service workers
// when kThirdPartyStoragePartitioning is enabled. This is expected to be the
// same for localStorage.
TEST(StorageKeyTest, SerializePartitioned) {
  base::test::ScopedFeatureList scope_feature_list;
  scope_feature_list.InitAndEnableFeature(
      features::kThirdPartyStoragePartitioning);

  net::SchemefulSite SiteExample(GURL("https://example.com"));
  net::SchemefulSite SiteTest(GURL("https://test.example"));

  struct {
    const std::pair<const char*, const net::SchemefulSite&>
        origin_and_top_level_site;
    const char* expected_serialization;
  } kTestCases[] = {
      // 3p context case
      {{"https://example.com/", SiteTest},
       "https://example.com/^0https://test.example"},
      {{"https://sub.test.example/", SiteExample},
       "https://sub.test.example/^0https://example.com"},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin_and_top_level_site.first);
    const url::Origin origin =
        url::Origin::Create(GURL(test.origin_and_top_level_site.first));
    const net::SchemefulSite& site = test.origin_and_top_level_site.second;
    StorageKey key(origin, site);
    EXPECT_EQ(test.expected_serialization, key.Serialize());
    EXPECT_EQ(test.expected_serialization, key.SerializeForLocalStorage());
  }
}

TEST(StorageKeyTest, SerializeNonce) {
  struct {
    const std::pair<const char*, const base::UnguessableToken> origin_and_nonce;
    const char* expected_serialization;
  } kTestCases[] = {
      {{"https://example.com/",
        base::UnguessableToken::Deserialize(12345ULL, 67890ULL)},
       "https://example.com/^112345^267890"},
      {{"https://test.example",
        base::UnguessableToken::Deserialize(22222ULL, 99999ULL)},
       "https://test.example/^122222^299999"},
      {{"https://sub.test.example/",
        base::UnguessableToken::Deserialize(9876ULL, 54321ULL)},
       "https://sub.test.example/^19876^254321"},
      {{"https://other.example/",
        base::UnguessableToken::Deserialize(3735928559ULL, 110521ULL)},
       "https://other.example/^13735928559^2110521"},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin_and_nonce.first);
    const url::Origin origin =
        url::Origin::Create(GURL(test.origin_and_nonce.first));
    const base::UnguessableToken& nonce = test.origin_and_nonce.second;
    StorageKey key = StorageKey::CreateWithNonce(origin, nonce);
    EXPECT_EQ(test.expected_serialization, key.Serialize());
  }
}

// Test that deserialized StorageKeys are valid/opaque as expected.
TEST(StorageKeyTest, Deserialize) {
  std::string example = "https://example.com/";
  // Localstorage serialization doesn't have a trailing slash
  std::string example_local_storage = "https://example.com";
  std::string test = "https://test.example/";
  std::string wrong = "I'm not a valid URL.";

  // TODO(https://crbug.com/1199077): Refactor this test to use a for loop
  // across an array of structs. See SerializeNonce for an example.
  absl::optional<StorageKey> key1 = StorageKey::Deserialize(example);
  absl::optional<StorageKey> key1a =
      StorageKey::Deserialize(example_local_storage);
  absl::optional<StorageKey> key2 = StorageKey::Deserialize(test);
  absl::optional<StorageKey> key3 = StorageKey::Deserialize(wrong);
  absl::optional<StorageKey> key4 = StorageKey::Deserialize(std::string());

  ASSERT_TRUE(key1.has_value());
  EXPECT_FALSE(IsOpaque(*key1));
  ASSERT_TRUE(key1a.has_value());
  EXPECT_FALSE(IsOpaque(*key1a));
  ASSERT_TRUE(key2.has_value());
  EXPECT_FALSE(IsOpaque(*key2));
  EXPECT_FALSE(key3.has_value());
  EXPECT_FALSE(key4.has_value());

  std::string example_with_test = "https://example.com/^0https://test.example";
  std::string example_with_wrong_seperator_site =
      "https://example.com/^1https://test.example";
  std::string test_with_example = "https://test.example/^0https://example.com";
  std::string example_with_wrong = "https://example.com/^0I'm not a valid URL.";
  std::string wrong_with_example = "I'm not a valid URL.^0https://example.com";

  absl::optional<StorageKey> key5 = StorageKey::Deserialize(example_with_test);
  absl::optional<StorageKey> key5b =
      StorageKey::Deserialize(example_with_wrong_seperator_site);
  absl::optional<StorageKey> key6 = StorageKey::Deserialize(test_with_example);
  absl::optional<StorageKey> key7 = StorageKey::Deserialize(example_with_wrong);
  absl::optional<StorageKey> key8 = StorageKey::Deserialize(wrong_with_example);

  ASSERT_TRUE(key5.has_value());
  EXPECT_FALSE(IsOpaque(*key5));
  EXPECT_FALSE(key5b.has_value());
  ASSERT_TRUE(key6.has_value());
  EXPECT_FALSE(IsOpaque(*key6));
  EXPECT_FALSE(key7.has_value());
  EXPECT_FALSE(key8.has_value());

  std::string example_with_nonce = "https://example.com/^112345^267890";
  std::string example_with_wrong_seperator_nonce_1 =
      "https://example.com/^112345^167890";
  std::string example_with_wrong_seperator_nonce_2 =
      "https://example.com/^112345^967890";
  std::string example_with_wrong_nonce = "https://example.com/^1nota^2nonce";
  std::string wrong_with_nonce = "I'm not a valid URL.^112345^267890";

  absl::optional<StorageKey> key9 = StorageKey::Deserialize(example_with_nonce);
  absl::optional<StorageKey> key9b =
      StorageKey::Deserialize(example_with_wrong_seperator_nonce_1);
  absl::optional<StorageKey> key9c =
      StorageKey::Deserialize(example_with_wrong_seperator_nonce_2);
  absl::optional<StorageKey> key10 =
      StorageKey::Deserialize(example_with_wrong_nonce);
  absl::optional<StorageKey> key11 = StorageKey::Deserialize(wrong_with_nonce);

  ASSERT_TRUE(key9.has_value());
  EXPECT_FALSE(IsOpaque(*key9));
  EXPECT_FALSE(key9b.has_value());
  EXPECT_FALSE(key9c.has_value());
  EXPECT_FALSE(key10.has_value());
  EXPECT_FALSE(key11.has_value());

  std::string nonce_low_first = "https://example.com/^212345^167890";
  std::string malformed_3_carets = "https://example.com/^112345^267890^";
  std::string malformed_seperators_too_close = "https://example.com/^1^267890";
  std::string malformed_first_party =
      "https://www.example.com/^0https://example.com";

  absl::optional<StorageKey> key12 = StorageKey::Deserialize(nonce_low_first);
  absl::optional<StorageKey> key13 =
      StorageKey::Deserialize(malformed_3_carets);
  absl::optional<StorageKey> key14 =
      StorageKey::Deserialize(malformed_seperators_too_close);
  absl::optional<StorageKey> key15 =
      StorageKey::Deserialize(malformed_first_party);

  EXPECT_FALSE(key12.has_value());
  EXPECT_FALSE(key13.has_value());
  EXPECT_FALSE(key14.has_value());
  EXPECT_FALSE(key15.has_value());
}

// Test that string -> StorageKey test function performs as expected.
TEST(StorageKeyTest, CreateFromStringForTesting) {
  std::string example = "https://example.com/";
  std::string wrong = "I'm not a valid URL.";

  StorageKey key1 = StorageKey::CreateFromStringForTesting(example);
  StorageKey key2 = StorageKey::CreateFromStringForTesting(wrong);
  StorageKey key3 = StorageKey::CreateFromStringForTesting(std::string());

  EXPECT_FALSE(IsOpaque(key1));
  EXPECT_EQ(key1, StorageKey(url::Origin::Create(GURL(example))));
  EXPECT_TRUE(IsOpaque(key2));
  EXPECT_TRUE(IsOpaque(key3));
}

// Test that a StorageKey, constructed by deserializing another serialized
// StorageKey, is equivalent to the original.
TEST(StorageKeyTest, SerializeDeserialize) {
  const char* kTestCases[] = {"https://example.com", "https://sub.test.example",
                              "file://", "file://example.fileshare.com"};

  for (const char* test : kTestCases) {
    SCOPED_TRACE(test);
    url::Origin origin = url::Origin::Create(GURL(test));
    StorageKey key(origin);
    std::string key_string = key.Serialize();
    std::string key_string_for_local_storage = key.SerializeForLocalStorage();
    absl::optional<StorageKey> key_deserialized =
        StorageKey::Deserialize(key_string);
    absl::optional<StorageKey> key_deserialized_from_local_storage =
        StorageKey::Deserialize(key_string_for_local_storage);

    ASSERT_TRUE(key_deserialized.has_value());
    ASSERT_TRUE(key_deserialized_from_local_storage.has_value());
    if (origin.scheme() != "file") {
      EXPECT_EQ(key, *key_deserialized);
      EXPECT_EQ(key, *key_deserialized_from_local_storage);
    } else {
      // file origins are all collapsed to file:// by serialization.
      EXPECT_EQ(StorageKey(url::Origin::Create(GURL("file://"))),
                *key_deserialized);
      EXPECT_EQ(StorageKey(url::Origin::Create(GURL("file://"))),
                *key_deserialized_from_local_storage);
    }
  }
}

// Same as SerializeDeserialize but for partitioned StorageKeys when
// kThirdPartyStoragePartitioning is enabled.
TEST(StorageKeyTest, SerializeDeserializePartitioned) {
  base::test::ScopedFeatureList scope_feature_list;
  scope_feature_list.InitAndEnableFeature(
      features::kThirdPartyStoragePartitioning);

  net::SchemefulSite SiteExample(GURL("https://example.com"));
  net::SchemefulSite SiteTest(GURL("https://test.example"));
  net::SchemefulSite SiteFile(GURL("file:///"));

  struct {
    const char* origin;
    const net::SchemefulSite& site;
  } kTestCases[] = {
      // 1p context case.
      {"https://example.com/", SiteExample},
      {"https://test.example", SiteTest},
      {"https://sub.test.example/", SiteTest},
      // 3p context case
      {"https://example.com/", SiteTest},
      {"https://sub.test.example/", SiteExample},
      // File case.
      {"file:///foo/bar", SiteFile},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin);
    url::Origin origin = url::Origin::Create(GURL(test.origin));
    const net::SchemefulSite& site = test.site;

    StorageKey key(origin, site);
    std::string key_string = key.Serialize();
    std::string key_string_for_local_storage = key.SerializeForLocalStorage();
    absl::optional<StorageKey> key_deserialized =
        StorageKey::Deserialize(key_string);
    absl::optional<StorageKey> key_deserialized_from_local_storage =
        StorageKey::Deserialize(key_string_for_local_storage);

    ASSERT_TRUE(key_deserialized.has_value());
    ASSERT_TRUE(key_deserialized_from_local_storage.has_value());
    if (origin.scheme() != "file") {
      EXPECT_EQ(key, *key_deserialized);
      EXPECT_EQ(key, *key_deserialized_from_local_storage);
    } else {
      // file origins are all collapsed to file:// by serialization.
      EXPECT_EQ(StorageKey(url::Origin::Create(GURL("file://")),
                           net::SchemefulSite(GURL("file://"))),
                *key_deserialized);
      EXPECT_EQ(StorageKey(url::Origin::Create(GURL("file://")),
                           net::SchemefulSite(GURL("file://"))),
                *key_deserialized_from_local_storage);
    }
  }
}

TEST(StorageKeyTest, SerializeDeserializeNonce) {
  struct {
    const char* origin;
    const base::UnguessableToken nonce;
  } kTestCases[] = {
      {"https://example.com/",
       base::UnguessableToken::Deserialize(12345ULL, 67890ULL)},
      {"https://test.example",
       base::UnguessableToken::Deserialize(22222ULL, 99999ULL)},
      {"https://sub.test.example/",
       base::UnguessableToken::Deserialize(9876ULL, 54321ULL)},
      {"https://other.example/",
       base::UnguessableToken::Deserialize(3735928559ULL, 110521ULL)},
      {"https://other2.example/", base::UnguessableToken::Create()},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.origin);
    url::Origin origin = url::Origin::Create(GURL(test.origin));
    const base::UnguessableToken& nonce = test.nonce;

    StorageKey key = StorageKey::CreateWithNonce(origin, nonce);
    std::string key_string = key.Serialize();
    std::string key_string_for_local_storage = key.SerializeForLocalStorage();
    absl::optional<StorageKey> key_deserialized =
        StorageKey::Deserialize(key_string);
    absl::optional<StorageKey> key_deserialized_from_local_storage =
        StorageKey::Deserialize(key_string_for_local_storage);

    ASSERT_TRUE(key_deserialized.has_value());
    ASSERT_TRUE(key_deserialized_from_local_storage.has_value());

    EXPECT_EQ(key, *key_deserialized);
    EXPECT_EQ(key, *key_deserialized_from_local_storage);
  }
}

TEST(StorageKeyTest, IsThirdPartyStoragePartitioningEnabled) {
  EXPECT_FALSE(StorageKey::IsThirdPartyStoragePartitioningEnabled());
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kThirdPartyStoragePartitioning);
  EXPECT_TRUE(StorageKey::IsThirdPartyStoragePartitioningEnabled());
}

// Test that StorageKey's top_level_site getter returns origin's site when
// storage partitioning is disabled.
TEST(StorageKeyTest, TopLevelSiteGetter) {
  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));

  StorageKey key_origin1 = StorageKey(origin1);
  StorageKey key_origin1_site1 = StorageKey(origin1, origin1);
  StorageKey key_origin1_site2 = StorageKey(origin1, origin2);

  EXPECT_EQ(net::SchemefulSite(origin1), key_origin1.top_level_site());
  EXPECT_EQ(net::SchemefulSite(origin1), key_origin1_site1.top_level_site());
  EXPECT_EQ(net::SchemefulSite(origin1), key_origin1_site2.top_level_site());
}

// Test that StorageKey's top_level_site getter returns the top level site when
// storage partitioning is enabled.
TEST(StorageKeyTest, TopLevelSiteGetterWithPartitioningEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kThirdPartyStoragePartitioning);

  url::Origin origin1 = url::Origin::Create(GURL("https://example.com"));
  url::Origin origin2 = url::Origin::Create(GURL("https://test.example"));

  StorageKey key_origin1 = StorageKey(origin1);
  StorageKey key_origin1_site1 = StorageKey(origin1, origin1);
  StorageKey key_origin1_site2 = StorageKey(origin1, origin2);

  EXPECT_EQ(net::SchemefulSite(origin1), key_origin1.top_level_site());
  EXPECT_EQ(net::SchemefulSite(origin1), key_origin1_site1.top_level_site());
  EXPECT_EQ(net::SchemefulSite(origin2), key_origin1_site2.top_level_site());
}

TEST(StorageKeyTest, IsThirdPartyContext) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kThirdPartyStoragePartitioning);

  const url::Origin kOrigin = url::Origin::Create(GURL("https://www.foo.com"));
  const url::Origin kInsecureOrigin =
      url::Origin::Create(GURL("http://www.foo.com"));
  const url::Origin kSubdomainOrigin =
      url::Origin::Create(GURL("https://bar.foo.com"));
  const url::Origin kDifferentSite =
      url::Origin::Create(GURL("https://www.bar.com"));

  struct TestCase {
    const url::Origin origin;
    const url::Origin top_level_origin;
    const bool expected;
    const bool has_nonce = false;
  } test_cases[] = {
      {kOrigin, kOrigin, false},          {kOrigin, kInsecureOrigin, true},
      {kOrigin, kSubdomainOrigin, false}, {kOrigin, kDifferentSite, true},
      {kOrigin, kOrigin, true, true},
  };
  for (const auto& test_case : test_cases) {
    if (test_case.has_nonce) {
      StorageKey key = StorageKey::CreateWithNonce(
          test_case.origin, base::UnguessableToken::Create());
      EXPECT_EQ(test_case.expected, key.IsThirdPartyContext());
      EXPECT_NE(key.IsThirdPartyContext(), key.IsFirstPartyContext());
      continue;
    }
    StorageKey key(test_case.origin, test_case.top_level_origin);
    EXPECT_EQ(test_case.expected, key.IsThirdPartyContext());
    EXPECT_NE(key.IsThirdPartyContext(), key.IsFirstPartyContext());
    // IsThirdPartyContext should not depend on the order of the arguments.
    key = StorageKey(test_case.top_level_origin, test_case.origin);
    EXPECT_EQ(test_case.expected, key.IsThirdPartyContext());
    EXPECT_NE(key.IsThirdPartyContext(), key.IsFirstPartyContext());
  }
}

TEST(StorageKeyTest, ToNetSiteForCookies) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kThirdPartyStoragePartitioning);

  const url::Origin kOrigin = url::Origin::Create(GURL("https://www.foo.com"));
  const url::Origin kInsecureOrigin =
      url::Origin::Create(GURL("http://www.foo.com"));
  const url::Origin kSubdomainOrigin =
      url::Origin::Create(GURL("https://bar.foo.com"));
  const url::Origin kDifferentSite =
      url::Origin::Create(GURL("https://www.bar.com"));

  struct TestCase {
    const url::Origin origin;
    const url::Origin top_level_origin;
    const net::SchemefulSite expected;
    const bool expected_opaque = false;
    const bool has_nonce = false;
  } test_cases[] = {
      {kOrigin, kOrigin, net::SchemefulSite(kOrigin)},
      {kOrigin, kInsecureOrigin, net::SchemefulSite(kInsecureOrigin)},
      {kInsecureOrigin, kOrigin, net::SchemefulSite(kOrigin)},
      {kOrigin, kSubdomainOrigin, net::SchemefulSite(kOrigin)},
      {kSubdomainOrigin, kOrigin, net::SchemefulSite(kOrigin)},
      {kOrigin, kDifferentSite, net::SchemefulSite(), true},
      {kOrigin, kOrigin, net::SchemefulSite(), true, true},
  };
  for (const auto& test_case : test_cases) {
    net::SchemefulSite got_site;
    if (test_case.has_nonce) {
      got_site = StorageKey::CreateWithNonce(test_case.origin,
                                             base::UnguessableToken::Create())
                     .ToNetSiteForCookies()
                     .site();
    } else {
      got_site = StorageKey(test_case.origin, test_case.top_level_origin)
                     .ToNetSiteForCookies()
                     .site();
    }
    if (test_case.expected_opaque) {
      EXPECT_TRUE(got_site.opaque());
      continue;
    }
    EXPECT_EQ(test_case.expected, got_site);
  }
}

}  // namespace blink
