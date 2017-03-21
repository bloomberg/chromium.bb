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

const char* kValidPolicies[] = {
    "{\"vibrate\": []}",
    "{\"vibrate\": [\"self\"]}",
    "{\"vibrate\": [\"*\"]}",
    "{\"vibrate\": [\"" ORIGIN_A "\"]}",
    "{\"vibrate\": [\"" ORIGIN_B "\"]}",
    "{\"vibrate\": [\"" ORIGIN_A "\", \"" ORIGIN_B "\"]}",
    "{\"fullscreen\": [\"" ORIGIN_A "\"], \"payment\": [\"self\"]}",
    "{\"fullscreen\": [\"" ORIGIN_A "\"]}, {\"payment\": [\"self\"]}"};

const char* kInvalidPolicies[] = {
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

  RefPtr<SecurityOrigin> m_originA = SecurityOrigin::createFromString(ORIGIN_A);
  RefPtr<SecurityOrigin> m_originB = SecurityOrigin::createFromString(ORIGIN_B);
  RefPtr<SecurityOrigin> m_originC = SecurityOrigin::createFromString(ORIGIN_C);
};

TEST_F(FeaturePolicyTest, ParseValidPolicy) {
  Vector<String> messages;
  for (const char* policyString : kValidPolicies) {
    messages.clear();
    parseFeaturePolicy(policyString, m_originA.get(), &messages);
    EXPECT_EQ(0UL, messages.size());
  }
}

TEST_F(FeaturePolicyTest, ParseInvalidPolicy) {
  Vector<String> messages;
  for (const char* policyString : kInvalidPolicies) {
    messages.clear();
    parseFeaturePolicy(policyString, m_originA.get(), &messages);
    EXPECT_NE(0UL, messages.size());
  }
}

TEST_F(FeaturePolicyTest, PolicyParsedCorrectly) {
  Vector<String> messages;

  // Empty policy.
  WebParsedFeaturePolicy parsedPolicy =
      parseFeaturePolicy("{}", m_originA.get(), &messages);
  EXPECT_EQ(0UL, parsedPolicy.size());

  // Simple policy with "self".
  parsedPolicy = parseFeaturePolicy("{\"vibrate\": [\"self\"]}",
                                    m_originA.get(), &messages);
  EXPECT_EQ(1UL, parsedPolicy.size());
  EXPECT_EQ(WebFeaturePolicyFeature::Vibrate, parsedPolicy[0].feature);
  EXPECT_FALSE(parsedPolicy[0].matchesAllOrigins);
  EXPECT_EQ(1UL, parsedPolicy[0].origins.size());
  EXPECT_TRUE(m_originA->isSameSchemeHostPortAndSuborigin(
      parsedPolicy[0].origins[0].get()));

  // Simple policy with *.
  parsedPolicy =
      parseFeaturePolicy("{\"vibrate\": [\"*\"]}", m_originA.get(), &messages);
  EXPECT_EQ(1UL, parsedPolicy.size());
  EXPECT_EQ(WebFeaturePolicyFeature::Vibrate, parsedPolicy[0].feature);
  EXPECT_TRUE(parsedPolicy[0].matchesAllOrigins);
  EXPECT_EQ(0UL, parsedPolicy[0].origins.size());

  // Complicated policy.
  parsedPolicy = parseFeaturePolicy(
      "{\"vibrate\": [\"*\"], "
      "\"fullscreen\": [\"https://example.net\", \"https://example.org\"], "
      "\"payment\": [\"self\"]}",
      m_originA.get(), &messages);
  EXPECT_EQ(3UL, parsedPolicy.size());
  EXPECT_EQ(WebFeaturePolicyFeature::Vibrate, parsedPolicy[0].feature);
  EXPECT_TRUE(parsedPolicy[0].matchesAllOrigins);
  EXPECT_EQ(0UL, parsedPolicy[0].origins.size());
  EXPECT_EQ(WebFeaturePolicyFeature::Fullscreen, parsedPolicy[1].feature);
  EXPECT_FALSE(parsedPolicy[1].matchesAllOrigins);
  EXPECT_EQ(2UL, parsedPolicy[1].origins.size());
  EXPECT_TRUE(m_originB->isSameSchemeHostPortAndSuborigin(
      parsedPolicy[1].origins[0].get()));
  EXPECT_TRUE(m_originC->isSameSchemeHostPortAndSuborigin(
      parsedPolicy[1].origins[1].get()));
  EXPECT_EQ(WebFeaturePolicyFeature::Payment, parsedPolicy[2].feature);
  EXPECT_FALSE(parsedPolicy[2].matchesAllOrigins);
  EXPECT_EQ(1UL, parsedPolicy[2].origins.size());
  EXPECT_TRUE(m_originA->isSameSchemeHostPortAndSuborigin(
      parsedPolicy[2].origins[0].get()));
}

TEST_F(FeaturePolicyTest, ParseEmptyContainerPolicy) {
  WebParsedFeaturePolicy containerPolicy =
      getContainerPolicyFromAllowedFeatures(
          std::vector<WebFeaturePolicyFeature>({}), m_originA.get());
  EXPECT_EQ(0UL, containerPolicy.size());
}

TEST_F(FeaturePolicyTest, ParseContainerPolicy) {
  WebParsedFeaturePolicy containerPolicy =
      getContainerPolicyFromAllowedFeatures(
          std::vector<WebFeaturePolicyFeature>(
              {WebFeaturePolicyFeature::Vibrate,
               WebFeaturePolicyFeature::Payment}),
          m_originA.get());
  EXPECT_EQ(2UL, containerPolicy.size());
  EXPECT_EQ(WebFeaturePolicyFeature::Vibrate, containerPolicy[0].feature);
  EXPECT_FALSE(containerPolicy[0].matchesAllOrigins);
  EXPECT_EQ(1UL, containerPolicy[0].origins.size());
  EXPECT_TRUE(m_originA->isSameSchemeHostPortAndSuborigin(
      containerPolicy[0].origins[0].get()));
  EXPECT_EQ(WebFeaturePolicyFeature::Payment, containerPolicy[1].feature);
  EXPECT_FALSE(containerPolicy[1].matchesAllOrigins);
  EXPECT_EQ(1UL, containerPolicy[1].origins.size());
  EXPECT_TRUE(m_originA->isSameSchemeHostPortAndSuborigin(
      containerPolicy[1].origins[0].get()));
}

}  // namespace blink
