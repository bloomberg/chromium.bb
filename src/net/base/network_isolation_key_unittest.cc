// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_isolation_key.h"

#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace net {

namespace {

const char kDataUrl[] = "data:text/html,<body>Hello World</body>";

}  // namespace

TEST(NetworkIsolationKeyTest, EmptyKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  NetworkIsolationKey key;
  EXPECT_FALSE(key.IsFullyPopulated());
  EXPECT_EQ(std::string(), key.ToString());
  EXPECT_TRUE(key.IsTransient());
  EXPECT_EQ("null", key.ToDebugString());
}

TEST(NetworkIsolationKeyTest, NonEmptyKey) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  SchemefulSite site = SchemefulSite(GURL("http://a.test/"));
  NetworkIsolationKey key(site, site);
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_EQ(site.Serialize(), key.ToString());
  EXPECT_FALSE(key.IsTransient());
  EXPECT_EQ("http://a.test", key.ToDebugString());
}

TEST(NetworkIsolationKeyTest, OpaqueOriginKey) {
  SchemefulSite site_data = SchemefulSite(GURL(kDataUrl));
  NetworkIsolationKey key(site_data, site_data);
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_EQ(std::string(), key.ToString());
  EXPECT_TRUE(key.IsTransient());

  // Create another site with an opaque origin, and make sure it's different and
  // has a different debug string.
  SchemefulSite other_site = SchemefulSite(GURL(kDataUrl));
  NetworkIsolationKey other_key(other_site, other_site);
  EXPECT_NE(key, other_key);
  EXPECT_NE(key.ToDebugString(), other_key.ToDebugString());
}

TEST(NetworkIsolationKeyTest, Operators) {
  // These are in ascending order.
  const NetworkIsolationKey kKeys[] = {
      NetworkIsolationKey(),
      // Site with unique origins are still sorted by scheme, so data is before
      // file, and file before http.
      NetworkIsolationKey(SchemefulSite(GURL(kDataUrl)),
                          SchemefulSite(GURL(kDataUrl))),
      NetworkIsolationKey(SchemefulSite(GURL("file:///foo")),
                          SchemefulSite(GURL("file:///foo"))),
      NetworkIsolationKey(SchemefulSite(GURL("http://a.test/")),
                          SchemefulSite(GURL("http://a.test/"))),
      NetworkIsolationKey(SchemefulSite(GURL("http://b.test/")),
                          SchemefulSite(GURL("http://b.test/"))),
      NetworkIsolationKey(SchemefulSite(GURL("https://a.test/")),
                          SchemefulSite(GURL("https://a.test/"))),
  };

  for (size_t first = 0; first < base::size(kKeys); ++first) {
    NetworkIsolationKey key1 = kKeys[first];
    SCOPED_TRACE(key1.ToDebugString());

    EXPECT_TRUE(key1 == key1);
    EXPECT_FALSE(key1 < key1);

    // Make sure that copying a key doesn't change the results of any operation.
    // This check is a bit more interesting with unique origins.
    NetworkIsolationKey key1_copy = key1;
    EXPECT_TRUE(key1 == key1_copy);
    EXPECT_FALSE(key1 < key1_copy);
    EXPECT_FALSE(key1_copy < key1);

    for (size_t second = first + 1; second < base::size(kKeys); ++second) {
      NetworkIsolationKey key2 = kKeys[second];
      SCOPED_TRACE(key2.ToDebugString());

      EXPECT_TRUE(key1 < key2);
      EXPECT_FALSE(key2 < key1);
      EXPECT_FALSE(key1 == key2);
      EXPECT_FALSE(key2 == key1);
    }
  }
}

TEST(NetworkIsolationKeyTest, UniqueOriginOperators) {
  const auto kSite1 = SchemefulSite(GURL(kDataUrl));
  const auto kSite2 = SchemefulSite(GURL(kDataUrl));
  NetworkIsolationKey key1(kSite1, kSite1);
  NetworkIsolationKey key2(kSite2, kSite2);

  EXPECT_TRUE(key1 == key1);
  EXPECT_TRUE(key2 == key2);

  // Creating copies shouldn't affect comparison result.
  EXPECT_TRUE(NetworkIsolationKey(key1) == NetworkIsolationKey(key1));
  EXPECT_TRUE(NetworkIsolationKey(key2) == NetworkIsolationKey(key2));

  EXPECT_FALSE(key1 == key2);
  EXPECT_FALSE(key2 == key1);

  // Order of Nonces isn't predictable, but they should have an ordering.
  EXPECT_TRUE(key1 < key2 || key2 < key1);
  EXPECT_TRUE(!(key1 < key2) || !(key2 < key1));
}

TEST(NetworkIsolationKeyTest, KeyWithOpaqueFrameOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  SchemefulSite site_data = SchemefulSite(GURL(kDataUrl));

  NetworkIsolationKey key1(SchemefulSite(GURL("http://a.test")), site_data);
  EXPECT_TRUE(key1.IsFullyPopulated());
  EXPECT_FALSE(key1.IsTransient());
  EXPECT_EQ("http://a.test", key1.ToString());
  EXPECT_EQ("http://a.test", key1.ToDebugString());

  NetworkIsolationKey key2(site_data, SchemefulSite(GURL("http://a.test")));
  EXPECT_TRUE(key2.IsFullyPopulated());
  EXPECT_TRUE(key2.IsTransient());
  EXPECT_EQ("", key2.ToString());
  EXPECT_EQ(site_data.GetDebugString(), key2.ToDebugString());
  EXPECT_NE(SchemefulSite(GURL(kDataUrl)).GetDebugString(),
            key2.ToDebugString());
}

TEST(NetworkIsolationKeyTest, ValueRoundTripEmpty) {
  const SchemefulSite kJunkSite = SchemefulSite(GURL("data:text/html,junk"));

  for (bool use_frame_sites : {true, false}) {
    SCOPED_TRACE(use_frame_sites);
    base::test::ScopedFeatureList feature_list;
    if (use_frame_sites) {
      feature_list.InitAndEnableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    }

    // Convert empty key to value and back, expecting the same value.
    NetworkIsolationKey no_frame_site_key;
    base::Value no_frame_site_value;
    ASSERT_TRUE(no_frame_site_key.ToValue(&no_frame_site_value));

    // Fill initial value with junk data, to make sure it's overwritten.
    NetworkIsolationKey out_key(kJunkSite, kJunkSite);
    EXPECT_TRUE(NetworkIsolationKey::FromValue(no_frame_site_value, &out_key));
    EXPECT_EQ(no_frame_site_key, out_key);
  }
}

TEST(NetworkIsolationKeyTest, ValueRoundTripNoFrameOrigin) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);
  const SchemefulSite kJunkSite = SchemefulSite(GURL("data:text/html,junk"));

  NetworkIsolationKey key1(SchemefulSite(GURL("https://foo.test/")), kJunkSite);
  base::Value value;
  ASSERT_TRUE(key1.ToValue(&value));

  // Fill initial value with junk data, to make sure it's overwritten.
  NetworkIsolationKey key2(kJunkSite, kJunkSite);
  EXPECT_TRUE(NetworkIsolationKey::FromValue(value, &key2));
  EXPECT_EQ(key1, key2);

  feature_list.Reset();
  feature_list.InitAndEnableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  // Loading should fail when frame sites are enabled.
  EXPECT_FALSE(NetworkIsolationKey::FromValue(value, &key2));
}

TEST(NetworkIsolationKeyTest, ValueRoundTripFrameSite) {
  const SchemefulSite kJunkSite = SchemefulSite(GURL("data:text/html,junk"));

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  NetworkIsolationKey key1(SchemefulSite(GURL("https://foo.test/")),
                           SchemefulSite(GURL("https://foo.test/")));
  base::Value value;
  ASSERT_TRUE(key1.ToValue(&value));

  // Fill initial value with junk data, to make sure it's overwritten.
  NetworkIsolationKey key2(kJunkSite, kJunkSite);
  EXPECT_TRUE(NetworkIsolationKey::FromValue(value, &key2));
  EXPECT_EQ(key1, key2);

  feature_list.Reset();
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  // Loading should fail when frame sites are disabled.
  EXPECT_FALSE(NetworkIsolationKey::FromValue(value, &key2));
}

TEST(NetworkIsolationKeyTest, ToValueTransientSite) {
  const SchemefulSite kSiteWithTransientOrigin =
      SchemefulSite(GURL("data:text/html,transient"));

  for (bool use_frame_sites : {true, false}) {
    SCOPED_TRACE(use_frame_sites);
    base::test::ScopedFeatureList feature_list;
    if (use_frame_sites) {
      feature_list.InitAndEnableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    }

    NetworkIsolationKey key(kSiteWithTransientOrigin, kSiteWithTransientOrigin);
    EXPECT_TRUE(key.IsTransient());
    base::Value value;
    EXPECT_FALSE(key.ToValue(&value));
  }
}

TEST(NetworkIsolationKeyTest, FromValueBadData) {
  // Can't create these inline, since vector initialization lists require a
  // copy, and base::Value has no copy operator, only move.
  base::Value::ListStorage not_a_url_list;
  not_a_url_list.emplace_back(base::Value("not-a-url"));

  base::Value::ListStorage transient_origin_list;
  transient_origin_list.emplace_back(base::Value("data:text/html,transient"));

  base::Value::ListStorage too_many_origins_list;
  too_many_origins_list.emplace_back(base::Value("https://too/"));
  too_many_origins_list.emplace_back(base::Value("https://many/"));
  too_many_origins_list.emplace_back(base::Value("https://origins/"));

  const base::Value kTestCases[] = {
      base::Value(base::Value::Type::STRING),
      base::Value(base::Value::Type::DICTIONARY),
      base::Value(std::move(not_a_url_list)),
      base::Value(std::move(transient_origin_list)),
      base::Value(std::move(too_many_origins_list)),
  };

  for (bool use_frame_origins : {true, false}) {
    SCOPED_TRACE(use_frame_origins);
    base::test::ScopedFeatureList feature_list;
    if (use_frame_origins) {
      feature_list.InitAndEnableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    }

    for (const auto& test_case : kTestCases) {
      NetworkIsolationKey key;
      // Write the value on failure.
      EXPECT_FALSE(NetworkIsolationKey::FromValue(test_case, &key))
          << test_case;
    }
  }
}

TEST(NetworkIsolationKeyTest, OpaqueNonTransient_DisableAppendFrameSite) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  NetworkIsolationKey key = NetworkIsolationKey::CreateOpaqueAndNonTransient();
  NetworkIsolationKey other_key =
      NetworkIsolationKey::CreateOpaqueAndNonTransient();
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_FALSE(key.IsTransient());
  EXPECT_FALSE(key.IsEmpty());
  EXPECT_EQ(key.GetTopFrameSite()->GetDebugString() + " non-transient",
            key.ToDebugString());
  EXPECT_EQ(key.ToString(), key.ToString());
  EXPECT_NE(key.ToString(), other_key.ToString());

  // |opaque_and_non_transient_| is kept when a new frame site is opaque.
  SchemefulSite opaque_site;
  NetworkIsolationKey modified_key = key.CreateWithNewFrameSite(opaque_site);
  EXPECT_TRUE(modified_key.IsFullyPopulated());
  EXPECT_FALSE(modified_key.IsTransient());
  EXPECT_FALSE(modified_key.IsEmpty());
  EXPECT_EQ(modified_key.ToString(), key.ToString());
  EXPECT_EQ(modified_key.GetTopFrameSite()->GetDebugString() + " non-transient",
            modified_key.ToDebugString());

  // Should not be equal to a similar NetworkIsolationKey derived from it.
  EXPECT_NE(
      key, NetworkIsolationKey(*key.GetTopFrameSite(), *key.GetTopFrameSite()));

  // To and back from a Value should yield the same key.
  base::Value value;
  ASSERT_TRUE(key.ToValue(&value));
  NetworkIsolationKey from_value;
  ASSERT_TRUE(NetworkIsolationKey::FromValue(value, &from_value));
  EXPECT_EQ(key, from_value);
  EXPECT_EQ(key.ToString(), from_value.ToString());
  EXPECT_EQ(key.ToDebugString(), from_value.ToDebugString());
}

TEST(NetworkIsolationKeyTest, OpaqueNonTransient_EnableAppendFrameSite) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  NetworkIsolationKey key = NetworkIsolationKey::CreateOpaqueAndNonTransient();
  NetworkIsolationKey other_key =
      NetworkIsolationKey::CreateOpaqueAndNonTransient();
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_FALSE(key.IsTransient());
  EXPECT_FALSE(key.IsEmpty());
  EXPECT_EQ(key.ToString(), key.ToString());
  EXPECT_NE(key.ToString(), other_key.ToString());
  EXPECT_EQ(key.GetTopFrameSite()->GetDebugString() + " " +
                key.GetFrameSite()->GetDebugString() + " non-transient",
            key.ToDebugString());

  // |opaque_and_non_transient_| is kept when a new frame site is opaque.
  SchemefulSite opaque_site;
  NetworkIsolationKey modified_key = key.CreateWithNewFrameSite(opaque_site);
  EXPECT_TRUE(modified_key.IsFullyPopulated());
  EXPECT_FALSE(modified_key.IsTransient());
  EXPECT_FALSE(modified_key.IsEmpty());
  EXPECT_NE(key.ToString(), modified_key.ToString());
  EXPECT_EQ(modified_key.GetTopFrameSite()->GetDebugString() + " " +
                modified_key.GetFrameSite()->GetDebugString() +
                " non-transient",
            modified_key.ToDebugString());

  // Should not be equal to a similar NetworkIsolationKey derived from it.
  EXPECT_NE(key,
            NetworkIsolationKey(*key.GetTopFrameSite(), *key.GetFrameSite()));

  // To and back from a Value should yield the same key.
  base::Value value;
  ASSERT_TRUE(key.ToValue(&value));
  NetworkIsolationKey from_value;
  ASSERT_TRUE(NetworkIsolationKey::FromValue(value, &from_value));
  EXPECT_EQ(key, from_value);
  EXPECT_EQ(key.ToString(), from_value.ToString());
  EXPECT_EQ(key.ToDebugString(), from_value.ToDebugString());
}

class NetworkIsolationKeyWithFrameOriginTest : public testing::Test {
 public:
  NetworkIsolationKeyWithFrameOriginTest() {
    feature_list_.InitAndEnableFeature(
        features::kAppendFrameOriginToNetworkIsolationKey);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(NetworkIsolationKeyWithFrameOriginTest, WithFrameSite) {
  NetworkIsolationKey key(SchemefulSite(GURL("http://b.test")),
                          SchemefulSite(GURL("http://a.test/")));
  EXPECT_TRUE(key.IsFullyPopulated());
  EXPECT_FALSE(key.IsTransient());
  EXPECT_EQ("http://b.test http://a.test", key.ToString());
  EXPECT_EQ("http://b.test http://a.test", key.ToDebugString());

  EXPECT_TRUE(key == key);
  EXPECT_FALSE(key != key);
  EXPECT_FALSE(key < key);
}

TEST_F(NetworkIsolationKeyWithFrameOriginTest, OpaqueSiteKey) {
  SchemefulSite site_data = SchemefulSite(GURL(kDataUrl));
  SchemefulSite site_data2 = SchemefulSite(GURL(kDataUrl));
  SchemefulSite site_a = SchemefulSite(GURL("http://a.test"));

  NetworkIsolationKey key1(site_a, site_data);
  EXPECT_TRUE(key1.IsFullyPopulated());
  EXPECT_TRUE(key1.IsTransient());
  EXPECT_EQ("", key1.ToString());
  EXPECT_EQ("http://a.test " + site_data.GetDebugString(),
            key1.ToDebugString());
  EXPECT_NE(NetworkIsolationKey(site_a, site_data2), key1);

  NetworkIsolationKey key2(site_data, site_a);
  EXPECT_TRUE(key2.IsFullyPopulated());
  EXPECT_TRUE(key2.IsTransient());
  EXPECT_EQ("", key2.ToString());
  EXPECT_EQ(site_data.GetDebugString() + " http://a.test",
            key2.ToDebugString());
  EXPECT_NE(NetworkIsolationKey(site_data2, site_a), key2);
}

TEST_F(NetworkIsolationKeyWithFrameOriginTest, OpaqueSiteKeyBoth) {
  SchemefulSite site_data_1 = SchemefulSite(GURL(kDataUrl));
  SchemefulSite site_data_2 = SchemefulSite(GURL(kDataUrl));
  SchemefulSite site_data_3 = SchemefulSite(GURL(kDataUrl));

  NetworkIsolationKey key1(site_data_1, site_data_2);
  NetworkIsolationKey key2(site_data_1, site_data_2);
  NetworkIsolationKey key3(site_data_1, site_data_3);

  // All the keys should be fully populated and transient.
  EXPECT_TRUE(key1.IsFullyPopulated());
  EXPECT_TRUE(key2.IsFullyPopulated());
  EXPECT_TRUE(key3.IsFullyPopulated());
  EXPECT_TRUE(key1.IsTransient());
  EXPECT_TRUE(key2.IsTransient());
  EXPECT_TRUE(key3.IsTransient());

  // Test the equality/comparisons of the various keys
  EXPECT_TRUE(key1 == key2);
  EXPECT_FALSE(key1 == key3);
  EXPECT_FALSE(key1 < key2 || key2 < key1);
  EXPECT_TRUE(key1 < key3 || key3 < key1);

  // Test the ToString and ToDebugString
  EXPECT_EQ(key1.ToDebugString(), key2.ToDebugString());
  EXPECT_NE(key1.ToDebugString(), key3.ToDebugString());
  EXPECT_EQ("", key1.ToString());
  EXPECT_EQ("", key2.ToString());
  EXPECT_EQ("", key3.ToString());
}

// Make sure that the logic to extract the registerable domain from an origin
// does not affect the host when using a non-standard scheme.
TEST(NetworkIsolationKeyTest, NonStandardScheme) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAppendFrameOriginToNetworkIsolationKey);

  // Have to register the scheme, or SchemefulSite() will return an opaque
  // origin.
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("foo", url::SCHEME_WITH_HOST);

  SchemefulSite site = SchemefulSite(GURL("foo://a.foo.com"));
  net::NetworkIsolationKey key(site, site);
  EXPECT_FALSE(key.GetTopFrameSite()->opaque());
  EXPECT_EQ("foo://a.foo.com", key.ToString());
}

TEST_F(NetworkIsolationKeyWithFrameOriginTest, CreateWithNewFrameSite) {
  SchemefulSite site_a = SchemefulSite(GURL("http://a.com"));
  SchemefulSite site_b = SchemefulSite(GURL("http://b.com"));
  SchemefulSite site_c = SchemefulSite(GURL("http://c.com"));

  net::NetworkIsolationKey key(site_a, site_b);
  NetworkIsolationKey key_c = key.CreateWithNewFrameSite(site_c);
  EXPECT_EQ(site_c, key_c.GetFrameSite());
  EXPECT_EQ(site_a, key_c.GetTopFrameSite());
}

TEST(NetworkIsolationKeyTest, CreateTransient) {
  for (bool use_frame_sites : {true, false}) {
    SCOPED_TRACE(use_frame_sites);
    base::test::ScopedFeatureList feature_list;
    if (use_frame_sites) {
      feature_list.InitAndEnableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    } else {
      feature_list.InitAndDisableFeature(
          features::kAppendFrameOriginToNetworkIsolationKey);
    }

    NetworkIsolationKey transient_key = NetworkIsolationKey::CreateTransient();
    EXPECT_TRUE(transient_key.IsFullyPopulated());
    EXPECT_TRUE(transient_key.IsTransient());
    EXPECT_FALSE(transient_key.IsEmpty());
    EXPECT_EQ(transient_key, transient_key);

    // Transient values can't be saved to disk.
    base::Value value;
    EXPECT_FALSE(transient_key.ToValue(&value));

    // Make sure that subsequent calls don't return the same NIK.
    for (int i = 0; i < 1000; ++i) {
      EXPECT_NE(transient_key, NetworkIsolationKey::CreateTransient());
    }
  }
}

}  // namespace net
