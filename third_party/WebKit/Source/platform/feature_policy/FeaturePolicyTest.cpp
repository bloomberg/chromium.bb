// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/feature_policy/FeaturePolicy.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "testing/gtest/include/gtest/gtest.h"

// Origin strings used for tests
#define ORIGIN_A "https://example.com/"
#define ORIGIN_B "https://example.net/"
#define ORIGIN_C "https://example.org/"

namespace blink {

namespace {

const char* const kValidPolicies[] = {
    "{\"vibrate\": []}",
    "{\"vibrate\": [\"self\"]}",
    "{\"vibrate\": [\"*\"]}",
    "{\"vibrate\": [\"" ORIGIN_A "\"]}",
    "{\"vibrate\": [\"" ORIGIN_B "\"]}",
    "{\"vibrate\": [\"" ORIGIN_A "\", \"" ORIGIN_B "\"]}",
    "{\"fullscreen\": [\"" ORIGIN_A "\"], \"payment\": [\"self\"]}",
    "{\"fullscreen\": [\"" ORIGIN_A "\"]}, {\"payment\": [\"self\"]}"};

const char* const kInvalidPolicies[] = {
    "Not A JSON literal",
    "\"Not a JSON object\"",
    "[\"Also\", \"Not a JSON object\"]",
    "1.0",
    "{\"vibrate\": \"Not a JSON array\"}",
    "{\"vibrate\": [\"*\"], \"payment\": \"Not a JSON array\"}"};

}  // namespace

class FeaturePolicyTest : public ::testing::Test {
 protected:
  FeaturePolicyTest() {}

  ~FeaturePolicyTest() {
  }

  RefPtr<SecurityOrigin> origin_a_ = SecurityOrigin::CreateFromString(ORIGIN_A);
  RefPtr<SecurityOrigin> origin_b_ = SecurityOrigin::CreateFromString(ORIGIN_B);
  RefPtr<SecurityOrigin> origin_c_ = SecurityOrigin::CreateFromString(ORIGIN_C);

  const FeatureNameMap test_feature_name_map = {
      {"fullscreen", blink::WebFeaturePolicyFeature::kFullscreen},
      {"payment", blink::WebFeaturePolicyFeature::kPayment},
      {"vibrate", blink::WebFeaturePolicyFeature::kVibrate}};
};

TEST_F(FeaturePolicyTest, ParseValidPolicy) {
  Vector<String> messages;
  for (const char* policy_string : kValidPolicies) {
    messages.clear();
    ParseFeaturePolicy(policy_string, origin_a_.Get(), &messages,
                       test_feature_name_map);
    EXPECT_EQ(0UL, messages.size());
  }
}

TEST_F(FeaturePolicyTest, ParseInvalidPolicy) {
  Vector<String> messages;
  for (const char* policy_string : kInvalidPolicies) {
    messages.clear();
    ParseFeaturePolicy(policy_string, origin_a_.Get(), &messages,
                       test_feature_name_map);
    EXPECT_NE(0UL, messages.size());
  }
}

TEST_F(FeaturePolicyTest, PolicyParsedCorrectly) {
  Vector<String> messages;

  // Empty policy.
  WebParsedFeaturePolicy parsed_policy = ParseFeaturePolicy(
      "{}", origin_a_.Get(), &messages, test_feature_name_map);
  EXPECT_EQ(0UL, parsed_policy.size());

  // Simple policy with "self".
  parsed_policy =
      ParseFeaturePolicy("{\"vibrate\": [\"self\"]}", origin_a_.Get(),
                         &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());

  EXPECT_EQ(WebFeaturePolicyFeature::kVibrate, parsed_policy[0].feature);
  EXPECT_FALSE(parsed_policy[0].matches_all_origins);
  EXPECT_EQ(1UL, parsed_policy[0].origins.size());
  EXPECT_TRUE(origin_a_->IsSameSchemeHostPortAndSuborigin(
      parsed_policy[0].origins[0].Get()));
  // Simple policy with *.
  parsed_policy = ParseFeaturePolicy("{\"vibrate\": [\"*\"]}", origin_a_.Get(),
                                     &messages, test_feature_name_map);
  EXPECT_EQ(1UL, parsed_policy.size());
  EXPECT_EQ(WebFeaturePolicyFeature::kVibrate, parsed_policy[0].feature);
  EXPECT_TRUE(parsed_policy[0].matches_all_origins);
  EXPECT_EQ(0UL, parsed_policy[0].origins.size());

  // Complicated policy.
  parsed_policy = ParseFeaturePolicy(
      "{\"vibrate\": [\"*\"], "
      "\"fullscreen\": [\"https://example.net\", \"https://example.org\"], "
      "\"payment\": [\"self\"]}",
      origin_a_.Get(), &messages, test_feature_name_map);
  EXPECT_EQ(3UL, parsed_policy.size());
  EXPECT_EQ(WebFeaturePolicyFeature::kVibrate, parsed_policy[0].feature);
  EXPECT_TRUE(parsed_policy[0].matches_all_origins);
  EXPECT_EQ(0UL, parsed_policy[0].origins.size());
  EXPECT_EQ(WebFeaturePolicyFeature::kFullscreen, parsed_policy[1].feature);
  EXPECT_FALSE(parsed_policy[1].matches_all_origins);
  EXPECT_EQ(2UL, parsed_policy[1].origins.size());
  EXPECT_TRUE(origin_b_->IsSameSchemeHostPortAndSuborigin(
      parsed_policy[1].origins[0].Get()));
  EXPECT_TRUE(origin_c_->IsSameSchemeHostPortAndSuborigin(
      parsed_policy[1].origins[1].Get()));
  EXPECT_EQ(WebFeaturePolicyFeature::kPayment, parsed_policy[2].feature);
  EXPECT_FALSE(parsed_policy[2].matches_all_origins);
  EXPECT_EQ(1UL, parsed_policy[2].origins.size());
  EXPECT_TRUE(origin_a_->IsSameSchemeHostPortAndSuborigin(
      parsed_policy[2].origins[0].Get()));
}

TEST_F(FeaturePolicyTest, ParseEmptyContainerPolicy) {
  WebParsedFeaturePolicy container_policy =
      GetContainerPolicyFromAllowedFeatures(
          std::vector<WebFeaturePolicyFeature>({}), false, false,
          origin_a_.Get());
  EXPECT_EQ(0UL, container_policy.size());
}

TEST_F(FeaturePolicyTest, ParseContainerPolicy) {
  WebParsedFeaturePolicy container_policy =
      GetContainerPolicyFromAllowedFeatures(
          std::vector<WebFeaturePolicyFeature>(
              {WebFeaturePolicyFeature::kVibrate,
               WebFeaturePolicyFeature::kPayment}),
          false, false, origin_a_.Get());
  EXPECT_EQ(2UL, container_policy.size());
  EXPECT_EQ(WebFeaturePolicyFeature::kVibrate, container_policy[0].feature);
  EXPECT_FALSE(container_policy[0].matches_all_origins);
  EXPECT_EQ(1UL, container_policy[0].origins.size());
  EXPECT_TRUE(origin_a_->IsSameSchemeHostPortAndSuborigin(
      container_policy[0].origins[0].Get()));
  EXPECT_EQ(WebFeaturePolicyFeature::kPayment, container_policy[1].feature);
  EXPECT_FALSE(container_policy[1].matches_all_origins);
  EXPECT_EQ(1UL, container_policy[1].origins.size());
  EXPECT_TRUE(origin_a_->IsSameSchemeHostPortAndSuborigin(
      container_policy[1].origins[0].Get()));
}

TEST_F(FeaturePolicyTest, ParseContainerPolicyWithAllowFullscreen) {
  WebParsedFeaturePolicy container_policy =
      GetContainerPolicyFromAllowedFeatures(
          std::vector<WebFeaturePolicyFeature>({}), true, false,
          origin_a_.Get());
  EXPECT_EQ(1UL, container_policy.size());
  EXPECT_EQ(WebFeaturePolicyFeature::kFullscreen, container_policy[0].feature);
  EXPECT_TRUE(container_policy[0].matches_all_origins);
}

TEST_F(FeaturePolicyTest, ParseContainerPolicyWithAllowPaymentRequest) {
  WebParsedFeaturePolicy container_policy =
      GetContainerPolicyFromAllowedFeatures(
          std::vector<WebFeaturePolicyFeature>(
              {WebFeaturePolicyFeature::kVibrate}),
          false, true, origin_a_.Get());
  EXPECT_EQ(2UL, container_policy.size());
  EXPECT_EQ(WebFeaturePolicyFeature::kVibrate, container_policy[0].feature);
  EXPECT_FALSE(container_policy[0].matches_all_origins);
  EXPECT_EQ(1UL, container_policy[0].origins.size());
  EXPECT_TRUE(origin_a_->IsSameSchemeHostPortAndSuborigin(
      container_policy[0].origins[0].Get()));
  EXPECT_EQ(WebFeaturePolicyFeature::kPayment, container_policy[1].feature);
  EXPECT_TRUE(container_policy[1].matches_all_origins);
}

TEST_F(FeaturePolicyTest, ParseContainerPolicyWithAllowAttributes) {
  WebParsedFeaturePolicy container_policy =
      GetContainerPolicyFromAllowedFeatures(
          std::vector<WebFeaturePolicyFeature>(
              {WebFeaturePolicyFeature::kVibrate,
               WebFeaturePolicyFeature::kPayment}),
          true, true, origin_a_.Get());
  EXPECT_EQ(3UL, container_policy.size());
  EXPECT_EQ(WebFeaturePolicyFeature::kVibrate, container_policy[0].feature);
  EXPECT_FALSE(container_policy[0].matches_all_origins);
  EXPECT_EQ(1UL, container_policy[0].origins.size());
  EXPECT_TRUE(origin_a_->IsSameSchemeHostPortAndSuborigin(
      container_policy[0].origins[0].Get()));
  EXPECT_EQ(WebFeaturePolicyFeature::kPayment, container_policy[1].feature);
  EXPECT_FALSE(container_policy[1].matches_all_origins);
  EXPECT_EQ(1UL, container_policy[1].origins.size());
  EXPECT_TRUE(origin_a_->IsSameSchemeHostPortAndSuborigin(
      container_policy[1].origins[0].Get()));
  EXPECT_EQ(WebFeaturePolicyFeature::kFullscreen, container_policy[2].feature);
  EXPECT_TRUE(container_policy[2].matches_all_origins);
}

}  // namespace blink
