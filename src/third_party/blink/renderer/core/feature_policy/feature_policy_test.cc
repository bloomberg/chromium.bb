// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/feature_policy/feature_policy.h"

#include <map>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"

#include "url/gurl.h"
#include "url/origin.h"

// Origin strings used for tests
#define ORIGIN_A "https://example.com/"
#define ORIGIN_B "https://example.net/"
#define ORIGIN_C "https://example.org/"

class GURL;

namespace blink {

namespace {

const char* const kValidPolicies[] = {
    "",      // An empty policy.
    " ",     // An empty policy.
    ";;",    // Empty policies.
    ",,",    // Empty policies.
    " ; ;",  // Empty policies.
    " , ,",  // Empty policies.
    ",;,",   // Empty policies.
    "geolocation 'none'",
    "geolocation 'self'",
    "geolocation 'src'",  // Only valid for iframe allow attribute.
    "geolocation",        // Only valid for iframe allow attribute.
    "geolocation; fullscreen; payment",
    "geolocation *",
    "geolocation " ORIGIN_A "",
    "geolocation " ORIGIN_B "",
    "geolocation  " ORIGIN_A " " ORIGIN_B "",
    "geolocation 'none' " ORIGIN_A " " ORIGIN_B "",
    "geolocation " ORIGIN_A " 'none' " ORIGIN_B "",
    "geolocation 'none' 'none' 'none'",
    "geolocation " ORIGIN_A " *",
    "fullscreen  " ORIGIN_A "; payment 'self'",
    "fullscreen  " ORIGIN_A "(true)",
    "fullscreen  " ORIGIN_A "(false)",
    "fullscreen  " ORIGIN_A "(True)",
    "fullscreen  " ORIGIN_A "(TRUE)",
    "oversized-images " ORIGIN_A "(2.0)",
    "oversized-images " ORIGIN_A "(0.0)",
    "oversized-images " ORIGIN_A "(4)",
    "oversized-images " ORIGIN_A "(20000)",
    "oversized-images " ORIGIN_A "(2e50)",
    "oversized-images " ORIGIN_A "(inf)",
    "oversized-images " ORIGIN_A "(Inf)",
    "oversized-images " ORIGIN_A "(INF)",
    "fullscreen " ORIGIN_A "; payment *, geolocation 'self'"};

const char* const kInvalidPolicies[] = {
    "badfeaturename",
    "badfeaturename 'self'",
    "1.0",
    "geolocation data://badorigin",
    "geolocation https://bad;origin",
    "geolocation https:/bad,origin",
    "geolocation https://example.com, https://a.com",
    "geolocation *, payment data://badorigin",
    "geolocation ws://xn--fd\xbcwsw3taaaaaBaa333aBBBBBBJBBJBBBt",
    "fullscreen(true)",
    "fullscreen  " ORIGIN_A "(notabool)",
    "fullscreen " ORIGIN_A "(2.0)",
    "oversized-images " ORIGIN_A "(true)",
    "oversized-images " ORIGIN_A "(Something else)",
    "oversized-images " ORIGIN_A "(1",
    "oversized-images " ORIGIN_A "(-1)",
    "oversized-images " ORIGIN_A "(1.2.3)",
    "oversized-images " ORIGIN_A "(1.a.3)",
    "fullscreen  " ORIGIN_A "()"};

}  // namespace

class FeaturePolicyParserTest : public testing::Test {
 protected:
  FeaturePolicyParserTest() = default;

  ~FeaturePolicyParserTest() override = default;

  scoped_refptr<const SecurityOrigin> origin_a_ =
      SecurityOrigin::CreateFromString(ORIGIN_A);
  scoped_refptr<const SecurityOrigin> origin_b_ =
      SecurityOrigin::CreateFromString(ORIGIN_B);
  scoped_refptr<const SecurityOrigin> origin_c_ =
      SecurityOrigin::CreateFromString(ORIGIN_C);

  url::Origin expected_url_origin_a_ = url::Origin::Create(GURL(ORIGIN_A));
  url::Origin expected_url_origin_b_ = url::Origin::Create(GURL(ORIGIN_B));
  url::Origin expected_url_origin_c_ = url::Origin::Create(GURL(ORIGIN_C));

  const FeatureNameMap test_feature_name_map = {
      {"fullscreen", blink::mojom::FeaturePolicyFeature::kFullscreen},
      {"payment", blink::mojom::FeaturePolicyFeature::kPayment},
      {"geolocation", blink::mojom::FeaturePolicyFeature::kGeolocation},
      {"oversized-images",
       blink::mojom::FeaturePolicyFeature::kOversizedImages}};

  const PolicyValue min_value = PolicyValue(false);
  const PolicyValue max_value = PolicyValue(true);
  const PolicyValue sample_double_value =
      PolicyValue(1.5, mojom::PolicyValueType::kDecDouble);
  const PolicyValue default_double_value =
      PolicyValue(2.0, mojom::PolicyValueType::kDecDouble);
  const PolicyValue min_double_value =
      PolicyValue::CreateMinPolicyValue(mojom::PolicyValueType::kDecDouble);
  const PolicyValue max_double_value =
      PolicyValue::CreateMaxPolicyValue(mojom::PolicyValueType::kDecDouble);
};

TEST_F(FeaturePolicyParserTest, ParseValidPolicy) {
  Vector<String> messages;
  for (const char* policy_string : kValidPolicies) {
    messages.clear();
    ParseFeaturePolicy(policy_string, origin_a_.get(), origin_b_.get(),
                       &messages, test_feature_name_map);
    EXPECT_EQ(0UL, messages.size()) << "Should parse " << policy_string;
  }
}

TEST_F(FeaturePolicyParserTest, ParseInvalidPolicy) {
  Vector<String> messages;
  for (const char* policy_string : kInvalidPolicies) {
    messages.clear();
    ParseFeaturePolicy(policy_string, origin_a_.get(), origin_b_.get(),
                       &messages, test_feature_name_map);
    EXPECT_LT(0UL, messages.size()) << "Should fail to parse " << policy_string;
  }
}

TEST_F(FeaturePolicyParserTest, PolicyParsedCorrectly) {
  Vector<String> messages;

  // Empty policy.
  ParsedFeaturePolicy parsed_policy = ParseFeaturePolicy(
      "", origin_a_.get(), origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(0UL, parsed_policy.size());

  // Simple policy with 'self'.
  parsed_policy =
      ParseFeaturePolicy("geolocation 'self'", origin_a_.get(), origin_b_.get(),
                         &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_GE(min_value, parsed_policy[0].fallback_value);
  EXPECT_GE(min_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(1UL, parsed_policy[0].values.size());
  EXPECT_TRUE(parsed_policy[0].values.begin()->first.IsSameOriginWith(
      expected_url_origin_a_));

  // Simple policy with *.
  parsed_policy =
      ParseFeaturePolicy("geolocation *", origin_a_.get(), origin_b_.get(),
                         &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_LE(max_value, parsed_policy[0].fallback_value);
  EXPECT_LE(max_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());

  // Complicated policy.
  parsed_policy = ParseFeaturePolicy(
      "geolocation *; "
      "fullscreen https://example.net https://example.org; "
      "payment 'self'",
      origin_a_.get(), origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(3UL, parsed_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_LE(max_value, parsed_policy[0].fallback_value);
  EXPECT_LE(max_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, parsed_policy[1].feature);
  EXPECT_GE(min_value, parsed_policy[1].fallback_value);
  EXPECT_GE(min_value, parsed_policy[1].opaque_value);
  EXPECT_EQ(2UL, parsed_policy[1].values.size());
  auto it = parsed_policy[1].values.begin();
  EXPECT_TRUE(it->first.IsSameOriginWith(expected_url_origin_b_));
  EXPECT_TRUE((++it)->first.IsSameOriginWith(expected_url_origin_c_));
  EXPECT_EQ(mojom::FeaturePolicyFeature::kPayment, parsed_policy[2].feature);
  EXPECT_GE(min_value, parsed_policy[2].fallback_value);
  EXPECT_GE(min_value, parsed_policy[2].opaque_value);
  EXPECT_EQ(1UL, parsed_policy[2].values.size());
  EXPECT_TRUE(parsed_policy[2].values.begin()->first.IsSameOriginWith(
      expected_url_origin_a_));

  // Multiple policies.
  parsed_policy = ParseFeaturePolicy(
      "geolocation * https://example.net; "
      "fullscreen https://example.net none https://example.org,"
      "payment 'self' badorigin",
      origin_a_.get(), origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(3UL, parsed_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_LE(max_value, parsed_policy[0].fallback_value);
  EXPECT_LE(max_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, parsed_policy[1].feature);
  EXPECT_GE(min_value, parsed_policy[1].fallback_value);
  EXPECT_GE(min_value, parsed_policy[1].opaque_value);
  EXPECT_EQ(2UL, parsed_policy[1].values.size());
  it = parsed_policy[1].values.begin();
  EXPECT_TRUE(it->first.IsSameOriginWith(expected_url_origin_b_));
  EXPECT_TRUE((++it)->first.IsSameOriginWith(expected_url_origin_c_));
  EXPECT_EQ(mojom::FeaturePolicyFeature::kPayment, parsed_policy[2].feature);
  EXPECT_GE(min_value, parsed_policy[2].fallback_value);
  EXPECT_GE(min_value, parsed_policy[2].opaque_value);
  EXPECT_EQ(1UL, parsed_policy[2].values.size());
  EXPECT_TRUE(parsed_policy[2].values.begin()->first.IsSameOriginWith(
      expected_url_origin_a_));

  // Header policies with no optional origin lists.
  parsed_policy =
      ParseFeaturePolicy("geolocation;fullscreen;payment", origin_a_.get(),
                         nullptr, &messages, test_feature_name_map);
  EXPECT_EQ(3UL, parsed_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_GE(min_value, parsed_policy[0].fallback_value);
  EXPECT_GE(min_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(1UL, parsed_policy[0].values.size());
  EXPECT_TRUE(parsed_policy[0].values.begin()->first.IsSameOriginWith(
      expected_url_origin_a_));
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, parsed_policy[1].feature);
  EXPECT_GE(min_value, parsed_policy[1].fallback_value);
  EXPECT_GE(min_value, parsed_policy[1].opaque_value);
  EXPECT_EQ(1UL, parsed_policy[1].values.size());
  EXPECT_TRUE(parsed_policy[1].values.begin()->first.IsSameOriginWith(
      expected_url_origin_a_));
  EXPECT_EQ(mojom::FeaturePolicyFeature::kPayment, parsed_policy[2].feature);
  EXPECT_GE(min_value, parsed_policy[2].opaque_value);
  EXPECT_EQ(1UL, parsed_policy[2].values.size());
  EXPECT_TRUE(parsed_policy[2].values.begin()->first.IsSameOriginWith(
      expected_url_origin_a_));
}

TEST_F(FeaturePolicyParserTest, PolicyParsedCorrectlyForOpaqueOrigins) {
  Vector<String> messages;

  scoped_refptr<SecurityOrigin> opaque_origin =
      SecurityOrigin::CreateUniqueOpaque();

  // Empty policy.
  ParsedFeaturePolicy parsed_policy =
      ParseFeaturePolicy("", origin_a_.get(), opaque_origin.get(), &messages,
                         test_feature_name_map);
  EXPECT_EQ(0UL, parsed_policy.size());

  // Simple policy.
  parsed_policy =
      ParseFeaturePolicy("geolocation", origin_a_.get(), opaque_origin.get(),
                         &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_GE(min_value, parsed_policy[0].fallback_value);
  EXPECT_LE(max_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());

  // Simple policy with 'src'.
  parsed_policy =
      ParseFeaturePolicy("geolocation 'src'", origin_a_.get(),
                         opaque_origin.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_GE(min_value, parsed_policy[0].fallback_value);
  EXPECT_LE(max_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());

  // Simple policy with *.
  parsed_policy =
      ParseFeaturePolicy("geolocation *", origin_a_.get(), opaque_origin.get(),
                         &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_LE(max_value, parsed_policy[0].fallback_value);
  EXPECT_LE(max_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());

  // Policy with explicit origins
  parsed_policy = ParseFeaturePolicy(
      "geolocation https://example.net https://example.org", origin_a_.get(),
      opaque_origin.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_GE(min_value, parsed_policy[0].fallback_value);
  EXPECT_GE(min_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(2UL, parsed_policy[0].values.size());
  auto it = parsed_policy[0].values.begin();
  EXPECT_TRUE(it->first.IsSameOriginWith(expected_url_origin_b_));
  EXPECT_TRUE((++it)->first.IsSameOriginWith(expected_url_origin_c_));

  // Policy with multiple origins, including 'src'.
  parsed_policy = ParseFeaturePolicy("geolocation https://example.net 'src'",
                                     origin_a_.get(), opaque_origin.get(),
                                     &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kGeolocation,
            parsed_policy[0].feature);
  EXPECT_GE(min_value, parsed_policy[0].fallback_value);
  EXPECT_LE(max_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(1UL, parsed_policy[0].values.size());
  EXPECT_TRUE(parsed_policy[0].values.begin()->first.IsSameOriginWith(
      expected_url_origin_b_));
}

TEST_F(FeaturePolicyParserTest, BooleanPolicyParametersParsedCorrectly) {
  Vector<String> messages;
  ParsedFeaturePolicy parsed_policy;

  // Test no origin specified, in a container policy context.
  // (true)
  parsed_policy =
      ParseFeaturePolicy("fullscreen (true)", origin_a_.get(), origin_b_.get(),
                         &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, parsed_policy[0].feature);
  EXPECT_EQ(min_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(min_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(1UL, parsed_policy[0].values.size());
  EXPECT_TRUE(parsed_policy[0].values.begin()->first.IsSameOriginWith(
      expected_url_origin_b_));
  EXPECT_EQ(max_value, parsed_policy[0].values.begin()->second);

  // Test no origin specified, in a header context.
  // (true)
  parsed_policy = ParseFeaturePolicy("fullscreen (true)", origin_a_.get(),
                                     nullptr, &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, parsed_policy[0].feature);
  EXPECT_EQ(min_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(min_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(1UL, parsed_policy[0].values.size());
  EXPECT_TRUE(parsed_policy[0].values.begin()->first.IsSameOriginWith(
      expected_url_origin_a_));
  EXPECT_EQ(max_value, parsed_policy[0].values.begin()->second);

  // Test no origin specified, in a sandboxed container policy context.
  // (true)
  scoped_refptr<SecurityOrigin> opaque_origin =
      SecurityOrigin::CreateUniqueOpaque();
  parsed_policy =
      ParseFeaturePolicy("fullscreen (true)", origin_a_.get(), opaque_origin,
                         &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, parsed_policy[0].feature);
  EXPECT_EQ(min_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(max_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());

  // 'self'(true)
  parsed_policy =
      ParseFeaturePolicy("fullscreen 'self'(true)", origin_a_.get(),
                         origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, parsed_policy[0].feature);
  EXPECT_EQ(min_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(min_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(1UL, parsed_policy[0].values.size());
  EXPECT_TRUE(parsed_policy[0].values.begin()->first.IsSameOriginWith(
      expected_url_origin_a_));
  EXPECT_EQ(max_value, parsed_policy[0].values.begin()->second);

  // *(false)
  parsed_policy =
      ParseFeaturePolicy("fullscreen *(false)", origin_a_.get(),
                         origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, parsed_policy[0].feature);
  EXPECT_EQ(min_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(min_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());

  // *(true)
  parsed_policy =
      ParseFeaturePolicy("fullscreen *(true)", origin_a_.get(), origin_b_.get(),
                         &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, parsed_policy[0].feature);
  EXPECT_EQ(max_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(max_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());
}

TEST_F(FeaturePolicyParserTest, DoublePolicyParametersParsedCorrectly) {
  Vector<String> messages;
  ParsedFeaturePolicy parsed_policy;

  // 'self'(inf)
  parsed_policy =
      ParseFeaturePolicy("oversized-images 'self'(inf)", origin_a_.get(),
                         origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kOversizedImages,
            parsed_policy[0].feature);
  EXPECT_EQ(default_double_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(default_double_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(1UL, parsed_policy[0].values.size());
  EXPECT_TRUE(parsed_policy[0].values.begin()->first.IsSameOriginWith(
      expected_url_origin_a_));
  EXPECT_EQ(max_double_value, parsed_policy[0].values.begin()->second);

  // 'self'(1.5)
  parsed_policy =
      ParseFeaturePolicy("oversized-images 'self'(1.5)", origin_a_.get(),
                         origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kOversizedImages,
            parsed_policy[0].feature);
  EXPECT_EQ(default_double_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(default_double_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(1UL, parsed_policy[0].values.size());
  EXPECT_TRUE(parsed_policy[0].values.begin()->first.IsSameOriginWith(
      expected_url_origin_a_));
  EXPECT_EQ(sample_double_value, parsed_policy[0].values.begin()->second);

  // *(inf)
  parsed_policy =
      ParseFeaturePolicy("oversized-images *(inf)", origin_a_.get(),
                         origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kOversizedImages,
            parsed_policy[0].feature);
  EXPECT_EQ(max_double_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(max_double_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());

  // *(0)
  parsed_policy =
      ParseFeaturePolicy("oversized-images *(0)", origin_a_.get(),
                         origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kOversizedImages,
            parsed_policy[0].feature);
  EXPECT_EQ(min_double_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(min_double_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());

  // *(1.5)
  parsed_policy =
      ParseFeaturePolicy("oversized-images *(1.5)", origin_a_.get(),
                         origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kOversizedImages,
            parsed_policy[0].feature);
  EXPECT_EQ(sample_double_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(sample_double_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());

  // 'self'(1.5) 'src'(inf)
  // Fallbacks should be default values.
  parsed_policy = ParseFeaturePolicy("oversized-images 'self'(1.5) 'src'(inf)",
                                     origin_a_.get(), origin_b_.get(),
                                     &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kOversizedImages,
            parsed_policy[0].feature);
  EXPECT_EQ(default_double_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(default_double_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(2UL, parsed_policy[0].values.size());
  auto origin_and_value = parsed_policy[0].values.begin();
  EXPECT_TRUE(origin_and_value->first.IsSameOriginWith(expected_url_origin_a_));
  EXPECT_EQ(sample_double_value, origin_and_value->second);
  origin_and_value++;
  EXPECT_TRUE(origin_and_value->first.IsSameOriginWith(expected_url_origin_b_));
  EXPECT_EQ(max_double_value, origin_and_value->second);

  // *(1.5) 'src'(inf)
  // Fallbacks should be 1.5
  parsed_policy =
      ParseFeaturePolicy("oversized-images *(1.5) 'src'(inf)", origin_a_.get(),
                         origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kOversizedImages,
            parsed_policy[0].feature);
  EXPECT_EQ(sample_double_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(sample_double_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(1UL, parsed_policy[0].values.size());
  origin_and_value = parsed_policy[0].values.begin();
  EXPECT_TRUE(origin_and_value->first.IsSameOriginWith(expected_url_origin_b_));
  EXPECT_EQ(max_double_value, origin_and_value->second);

  // Test policy: 'self'(1.5) https://example.org(inf)
  // Fallbacks should be default value.
  parsed_policy = ParseFeaturePolicy(
      "oversized-images 'self'(1.5) " ORIGIN_C "(inf)", origin_a_.get(),
      origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kOversizedImages,
            parsed_policy[0].feature);
  EXPECT_EQ(default_double_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(default_double_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(2UL, parsed_policy[0].values.size());
  origin_and_value = parsed_policy[0].values.begin();
  EXPECT_TRUE(origin_and_value->first.IsSameOriginWith(expected_url_origin_a_));
  EXPECT_EQ(sample_double_value, origin_and_value->second);
  origin_and_value++;
  EXPECT_TRUE(origin_and_value->first.IsSameOriginWith(expected_url_origin_c_));
  EXPECT_EQ(max_double_value, origin_and_value->second);

  // Test policy: 'self'(1.5) https://example.org(inf) *(0)
  // Fallbacks should be 0.
  parsed_policy = ParseFeaturePolicy(
      "oversized-images 'self'(1.5) " ORIGIN_C "(inf) *(0)", origin_a_.get(),
      origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kOversizedImages,
            parsed_policy[0].feature);
  EXPECT_EQ(min_double_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(min_double_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(2UL, parsed_policy[0].values.size());
  origin_and_value = parsed_policy[0].values.begin();
  EXPECT_TRUE(origin_and_value->first.IsSameOriginWith(expected_url_origin_a_));
  EXPECT_EQ(sample_double_value, origin_and_value->second);
  origin_and_value++;
  EXPECT_TRUE(origin_and_value->first.IsSameOriginWith(expected_url_origin_c_));
  EXPECT_EQ(max_double_value, origin_and_value->second);
}

TEST_F(FeaturePolicyParserTest, RedundantBooleanItemsRemoved) {
  Vector<String> messages;
  ParsedFeaturePolicy parsed_policy;

  // 'self'(true) *(true)
  parsed_policy =
      ParseFeaturePolicy("fullscreen 'self'(true) *(true)", origin_a_.get(),
                         origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, parsed_policy[0].feature);
  EXPECT_EQ(max_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(max_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());

  // 'self'(false)
  parsed_policy =
      ParseFeaturePolicy("fullscreen 'self'(false)", origin_a_.get(),
                         origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, parsed_policy[0].feature);
  EXPECT_EQ(min_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(min_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());

  // (true)
  parsed_policy =
      ParseFeaturePolicy("fullscreen (false)", origin_a_.get(), origin_b_.get(),
                         &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kFullscreen, parsed_policy[0].feature);
  EXPECT_EQ(min_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(min_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());
}

TEST_F(FeaturePolicyParserTest, RedundantDoubleItemsRemoved) {
  Vector<String> messages;
  ParsedFeaturePolicy parsed_policy;

  // 'self'(1.5) *(1.5)
  parsed_policy =
      ParseFeaturePolicy("oversized-images 'self'(1.5) *(1.5)", origin_a_.get(),
                         origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kOversizedImages,
            parsed_policy[0].feature);
  EXPECT_EQ(sample_double_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(sample_double_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());

  // 'self'(inf)
  parsed_policy =
      ParseFeaturePolicy("oversized-images 'self'(2.0)", origin_a_.get(),
                         origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kOversizedImages,
            parsed_policy[0].feature);
  EXPECT_EQ(default_double_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(default_double_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());

  // (inf)
  parsed_policy =
      ParseFeaturePolicy("oversized-images (2.0)", origin_a_.get(),
                         origin_b_.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kOversizedImages,
            parsed_policy[0].feature);
  EXPECT_EQ(default_double_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(default_double_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());
}

// Test histogram counting the use of feature policies in header.
TEST_F(FeaturePolicyParserTest, HeaderHistogram) {
  const char* histogram_name = "Blink.UseCounter.FeaturePolicy.Header";
  HistogramTester tester;
  Vector<String> messages;

  ParseFeaturePolicy("payment; fullscreen", origin_a_.get(), nullptr, &messages,
                     test_feature_name_map);
  tester.ExpectTotalCount(histogram_name, 2);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kPayment), 1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kFullscreen), 1);
}

// Test counting the use of each feature policy only once per header.
TEST_F(FeaturePolicyParserTest, HistogramMultiple) {
  const char* histogram_name = "Blink.UseCounter.FeaturePolicy.Header";
  HistogramTester tester;
  Vector<String> messages;

  // If the same feature is listed multiple times, it should only be counted
  // once.
  ParseFeaturePolicy("geolocation 'self'; payment; geolocation *",
                     origin_a_.get(), nullptr, &messages,
                     test_feature_name_map);
  ParseFeaturePolicy("fullscreen 'self', fullscreen *", origin_a_.get(),
                     nullptr, &messages, test_feature_name_map);
  tester.ExpectTotalCount(histogram_name, 3);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kGeolocation), 1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kFullscreen), 1);
}

// Test histogram counting the use of feature policies via "allow"
// attribute. This test parses two policies on the same document.
TEST_F(FeaturePolicyParserTest, AllowHistogramSameDocument) {
  const char* histogram_name = "Blink.UseCounter.FeaturePolicy.Allow";
  HistogramTester tester;
  Vector<String> messages;
  auto dummy = std::make_unique<DummyPageHolder>();

  ParseFeaturePolicy("payment; fullscreen", origin_a_.get(), origin_b_.get(),
                     &messages, test_feature_name_map, &dummy->GetDocument());
  ParseFeaturePolicy("fullscreen; geolocation", origin_a_.get(),
                     origin_b_.get(), &messages, test_feature_name_map,
                     &dummy->GetDocument());
  tester.ExpectTotalCount(histogram_name, 3);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kPayment), 1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kFullscreen), 1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kGeolocation), 1);
}

// Test histogram counting the use of feature policies via "allow"
// attribute. This test parses two policies on different documents.
TEST_F(FeaturePolicyParserTest, AllowHistogramDifferentDocument) {
  const char* histogram_name = "Blink.UseCounter.FeaturePolicy.Allow";
  HistogramTester tester;
  Vector<String> messages;
  auto dummy = std::make_unique<DummyPageHolder>();
  auto dummy2 = std::make_unique<DummyPageHolder>();

  ParseFeaturePolicy("payment; fullscreen", origin_a_.get(), origin_b_.get(),
                     &messages, test_feature_name_map, &dummy->GetDocument());
  ParseFeaturePolicy("fullscreen; geolocation", origin_a_.get(),
                     origin_b_.get(), &messages, test_feature_name_map,
                     &dummy2->GetDocument());
  tester.ExpectTotalCount(histogram_name, 4);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kPayment), 1);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kFullscreen), 2);
  tester.ExpectBucketCount(
      histogram_name,
      static_cast<int>(blink::mojom::FeaturePolicyFeature::kGeolocation), 1);
}

TEST_F(FeaturePolicyParserTest, ParseParameterizedFeatures) {
  Vector<String> messages;

  scoped_refptr<SecurityOrigin> opaque_origin =
      SecurityOrigin::CreateUniqueOpaque();

  // Simple policy with *.
  ParsedFeaturePolicy parsed_policy =
      ParseFeaturePolicy("oversized-images *", origin_a_.get(),
                         opaque_origin.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());
  EXPECT_EQ(mojom::FeaturePolicyFeature::kOversizedImages,
            parsed_policy[0].feature);
  EXPECT_EQ(max_double_value, parsed_policy[0].fallback_value);
  EXPECT_EQ(max_double_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(0UL, parsed_policy[0].values.size());

  // Policy with explicit origins
  parsed_policy = ParseFeaturePolicy(
      "oversized-images https://example.net 'src'", origin_a_.get(),
      opaque_origin.get(), &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(mojom::FeaturePolicyFeature::kOversizedImages,
            parsed_policy[0].feature);
  EXPECT_GE(default_double_value, parsed_policy[0].fallback_value);
  EXPECT_LE(max_double_value, parsed_policy[0].opaque_value);
  EXPECT_EQ(1UL, parsed_policy[0].values.size());
  EXPECT_LE(max_double_value, parsed_policy[0].values.begin()->second);
}

// Test policy mutation methods
class FeaturePolicyMutationTest : public testing::Test {
 protected:
  FeaturePolicyMutationTest() = default;

  ~FeaturePolicyMutationTest() override = default;

  url::Origin url_origin_a_ = url::Origin::Create(GURL(ORIGIN_A));
  url::Origin url_origin_b_ = url::Origin::Create(GURL(ORIGIN_B));
  url::Origin url_origin_c_ = url::Origin::Create(GURL(ORIGIN_C));

  // Returns true if the policy contains a declaration for the feature which
  // allows it in all origins.
  bool IsFeatureAllowedEverywhere(mojom::FeaturePolicyFeature feature,
                                  const ParsedFeaturePolicy& policy) {
    const auto& result = std::find_if(policy.begin(), policy.end(),
                                      [feature](const auto& declaration) {
                                        return declaration.feature == feature;
                                      });
    if (result == policy.end())
      return false;

    return result->feature == feature && result->fallback_value >= max_value &&
           result->opaque_value >= max_value && result->values.empty();
    return true;
  }

  // Returns true if the policy contains a declaration for the feature which
  // disallows it in all origins.
  bool IsFeatureDisallowedEverywhere(mojom::FeaturePolicyFeature feature,
                                     const ParsedFeaturePolicy& policy) {
    const auto& result = std::find_if(policy.begin(), policy.end(),
                                      [feature](const auto& declaration) {
                                        return declaration.feature == feature;
                                      });
    if (result == policy.end())
      return false;

    return result->feature == feature && result->fallback_value <= min_value &&
           result->opaque_value <= min_value && result->values.empty();
    return true;
  }

  const PolicyValue min_value = PolicyValue(false);
  const PolicyValue max_value = PolicyValue(true);
  const PolicyValue min_double_value =
      PolicyValue(2.0, mojom::PolicyValueType::kDecDouble);
  const PolicyValue max_double_value =
      PolicyValue::CreateMaxPolicyValue(mojom::PolicyValueType::kDecDouble);

  ParsedFeaturePolicy test_policy = {
      {mojom::FeaturePolicyFeature::kFullscreen,
       std::map<url::Origin, PolicyValue>{{url_origin_a_, PolicyValue(true)},
                                          {url_origin_b_, PolicyValue(true)}},
       PolicyValue(false), PolicyValue(false)},
      {mojom::FeaturePolicyFeature::kGeolocation,
       std::map<url::Origin, PolicyValue>{{url_origin_a_, PolicyValue(true)}},
       PolicyValue(false), PolicyValue(false)}};

  ParsedFeaturePolicy empty_policy = {};
};

TEST_F(FeaturePolicyMutationTest, TestIsFeatureDeclared) {
  EXPECT_TRUE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kFullscreen, test_policy));
  EXPECT_TRUE(IsFeatureDeclared(mojom::FeaturePolicyFeature::kGeolocation,
                                test_policy));
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kUsb, test_policy));
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kNotFound, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestIsFeatureDeclaredWithEmptyPolicy) {
  EXPECT_FALSE(IsFeatureDeclared(mojom::FeaturePolicyFeature::kFullscreen,
                                 empty_policy));
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kNotFound, empty_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveAbsentFeature) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kPayment, test_policy));
  EXPECT_FALSE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kPayment,
                                      test_policy));
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kPayment, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveFromEmptyPolicy) {
  ASSERT_EQ(0UL, empty_policy.size());
  EXPECT_FALSE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kPayment,
                                      test_policy));
  ASSERT_EQ(0UL, empty_policy.size());
}

TEST_F(FeaturePolicyMutationTest, TestRemoveFeatureIfPresent) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_TRUE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kFullscreen, test_policy));
  EXPECT_TRUE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kFullscreen,
                                     test_policy));
  EXPECT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kFullscreen, test_policy));

  // Attempt to remove the feature again
  EXPECT_FALSE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kFullscreen,
                                      test_policy));
  EXPECT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kFullscreen, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveFeatureIfPresentOnSecondFeature) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_TRUE(IsFeatureDeclared(mojom::FeaturePolicyFeature::kGeolocation,
                                test_policy));
  EXPECT_TRUE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kGeolocation,
                                     test_policy));
  ASSERT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(mojom::FeaturePolicyFeature::kGeolocation,
                                 test_policy));

  // Attempt to remove the feature again
  EXPECT_FALSE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kGeolocation,
                                      test_policy));
  EXPECT_EQ(1UL, test_policy.size());
  EXPECT_FALSE(IsFeatureDeclared(mojom::FeaturePolicyFeature::kGeolocation,
                                 test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestRemoveAllFeatures) {
  ASSERT_EQ(2UL, test_policy.size());
  EXPECT_TRUE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kFullscreen,
                                     test_policy));
  EXPECT_TRUE(RemoveFeatureIfPresent(mojom::FeaturePolicyFeature::kGeolocation,
                                     test_policy));
  EXPECT_EQ(0UL, test_policy.size());
  EXPECT_FALSE(
      IsFeatureDeclared(mojom::FeaturePolicyFeature::kFullscreen, test_policy));
  EXPECT_FALSE(IsFeatureDeclared(mojom::FeaturePolicyFeature::kGeolocation,
                                 test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestDisallowIfNotPresent) {
  ParsedFeaturePolicy copy = test_policy;
  // Try to disallow a feature which already exists
  EXPECT_FALSE(DisallowFeatureIfNotPresent(
      mojom::FeaturePolicyFeature::kFullscreen, copy));
  ASSERT_EQ(copy, test_policy);

  // Disallow a new feature
  EXPECT_TRUE(
      DisallowFeatureIfNotPresent(mojom::FeaturePolicyFeature::kPayment, copy));
  EXPECT_EQ(3UL, copy.size());
  // Verify that the feature is, in fact, now disallowed everywhere
  EXPECT_TRUE(IsFeatureDisallowedEverywhere(
      mojom::FeaturePolicyFeature::kPayment, copy));
}

TEST_F(FeaturePolicyMutationTest, TestAllowEverywhereIfNotPresent) {
  ParsedFeaturePolicy copy = test_policy;
  // Try to allow a feature which already exists
  EXPECT_FALSE(AllowFeatureEverywhereIfNotPresent(
      mojom::FeaturePolicyFeature::kFullscreen, copy));
  ASSERT_EQ(copy, test_policy);

  // Allow a new feature
  EXPECT_TRUE(AllowFeatureEverywhereIfNotPresent(
      mojom::FeaturePolicyFeature::kPayment, copy));
  EXPECT_EQ(3UL, copy.size());
  // Verify that the feature is, in fact, allowed everywhere
  EXPECT_TRUE(
      IsFeatureAllowedEverywhere(mojom::FeaturePolicyFeature::kPayment, copy));
}

TEST_F(FeaturePolicyMutationTest, TestDisallowUnconditionally) {
  // Try to disallow a feature which already exists
  DisallowFeature(mojom::FeaturePolicyFeature::kFullscreen, test_policy);
  // Should not have changed the number of declarations
  EXPECT_EQ(2UL, test_policy.size());
  // Verify that the feature is, in fact, now disallowed everywhere
  EXPECT_TRUE(IsFeatureDisallowedEverywhere(
      mojom::FeaturePolicyFeature::kFullscreen, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestDisallowNewFeatureUnconditionally) {
  // Try to disallow a feature which does not yet exist
  DisallowFeature(mojom::FeaturePolicyFeature::kPayment, test_policy);
  // Should have added a new declaration
  EXPECT_EQ(3UL, test_policy.size());
  // Verify that the feature is, in fact, now disallowed everywhere
  EXPECT_TRUE(IsFeatureDisallowedEverywhere(
      mojom::FeaturePolicyFeature::kPayment, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestAllowUnconditionally) {
  // Try to allow a feature which already exists
  AllowFeatureEverywhere(mojom::FeaturePolicyFeature::kFullscreen, test_policy);
  // Should not have changed the number of declarations
  EXPECT_EQ(2UL, test_policy.size());
  // Verify that the feature is, in fact, now allowed everywhere
  EXPECT_TRUE(IsFeatureAllowedEverywhere(
      mojom::FeaturePolicyFeature::kFullscreen, test_policy));
}

TEST_F(FeaturePolicyMutationTest, TestAllowNewFeatureUnconditionally) {
  // Try to allow a feature which does not yet exist
  AllowFeatureEverywhere(mojom::FeaturePolicyFeature::kPayment, test_policy);
  // Should have added a new declaration
  EXPECT_EQ(3UL, test_policy.size());
  // Verify that the feature is, in fact, now allowed everywhere
  EXPECT_TRUE(IsFeatureAllowedEverywhere(mojom::FeaturePolicyFeature::kPayment,
                                         test_policy));
}

class FeaturePolicyViolationHistogramTest : public testing::Test {
 protected:
  FeaturePolicyViolationHistogramTest() = default;

  ~FeaturePolicyViolationHistogramTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FeaturePolicyViolationHistogramTest);
};

TEST_F(FeaturePolicyViolationHistogramTest, PotentialViolation) {
  HistogramTester tester;
  const char* histogram_name =
      "Blink.UseCounter.FeaturePolicy.PotentialViolation";
  auto dummy_page_holder_ = std::make_unique<DummyPageHolder>();
  // Probing feature state should not count.
  dummy_page_holder_->GetDocument().IsFeatureEnabled(
      mojom::FeaturePolicyFeature::kPayment);
  tester.ExpectTotalCount(histogram_name, 0);
  // Checking the feature state with reporting intent should record a potential
  // violation.
  dummy_page_holder_->GetDocument().IsFeatureEnabled(
      mojom::FeaturePolicyFeature::kPayment, ReportOptions::kReportOnFailure);
  tester.ExpectTotalCount(histogram_name, 1);
  // The potential violation for an already recorded violation does not count
  // again.
  dummy_page_holder_->GetDocument().IsFeatureEnabled(
      mojom::FeaturePolicyFeature::kPayment, ReportOptions::kReportOnFailure);
  tester.ExpectTotalCount(histogram_name, 1);
  // Sanity check: check some other feature to increase the count.
  dummy_page_holder_->GetDocument().IsFeatureEnabled(
      mojom::FeaturePolicyFeature::kFullscreen,
      ReportOptions::kReportOnFailure);
  tester.ExpectTotalCount(histogram_name, 2);
}

}  // namespace blink
