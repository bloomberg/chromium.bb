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
    "{\"feature\": []}",
    "{\"feature\": [\"self\"]}",
    "{\"feature\": [\"*\"]}",
    "{\"feature\": [\"" ORIGIN_A "\"]}",
    "{\"feature\": [\"" ORIGIN_B "\"]}",
    "{\"feature\": [\"" ORIGIN_A "\", \"" ORIGIN_B "\"]}",
    "{\"feature1\": [\"" ORIGIN_A "\"], \"feature2\": [\"self\"]}",
    "{\"feature1\": [\"" ORIGIN_A "\"]}, {\"feature2\": [\"self\"]}"};

const char* kInvalidPolicies[] = {
    "Not A JSON literal",
    "\"Not a JSON object\"",
    "[\"Also\", \"Not a JSON object\"]",
    "1.0",
    "{\"Whitelist\": \"Not a JSON array\"}",
    "{\"feature1\": [\"*\"], \"feature2\": \"Not an array\"}"};

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
  parsedPolicy = parseFeaturePolicy("{\"default-self\": [\"self\"]}",
                                    m_originA.get(), &messages);
  EXPECT_EQ(1UL, parsedPolicy.size());
  EXPECT_EQ("default-self", parsedPolicy[0].featureName);
  EXPECT_FALSE(parsedPolicy[0].matchesAllOrigins);
  EXPECT_EQ(1UL, parsedPolicy[0].origins.size());
  EXPECT_TRUE(m_originA->isSameSchemeHostPortAndSuborigin(
      parsedPolicy[0].origins[0].get()));

  // Simple policy with *.
  parsedPolicy = parseFeaturePolicy("{\"default-self\": [\"*\"]}",
                                    m_originA.get(), &messages);
  EXPECT_EQ(1UL, parsedPolicy.size());
  EXPECT_EQ("default-self", parsedPolicy[0].featureName);
  EXPECT_TRUE(parsedPolicy[0].matchesAllOrigins);
  EXPECT_EQ(0UL, parsedPolicy[0].origins.size());

  // Complicated policy.
  parsedPolicy = parseFeaturePolicy(
      "{\"default-self\": [\"*\"], "
      "\"default-on\": [\"https://example.net\", \"https://example.org\"], "
      "\"default-off\": [\"self\"]}",
      m_originA.get(), &messages);
  EXPECT_EQ(3UL, parsedPolicy.size());
  EXPECT_EQ("default-self", parsedPolicy[0].featureName);
  EXPECT_TRUE(parsedPolicy[0].matchesAllOrigins);
  EXPECT_EQ(0UL, parsedPolicy[0].origins.size());
  EXPECT_EQ("default-on", parsedPolicy[1].featureName);
  EXPECT_FALSE(parsedPolicy[1].matchesAllOrigins);
  EXPECT_EQ(2UL, parsedPolicy[1].origins.size());
  EXPECT_TRUE(m_originB->isSameSchemeHostPortAndSuborigin(
      parsedPolicy[1].origins[0].get()));
  EXPECT_TRUE(m_originC->isSameSchemeHostPortAndSuborigin(
      parsedPolicy[1].origins[1].get()));
  EXPECT_EQ("default-off", parsedPolicy[2].featureName);
  EXPECT_FALSE(parsedPolicy[2].matchesAllOrigins);
  EXPECT_EQ(1UL, parsedPolicy[2].origins.size());
  EXPECT_TRUE(m_originA->isSameSchemeHostPortAndSuborigin(
      parsedPolicy[2].origins[0].get()));
}

}  // namespace blink
