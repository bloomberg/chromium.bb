// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/referrer.h"

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

using ReferrerSanitizerTest = testing::Test;

TEST_F(ReferrerSanitizerTest, SanitizesPolicyForEmptyReferrers) {
  EXPECT_DCHECK_DEATH(ignore_result(Referrer::SanitizeForRequest(
      GURL("https://a"),
      Referrer(GURL(), static_cast<network::mojom::ReferrerPolicy>(200)))));
}

TEST_F(ReferrerSanitizerTest, SanitizesPolicyForNonEmptyReferrers) {
  EXPECT_DCHECK_DEATH(ignore_result(Referrer::SanitizeForRequest(
      GURL("https://a"),
      Referrer(GURL("http://b"),
               static_cast<network::mojom::ReferrerPolicy>(200)))));
}

TEST_F(ReferrerSanitizerTest, SanitizeOriginForRequest) {
  GURL url_a = GURL("https://a.example.com");
  GURL url_b = GURL("https://b.example.com");
  url::Origin origin_a = url::Origin::Create(url_a);
  url::Origin origin_b = url::Origin::Create(url_b);
  url::Origin origin_a_opaque = origin_a.DeriveNewOpaqueOrigin();

  // Original origin should be returned when the policy is compatible with the
  // target.
  EXPECT_EQ(origin_a,
            Referrer::SanitizeOriginForRequest(
                url_a, origin_a, network::mojom::ReferrerPolicy::kSameOrigin));
  EXPECT_EQ(origin_b,
            Referrer::SanitizeOriginForRequest(
                url_a, origin_b, network::mojom::ReferrerPolicy::kAlways));

  // Opaque origin should be returned when the policy asks to avoid disclosing
  // the referrer to the target.
  EXPECT_TRUE(Referrer::SanitizeOriginForRequest(
                  url_a, origin_b, network::mojom::ReferrerPolicy::kNever)
                  .opaque());
  EXPECT_TRUE(Referrer::SanitizeOriginForRequest(
                  url_a, origin_b, network::mojom::ReferrerPolicy::kSameOrigin)
                  .opaque());

  // Okay to use an opaque origin as a target - a *unique* opaque origin should
  // be returned.
  url::Origin result = Referrer::SanitizeOriginForRequest(
      url_a, origin_a_opaque, network::mojom::ReferrerPolicy::kAlways);
  EXPECT_TRUE(result.opaque());
  EXPECT_FALSE(result.CanBeDerivedFrom(url_a));
  EXPECT_NE(result, origin_a_opaque);
}

TEST(ReferrerSanitizerTest, OnlyHTTPFamilyReferrer) {
  auto result = Referrer::SanitizeForRequest(
      GURL("https://a"),
      Referrer(GURL("chrome-extension://ghbmnnjooekpmoecnnnilnnbdlolhkhi"),
               network::mojom::ReferrerPolicy::kAlways));
  EXPECT_TRUE(result.url.is_empty());
}

TEST(ReferrerSanitizerTest, AboutBlankURLRequest) {
  auto result = Referrer::SanitizeForRequest(
      GURL("about:blank"),
      Referrer(GURL("http://foo"), network::mojom::ReferrerPolicy::kAlways));
  EXPECT_EQ(result.url, GURL("http://foo"));
}

TEST(ReferrerSanitizerTest, HTTPURLRequest) {
  auto result = Referrer::SanitizeForRequest(
      GURL("http://bar"),
      Referrer(GURL("http://foo"), network::mojom::ReferrerPolicy::kAlways));
  EXPECT_EQ(result.url, GURL("http://foo"));
}

TEST(ReferrerSanitizerTest, DataURLRequest) {
  auto result = Referrer::SanitizeForRequest(
      GURL("data:text/html,<div>foo</div>"),
      Referrer(GURL("http://foo"), network::mojom::ReferrerPolicy::kAlways));
  EXPECT_TRUE(result.url.is_empty());
}

TEST(ReferrerTest, BlinkNetRoundTripConversion) {
  const net::URLRequest::ReferrerPolicy policies[] = {
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      net::URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
      net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN,
      net::URLRequest::NEVER_CLEAR_REFERRER,
      net::URLRequest::ORIGIN,
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN,
      net::URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
      net::URLRequest::NO_REFERRER,
  };

  for (auto policy : policies) {
    EXPECT_EQ(Referrer::ReferrerPolicyForUrlRequest(
                  Referrer::NetReferrerPolicyToBlinkReferrerPolicy(policy)),
              policy);
  }
}

TEST(DefaultReferrerPolicyTest, Unconfigured) {
  EXPECT_EQ(
      Referrer::GetDefaultReferrerPolicy(),
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE);
}

TEST(DefaultReferrerPolicyTest, FeatureOnly) {
  base::test::ScopedFeatureList f;
  f.InitAndEnableFeature(features::kReducedReferrerGranularity);
  EXPECT_EQ(
      Referrer::GetDefaultReferrerPolicy(),
      net::URLRequest::REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN);
}

TEST(DefaultReferrerPolicyTest, SetAndGetForceLegacy) {
  EXPECT_FALSE(content::Referrer::ShouldForceLegacyDefaultReferrerPolicy());
  content::Referrer::SetForceLegacyDefaultReferrerPolicy(true);
  EXPECT_TRUE(content::Referrer::ShouldForceLegacyDefaultReferrerPolicy());
}

TEST(DefaultReferrerPolicyTest, ForceLegacyOnly) {
  content::Referrer::SetForceLegacyDefaultReferrerPolicy(true);
  EXPECT_EQ(
      Referrer::GetDefaultReferrerPolicy(),
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE);
}

TEST(DefaultReferrerPolicyTest, FeatureAndForceLegacy) {
  base::test::ScopedFeatureList f;
  f.InitAndEnableFeature(features::kReducedReferrerGranularity);
  content::Referrer::SetForceLegacyDefaultReferrerPolicy(true);
  EXPECT_EQ(
      Referrer::GetDefaultReferrerPolicy(),
      net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE);
}

}  // namespace content
