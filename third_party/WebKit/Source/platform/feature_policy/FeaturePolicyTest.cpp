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

// This is an example of a feature which should be enabled by default in all
// frames.
const FeaturePolicy::Feature kDefaultOnFeature{
    "default-on", FeaturePolicy::FeatureDefault::EnableForAll};

// This is an example of a feature which should be enabled in top-level frames,
// and same-origin child-frames, but must be delegated to all cross-origin
// frames explicitly.
const FeaturePolicy::Feature kDefaultSelfFeature{
    "default-self", FeaturePolicy::FeatureDefault::EnableForSelf};

// This is an example of a feature which should be disabled by default, both in
// top-level and nested frames.
const FeaturePolicy::Feature kDefaultOffFeature{
    "default-off", FeaturePolicy::FeatureDefault::DisableForAll};

}  // namespace

class FeaturePolicyTest : public ::testing::Test {
 protected:
  FeaturePolicyTest()
      : m_frameworkWasEnabled(RuntimeEnabledFeatures::featurePolicyEnabled()),
        m_featureList(
            {&kDefaultOnFeature, &kDefaultSelfFeature, &kDefaultOffFeature}) {
    RuntimeEnabledFeatures::setFeaturePolicyEnabled(true);
  }

  ~FeaturePolicyTest() {
    RuntimeEnabledFeatures::setFeaturePolicyEnabled(m_frameworkWasEnabled);
  }

  std::unique_ptr<FeaturePolicy> createFromParentPolicy(
      const FeaturePolicy* parent,
      RefPtr<SecurityOrigin> origin) {
    return FeaturePolicy::createFromParentPolicy(parent, nullptr, origin,
                                                 m_featureList);
  }

  std::unique_ptr<FeaturePolicy> createFromParentWithFramePolicy(
      const FeaturePolicy* parent,
      const WebParsedFeaturePolicyHeader* framePolicy,
      RefPtr<SecurityOrigin> origin) {
    return FeaturePolicy::createFromParentPolicy(parent, framePolicy, origin,
                                                 m_featureList);
  }

  RefPtr<SecurityOrigin> m_originA = SecurityOrigin::createFromString(ORIGIN_A);
  RefPtr<SecurityOrigin> m_originB = SecurityOrigin::createFromString(ORIGIN_B);
  RefPtr<SecurityOrigin> m_originC = SecurityOrigin::createFromString(ORIGIN_C);

 private:
  const bool m_frameworkWasEnabled;

  // Contains the list of controlled features, so that we are guaranteed to
  // have at least one of each kind of default behaviour represented.
  FeaturePolicy::FeatureList m_featureList;
};

TEST_F(FeaturePolicyTest, ParseValidPolicy) {
  Vector<String> messages;
  for (const char* policyString : kValidPolicies) {
    messages.clear();
    FeaturePolicy::parseFeaturePolicy(policyString, m_originA.get(), &messages);
    EXPECT_EQ(0UL, messages.size());
  }
}

TEST_F(FeaturePolicyTest, ParseInvalidPolicy) {
  Vector<String> messages;
  for (const char* policyString : kInvalidPolicies) {
    messages.clear();
    FeaturePolicy::parseFeaturePolicy(policyString, m_originA.get(), &messages);
    EXPECT_NE(0UL, messages.size());
  }
}

TEST_F(FeaturePolicyTest, PolicyParsedCorrectly) {
  Vector<String> messages;

  // Empty policy.
  WebParsedFeaturePolicyHeader parsedHeader =
      FeaturePolicy::parseFeaturePolicy("{}", m_originA.get(), &messages);
  EXPECT_EQ(0UL, parsedHeader.size());

  // Simple policy with "self".
  parsedHeader = FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"self\"]}", m_originA.get(), &messages);
  EXPECT_EQ(1UL, parsedHeader.size());
  EXPECT_EQ("default-self", parsedHeader[0].featureName);
  EXPECT_FALSE(parsedHeader[0].matchesAllOrigins);
  EXPECT_EQ(1UL, parsedHeader[0].origins.size());
  EXPECT_TRUE(m_originA->isSameSchemeHostPortAndSuborigin(
      parsedHeader[0].origins[0].get()));

  // Simple policy with *.
  parsedHeader = FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"*\"]}", m_originA.get(), &messages);
  EXPECT_EQ(1UL, parsedHeader.size());
  EXPECT_EQ("default-self", parsedHeader[0].featureName);
  EXPECT_TRUE(parsedHeader[0].matchesAllOrigins);
  EXPECT_EQ(0UL, parsedHeader[0].origins.size());

  // Complicated policy.
  parsedHeader = FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"*\"], "
      "\"default-on\": [\"https://example.net\", \"https://example.org\"], "
      "\"default-off\": [\"self\"]}",
      m_originA.get(), &messages);
  EXPECT_EQ(3UL, parsedHeader.size());
  EXPECT_EQ("default-self", parsedHeader[0].featureName);
  EXPECT_TRUE(parsedHeader[0].matchesAllOrigins);
  EXPECT_EQ(0UL, parsedHeader[0].origins.size());
  EXPECT_EQ("default-on", parsedHeader[1].featureName);
  EXPECT_FALSE(parsedHeader[1].matchesAllOrigins);
  EXPECT_EQ(2UL, parsedHeader[1].origins.size());
  EXPECT_TRUE(m_originB->isSameSchemeHostPortAndSuborigin(
      parsedHeader[1].origins[0].get()));
  EXPECT_TRUE(m_originC->isSameSchemeHostPortAndSuborigin(
      parsedHeader[1].origins[1].get()));
  EXPECT_EQ("default-off", parsedHeader[2].featureName);
  EXPECT_FALSE(parsedHeader[2].matchesAllOrigins);
  EXPECT_EQ(1UL, parsedHeader[2].origins.size());
  EXPECT_TRUE(m_originA->isSameSchemeHostPortAndSuborigin(
      parsedHeader[2].origins[0].get()));
}

TEST_F(FeaturePolicyTest, TestInitialPolicy) {
  // +-------------+
  // |(1)Origin A  |
  // |No Policy    |
  // +-------------+
  // Default-on and top-level-only features should be enabled in top-level
  // frame. Default-off features should be disabled.
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy1->isFeatureEnabled(kDefaultOffFeature));
}

TEST_F(FeaturePolicyTest, TestInitialSameOriginChildPolicy) {
  // +-----------------+
  // |(1)Origin A      |
  // |No Policy        |
  // | +-------------+ |
  // | |(2)Origin A  | |
  // | |No Policy    | |
  // | +-------------+ |
  // +-----------------+
  // Default-on and Default-self features should be enabled in a same-origin
  // child frame. Default-off features should be disabled.
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originA);
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultOffFeature));
}

TEST_F(FeaturePolicyTest, TestInitialCrossOriginChildPolicy) {
  // +-----------------+
  // |(1)Origin A      |
  // |No Policy        |
  // | +-------------+ |
  // | |(2)Origin B  | |
  // | |No Policy    | |
  // | +-------------+ |
  // +-----------------+
  // Default-on features should be enabled in child frame. Default-self and
  // Default-off features should be disabled.
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultOffFeature));
}

TEST_F(FeaturePolicyTest, TestCrossOriginChildCannotEnableFeature) {
  // +---------------------------------------+
  // |(1) Origin A                           |
  // |No Policy                              |
  // | +-----------------------------------+ |
  // | |(2) Origin B                       | |
  // | |Policy: {"default-self": ["self"]} | |
  // | +-----------------------------------+ |
  // +---------------------------------------+
  // Default-self feature should be disabled in cross origin frame, even if no
  // policy was specified in the parent frame.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"self\"]}", m_originB.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestFrameSelfInheritance) {
  // +------------------------------------------+
  // |(1) Origin A                              |
  // |Policy: {"default-self": ["self"]}        |
  // | +-----------------+  +-----------------+ |
  // | |(2) Origin A     |  |(4) Origin B     | |
  // | |No Policy        |  |No Policy        | |
  // | | +-------------+ |  | +-------------+ | |
  // | | |(3)Origin A  | |  | |(5)Origin B  | | |
  // | | |No Policy    | |  | |No Policy    | | |
  // | | +-------------+ |  | +-------------+ | |
  // | +-----------------+  +-----------------+ |
  // +------------------------------------------+
  // Feature should be enabled at the top-level, and through the chain of
  // same-origin frames 2 and 3. It should be disabled in frames 4 and 5, as
  // they are at a different origin.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"self\"]}", m_originA.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originA);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originA);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy5 =
      createFromParentPolicy(policy4.get(), m_originB);
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy5->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestReflexiveFrameSelfInheritance) {
  // +-----------------------------------+
  // |(1) Origin A                       |
  // |Policy: {"default-self": ["self"]} |
  // | +-----------------+               |
  // | |(2) Origin B     |               |
  // | |No Policy        |               |
  // | | +-------------+ |               |
  // | | |(3)Origin A  | |               |
  // | | |No Policy    | |               |
  // | | +-------------+ |               |
  // | +-----------------+               |
  // +-----------------------------------+
  // Feature which is enabled at top-level should be disabled in frame 3, as
  // it is embedded by frame 2, for which the feature is not enabled.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"self\"]}", m_originA.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originA);
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestSelectiveFrameInheritance) {
  // +------------------------------------------+
  // |(1) Origin A                              |
  // |Policy: {"default-self": ["Origin B"]}    |
  // | +-----------------+  +-----------------+ |
  // | |(2) Origin B     |  |(3) Origin C     | |
  // | |No Policy        |  |No Policy        | |
  // | |                 |  | +-------------+ | |
  // | |                 |  | |(4)Origin B  | | |
  // | |                 |  | |No Policy    | | |
  // | |                 |  | +-------------+ | |
  // | +-----------------+  +-----------------+ |
  // +------------------------------------------+
  // Feature should be enabled in second level Origin B frame, but disabled in
  // Frame 4, because it is embedded by frame 3, where the feature is not
  // enabled.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"" ORIGIN_B "\"]}", m_originA.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy1.get(), m_originC);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy3.get(), m_originB);
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestPolicyCanBlockSelf) {
  // +----------------------------+
  // |(1)Origin A                 |
  // |Policy: {"default-on": []}  |
  // +----------------------------+
  // Default-on feature should be disabled in top-level frame.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-on\": []}", m_originA.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  EXPECT_FALSE(policy1->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestParentPolicyBlocksSameOriginChildPolicy) {
  // +----------------------------+
  // |(1)Origin A                 |
  // |Policy: {"default-on": []}  |
  // | +-------------+            |
  // | |(2)Origin A  |            |
  // | |No Policy    |            |
  // | +-------------+            |
  // +----------------------------+
  // Feature should be disabled in child frame.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-on\": []}", m_originA.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originA);
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestChildPolicyCanBlockSelf) {
  // +--------------------------------+
  // |(1)Origin A                     |
  // |No Policy                       |
  // | +----------------------------+ |
  // | |(2)Origin B                 | |
  // | |Policy: {"default-on": []}  | |
  // | +----------------------------+ |
  // +--------------------------------+
  // Default-on feature should be disabled by cross-origin child frame.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-on\": []}", m_originB.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestChildPolicyCanBlockChildren) {
  // +--------------------------------------+
  // |(1)Origin A                           |
  // |No Policy                             |
  // | +----------------------------------+ |
  // | |(2)Origin B                       | |
  // | |Policy: {"default-on": ["self"]}  | |
  // | | +-------------+                  | |
  // | | |(3)Origin C  |                  | |
  // | | |No Policy    |                  | |
  // | | +-------------+                  | |
  // | +----------------------------------+ |
  // +--------------------------------------+
  // Default-on feature should be enabled in frames 1 and 2; disabled in frame
  // 3 by child frame policy.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-on\": [\"self\"]}", m_originB.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originC);
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestParentPolicyBlocksCrossOriginChildPolicy) {
  // +----------------------------+
  // |(1)Origin A                 |
  // |Policy: {"default-on": []}  |
  // | +-------------+            |
  // | |(2)Origin B  |            |
  // | |No Policy    |            |
  // | +-------------+            |
  // +----------------------------+
  // Default-on feature should be disabled in cross-origin child frame.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-on\": []}", m_originA.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestEnableForAllOrigins) {
  // +--------------------------------+
  // |(1) Origin A                    |
  // |Policy: {"default-self": ["*"]} |
  // | +-----------------+            |
  // | |(2) Origin B     |            |
  // | |No Policy        |            |
  // | | +-------------+ |            |
  // | | |(3)Origin A  | |            |
  // | | |No Policy    | |            |
  // | | +-------------+ |            |
  // | +-----------------+            |
  // +--------------------------------+
  // Feature should be enabled in top and second level; disabled in frame 3.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"*\"]}", m_originA.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originA);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestDefaultOnEnablesForAllAncestors) {
  // +---------------------------------------+
  // |(1) Origin A                           |
  // |Policy: {"default-on": ["Origin B"]}   |
  // | +-----------------------------------+ |
  // | |(2) Origin B                       | |
  // | |No Policy                          | |
  // | | +-------------+   +-------------+ | |
  // | | |(3)Origin B  |   |(4)Origin C  | | |
  // | | |No Policy    |   |No Policy    | | |
  // | | +-------------+   +-------------+ | |
  // | +-----------------------------------+ |
  // +---------------------------------------+
  // Feature should be disabled in frame 1; enabled in frames 2, 3 and 4.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-on\": [\"" ORIGIN_B "\"]}", m_originA.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy2.get(), m_originC);
  EXPECT_FALSE(policy1->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy3->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy4->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestDefaultSelfRespectsSameOriginEmbedding) {
  // +---------------------------------------+
  // |(1) Origin A                           |
  // |Policy: {"default-self": ["Origin B"]} |
  // | +-----------------------------------+ |
  // | |(2) Origin B                       | |
  // | |No Policy                          | |
  // | | +-------------+   +-------------+ | |
  // | | |(3)Origin B  |   |(4)Origin C  | | |
  // | | |No Policy    |   |No Policy    | | |
  // | | +-------------+   +-------------+ | |
  // | +-----------------------------------+ |
  // +---------------------------------------+
  // Feature should be disabled in frames 1 and 4; enabled in frames 2 and 3.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"" ORIGIN_B "\"]}", m_originA.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy2.get(), m_originC);
  EXPECT_FALSE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestDefaultOffMustBeDelegatedToAllCrossOriginFrames) {
  // +------------------------------------------------------------+
  // |(1) Origin A                                                |
  // |Policy: {"default-off": ["Origin B"]}                       |
  // | +--------------------------------------------------------+ |
  // | |(2) Origin B                                            | |
  // | |Policy: {"default-off": ["self"]}                       | |
  // | | +-------------+   +----------------------------------+ | |
  // | | |(3)Origin B  |   |(4)Origin C                       | | |
  // | | |No Policy    |   |Policy: {"default-off": ["self"]} | | |
  // | | +-------------+   +----------------------------------+ | |
  // | +--------------------------------------------------------+ |
  // +------------------------------------------------------------+
  // Feature should be disabled in frames 1, 3 and 4; enabled in frame 2 only.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-off\": [\"" ORIGIN_B "\"]}", m_originA.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-off\": [\"self\"]}", m_originB.get(), &messages));
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy2.get(), m_originC);
  policy4->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-off\": [\"self\"]}", m_originC.get(), &messages));
  EXPECT_FALSE(policy1->isFeatureEnabled(kDefaultOffFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultOffFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultOffFeature));
  EXPECT_FALSE(policy4->isFeatureEnabled(kDefaultOffFeature));
}

TEST_F(FeaturePolicyTest, TestReenableForAllOrigins) {
  // +------------------------------------+
  // |(1) Origin A                        |
  // |Policy: {"default-self": ["*"]}     |
  // | +--------------------------------+ |
  // | |(2) Origin B                    | |
  // | |Policy: {"default-self": ["*"]} | |
  // | | +-------------+                | |
  // | | |(3)Origin A  |                | |
  // | | |No Policy    |                | |
  // | | +-------------+                | |
  // | +--------------------------------+ |
  // +------------------------------------+
  // Feature should be enabled in all frames.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"*\"]}", m_originA.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"*\"]}", m_originB.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originA);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestBlockedFrameCannotReenable) {
  // +--------------------------------------+
  // |(1)Origin A                           |
  // |Policy: {"default-self": ["self"]}    |
  // | +----------------------------------+ |
  // | |(2)Origin B                       | |
  // | |Policy: {"default-self": ["*"]}   | |
  // | | +-------------+  +-------------+ | |
  // | | |(3)Origin A  |  |(4)Origin C  | | |
  // | | |No Policy    |  |No Policy    | | |
  // | | +-------------+  +-------------+ | |
  // | +----------------------------------+ |
  // +--------------------------------------+
  // Feature should be enabled at the top level; disabled in all other frames.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"self\"]}", m_originA.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"*\"]}", m_originB.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originA);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy2.get(), m_originC);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestEnabledFrameCanDelegate) {
  // +---------------------------------------------------+
  // |(1) Origin A                                       |
  // |Policy: {"default-self": ["self", "Origin B"]}     |
  // | +-----------------------------------------------+ |
  // | |(2) Origin B                                   | |
  // | |Policy: {"default-self": ["self", "Origin C"]} | |
  // | | +-------------+                               | |
  // | | |(3)Origin C  |                               | |
  // | | |No Policy    |                               | |
  // | | +-------------+                               | |
  // | +-----------------------------------------------+ |
  // +---------------------------------------------------+
  // Feature should be enabled in all frames.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"self\", \"" ORIGIN_B "\"]}", m_originA.get(),
      &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"self\", \"" ORIGIN_C "\"]}", m_originB.get(),
      &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originC);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy3->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestEnabledFrameCanDelegateByDefault) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |Policy: {"default-on": ["self", "Origin B"]}   |
  // | +--------------------+ +--------------------+ |
  // | |(2) Origin B        | | (4) Origin C       | |
  // | |No Policy           | | No Policy          | |
  // | | +-------------+    | |                    | |
  // | | |(3)Origin C  |    | |                    | |
  // | | |No Policy    |    | |                    | |
  // | | +-------------+    | |                    | |
  // | +--------------------+ +--------------------+ |
  // +-----------------------------------------------+
  // Feature should be enabled in frames 1, 2, and 3, and disabled in frame 4.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-on\": [\"self\", \"" ORIGIN_B "\"]}", m_originA.get(),
      &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originC);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy1.get(), m_originC);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy3->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_FALSE(policy4->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestNonNestedFeaturesDontDelegateByDefault) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |Policy: {"default-self": ["self", "Origin B"]} |
  // | +--------------------+ +--------------------+ |
  // | |(2) Origin B        | | (4) Origin C       | |
  // | |No Policy           | | No Policy          | |
  // | | +-------------+    | |                    | |
  // | | |(3)Origin C  |    | |                    | |
  // | | |No Policy    |    | |                    | |
  // | | +-------------+    | |                    | |
  // | +--------------------+ +--------------------+ |
  // +-----------------------------------------------+
  // Feature should be enabled in frames 1 and 2, and disabled in frames 3 and
  // 4.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"self\", \"" ORIGIN_B "\"]}", m_originA.get(),
      &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originC);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentPolicy(policy1.get(), m_originC);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy4->isFeatureEnabled(kDefaultSelfFeature));
}

TEST_F(FeaturePolicyTest, TestFeaturesAreIndependent) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |Policy: {"default-self": ["self", "Origin B"], |
  // |         "default-on": ["self"]}               |
  // | +-------------------------------------------+ |
  // | |(2) Origin B                               | |
  // | |Policy: {"default-self": ["*"],            | |
  // | |         "default-on": ["*"]}              | |
  // | | +-------------+                           | |
  // | | |(3)Origin C  |                           | |
  // | | |No Policy    |                           | |
  // | | +-------------+                           | |
  // | +-------------------------------------------+ |
  // +-----------------------------------------------+
  // Default-self feature should be enabled in all frames; Default-on feature
  // should be enabled in frame 1, and disabled in frames 2 and 3.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"self\", \"" ORIGIN_B
      "\"], \"default-on\": [\"self\"]}",
      m_originA.get(), &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentPolicy(policy1.get(), m_originB);
  policy2->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"*\"], \"default-on\": [\"*\"]}", m_originB.get(),
      &messages));
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentPolicy(policy2.get(), m_originC);
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_TRUE(policy1->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy2->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy2->isFeatureEnabled(kDefaultOnFeature));
  EXPECT_TRUE(policy3->isFeatureEnabled(kDefaultSelfFeature));
  EXPECT_FALSE(policy3->isFeatureEnabled(kDefaultOnFeature));
}

TEST_F(FeaturePolicyTest, TestFeatureEnabledForOrigin) {
  // +-----------------------------------------------+
  // |(1) Origin A                                   |
  // |Policy: {"default-off": ["self", "Origin B"]}  |
  // +-----------------------------------------------+
  // Features should be enabled by the policy in frame 1 for origins A and B,
  // and disabled for origin C.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-off\": [\"self\", \"" ORIGIN_B "\"]}", m_originA.get(),
      &messages));
  EXPECT_EQ(0UL, messages.size());
  EXPECT_TRUE(
      policy1->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originA));
  EXPECT_TRUE(
      policy1->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originB));
  EXPECT_FALSE(
      policy1->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originC));
}

// Test frame policies

TEST_F(FeaturePolicyTest, TestSimpleFramePolicy) {
  // +-------------------------------------------------+
  // |(1)Origin A                                      |
  // |No Policy                                        |
  // |                                                 |
  // |<iframe policy='{"default-self": ["Origin B"]}'> |
  // | +-------------+                                 |
  // | |(2)Origin B  |                                 |
  // | |No Policy    |                                 |
  // | +-------------+                                 |
  // +-------------------------------------------------+
  // Default-self feature should be enabled in cross-origin child frame because
  // permission was delegated through frame policy.
  // This is the same scenario as when the iframe is declared as
  // <iframe allow="default-self">
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  WebParsedFeaturePolicyHeader framePolicy = FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"" ORIGIN_B "\"]}", m_originA.get(), &messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentWithFramePolicy(policy1.get(), &framePolicy, m_originB);
  EXPECT_TRUE(
      policy1->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originA));
  EXPECT_FALSE(
      policy1->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originB));
  EXPECT_FALSE(
      policy1->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originC));
  EXPECT_FALSE(
      policy2->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originA));
  EXPECT_TRUE(
      policy2->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originB));
  EXPECT_FALSE(
      policy2->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originC));
}

TEST_F(FeaturePolicyTest, TestAllOriginFramePolicy) {
  // +------------------------------------------+
  // |(1)Origin A                               |
  // |No Policy                                 |
  // |                                          |
  // |<iframe policy='{"default-self": ["*"]}'> |
  // | +-------------+                          |
  // | |(2)Origin B  |                          |
  // | |No Policy    |                          |
  // | +-------------+                          |
  // +------------------------------------------+
  // Default-self feature should be enabled in cross-origin child frame because
  // permission was delegated through frame policy.
  // This is the same scenario that arises when the iframe is declared as
  // <iframe allowfullscreen>
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  WebParsedFeaturePolicyHeader framePolicy = FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"*\"]}", m_originA.get(), &messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentWithFramePolicy(policy1.get(), &framePolicy, m_originB);
  EXPECT_TRUE(
      policy1->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originA));
  EXPECT_FALSE(
      policy1->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originB));
  EXPECT_FALSE(
      policy1->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originC));
  EXPECT_FALSE(
      policy2->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originA));
  EXPECT_TRUE(
      policy2->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originB));
  EXPECT_FALSE(
      policy2->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originC));
}

TEST_F(FeaturePolicyTest, TestFramePolicyCanBeFurtherDelegated) {
  // +-----------------------------------------------------+
  // |(1)Origin A                                          |
  // |No Policy                                            |
  // |                                                     |
  // |<iframe policy='{"default-self": ["Origin B"]}'>     |
  // | +-------------------------------------------------+ |
  // | |(2)Origin B                                      | |
  // | |No Policy                                        | |
  // | |                                                 | |
  // | |<iframe policy='{"default-self": ["Origin C"]}'> | |
  // | | +-------------+                                 | |
  // | | |(3)Origin C  |                                 | |
  // | | |No Policy    |                                 | |
  // | | +-------------+                                 | |
  // | |                                                 | |
  // | |<iframe> (No frame policy)                       | |
  // | | +-------------+                                 | |
  // | | |(4)Origin C  |                                 | |
  // | | |No Policy    |                                 | |
  // | | +-------------+                                 | |
  // | +-------------------------------------------------+ |
  // +-----------------------------------------------------+
  // Default-self feature should be enabled in cross-origin child frames 2 and
  // 3. Feature should be disabled in frame 4 because it was not further
  // delegated through frame policy.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  WebParsedFeaturePolicyHeader framePolicy1 = FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"" ORIGIN_B "\"]}", m_originA.get(), &messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentWithFramePolicy(policy1.get(), &framePolicy1, m_originB);
  WebParsedFeaturePolicyHeader framePolicy2 = FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"" ORIGIN_C "\"]}", m_originB.get(), &messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentWithFramePolicy(policy2.get(), &framePolicy2, m_originC);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentWithFramePolicy(policy2.get(), nullptr, m_originC);
  EXPECT_FALSE(
      policy3->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originA));
  EXPECT_FALSE(
      policy3->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originB));
  EXPECT_TRUE(
      policy3->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originC));
  EXPECT_FALSE(
      policy4->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originA));
  EXPECT_FALSE(
      policy4->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originB));
  EXPECT_FALSE(
      policy4->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originC));
}

TEST_F(FeaturePolicyTest, TestDefaultOnCanBeDisabledByFramePolicy) {
  // +-------------------------------------+
  // |(1)Origin A                          |
  // |No Policy                            |
  // |                                     |
  // |<iframe policy='{"default-on": []}'> |
  // | +-------------+                     |
  // | |(2)Origin A  |                     |
  // | |No Policy    |                     |
  // | +-------------+                     |
  // |                                     |
  // |<iframe policy='{"default-on": []}'> |
  // | +-------------+                     |
  // | |(3)Origin B  |                     |
  // | |No Policy    |                     |
  // | +-------------+                     |
  // +-------------------------------------+
  // Default-on feature should be disabled in both same-origin and cross-origin
  // child frames because permission was removed through frame policy.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  WebParsedFeaturePolicyHeader framePolicy1 = FeaturePolicy::parseFeaturePolicy(
      "{\"default-on\": []}", m_originA.get(), &messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentWithFramePolicy(policy1.get(), &framePolicy1, m_originA);
  WebParsedFeaturePolicyHeader framePolicy2 = FeaturePolicy::parseFeaturePolicy(
      "{\"default-on\": []}", m_originA.get(), &messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentWithFramePolicy(policy1.get(), &framePolicy2, m_originB);
  EXPECT_TRUE(
      policy1->isFeatureEnabledForOrigin(kDefaultOnFeature, *m_originA));
  EXPECT_TRUE(
      policy1->isFeatureEnabledForOrigin(kDefaultOnFeature, *m_originB));
  EXPECT_TRUE(
      policy1->isFeatureEnabledForOrigin(kDefaultOnFeature, *m_originC));
  EXPECT_FALSE(
      policy2->isFeatureEnabledForOrigin(kDefaultOnFeature, *m_originA));
  EXPECT_FALSE(
      policy2->isFeatureEnabledForOrigin(kDefaultOnFeature, *m_originB));
  EXPECT_FALSE(
      policy2->isFeatureEnabledForOrigin(kDefaultOnFeature, *m_originC));
  EXPECT_FALSE(
      policy3->isFeatureEnabledForOrigin(kDefaultOnFeature, *m_originA));
  EXPECT_FALSE(
      policy3->isFeatureEnabledForOrigin(kDefaultOnFeature, *m_originB));
  EXPECT_FALSE(
      policy3->isFeatureEnabledForOrigin(kDefaultOnFeature, *m_originC));
}

TEST_F(FeaturePolicyTest, TestDefaultOffMustBeEnabledByChildFrame) {
  // +------------------------------------------------+
  // |(1)Origin A                                     |
  // |Policy: {"default-off": ["self"]}               |
  // |                                                |
  // |<iframe policy='{"default-off": ["Origin A"]}'> |
  // | +-------------+                                |
  // | |(2)Origin A  |                                |
  // | |No Policy    |                                |
  // | +-------------+                                |
  // |                                                |
  // |<iframe policy='{"default-off": ["Origin B"]}'> |
  // | +-------------+                                |
  // | |(3)Origin B  |                                |
  // | |No Policy    |                                |
  // | +-------------+                                |
  // +------------------------------------------------+
  // Default-off feature should be disabled in both same-origin and cross-origin
  // child frames because they did not declare their own policy to enable it.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-off\": [\"self\"]}", m_originA.get(), &messages));
  WebParsedFeaturePolicyHeader framePolicy1 = FeaturePolicy::parseFeaturePolicy(
      "{\"default-off\": [\"" ORIGIN_A "\"]}", m_originA.get(), &messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentWithFramePolicy(policy1.get(), &framePolicy1, m_originA);
  WebParsedFeaturePolicyHeader framePolicy2 = FeaturePolicy::parseFeaturePolicy(
      "{\"default-off\": [\"" ORIGIN_B "\"]}", m_originA.get(), &messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentWithFramePolicy(policy1.get(), &framePolicy2, m_originB);
  EXPECT_TRUE(
      policy1->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originA));
  EXPECT_FALSE(
      policy1->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originB));
  EXPECT_FALSE(
      policy1->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originC));
  EXPECT_FALSE(
      policy2->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originA));
  EXPECT_FALSE(
      policy2->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originB));
  EXPECT_FALSE(
      policy2->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originC));
  EXPECT_FALSE(
      policy3->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originA));
  EXPECT_FALSE(
      policy3->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originB));
  EXPECT_FALSE(
      policy3->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originC));
}

TEST_F(FeaturePolicyTest, TestDefaultOffCanBeEnabledByChildFrame) {
  // +------------------------------------------------+
  // |(1)Origin A                                     |
  // |Policy: {"default-off": ["self"]}               |
  // |                                                |
  // |<iframe policy='{"default-off": ["Origin A"]}'> |
  // | +--------------------------------------------+ |
  // | |(2)Origin A                                 | |
  // | |Policy: {"default-off": ["self"]}           | |
  // | +--------------------------------------------+ |
  // |                                                |
  // |<iframe policy='{"default-off": ["Origin B"]}'> |
  // | +--------------------------------------------+ |
  // | |(3)Origin B                                 | |
  // | |Policy: {"default-off": ["self"]}           | |
  // | +--------------------------------------------+ |
  // +------------------------------------------------+
  // Default-off feature should be enabled in both same-origin and cross-origin
  // child frames because it is delegated through the parent's frame policy, and
  // they declare their own policy to enable it.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-off\": [\"self\"]}", m_originA.get(), &messages));
  WebParsedFeaturePolicyHeader framePolicy1 = FeaturePolicy::parseFeaturePolicy(
      "{\"default-off\": [\"" ORIGIN_A "\"]}", m_originA.get(), &messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentWithFramePolicy(policy1.get(), &framePolicy1, m_originA);
  policy2->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-off\": [\"self\"]}", m_originA.get(), &messages));
  WebParsedFeaturePolicyHeader framePolicy2 = FeaturePolicy::parseFeaturePolicy(
      "{\"default-off\": [\"" ORIGIN_B "\"]}", m_originA.get(), &messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentWithFramePolicy(policy1.get(), &framePolicy2, m_originB);
  policy3->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-off\": [\"self\"]}", m_originB.get(), &messages));
  EXPECT_TRUE(
      policy1->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originA));
  EXPECT_FALSE(
      policy1->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originB));
  EXPECT_FALSE(
      policy1->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originC));
  EXPECT_TRUE(
      policy2->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originA));
  EXPECT_FALSE(
      policy2->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originB));
  EXPECT_FALSE(
      policy2->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originC));
  EXPECT_FALSE(
      policy3->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originA));
  EXPECT_TRUE(
      policy3->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originB));
  EXPECT_FALSE(
      policy3->isFeatureEnabledForOrigin(kDefaultOffFeature, *m_originC));
}

TEST_F(FeaturePolicyTest, TestFramePolicyModifiesHeaderPolicy) {
  // +-----------------------------------------------+
  // |(1)Origin A                                    |
  // |Policy: {"default-self": ["self", "Origin B"]} |
  // |                                               |
  // |<iframe policy='{"default-self": []}'>         |
  // | +-------------------------------------------+ |
  // | |(2)Origin B                                | |
  // | |No Policy                                  | |
  // | +-------------------------------------------+ |
  // |                                               |
  // |<iframe policy='{"default-self": []}'>         |
  // | +-------------------------------------------+ |
  // | |(3)Origin B                                | |
  // | |Policy: {"default-self": ["self"]}         | |
  // | +-------------------------------------------+ |
  // +-----------------------------------------------+
  // Default-self feature should be disabled in both cross-origin child frames
  // by frame policy, even though the parent frame's header policy would
  // otherwise enable it. This is true regardless of the child frame's header
  // policy.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  policy1->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"self\", \"" ORIGIN_B "\"]}", m_originA.get(),
      &messages));
  EXPECT_EQ(0UL, messages.size());
  WebParsedFeaturePolicyHeader framePolicy1 = FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": []}", m_originA.get(), &messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentWithFramePolicy(policy1.get(), &framePolicy1, m_originB);
  WebParsedFeaturePolicyHeader framePolicy2 = FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": []}", m_originA.get(), &messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentWithFramePolicy(policy1.get(), &framePolicy2, m_originB);
  policy3->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"self\"]}", m_originB.get(), &messages));
  EXPECT_FALSE(
      policy2->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originB));
  EXPECT_FALSE(
      policy3->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originB));
}

TEST_F(FeaturePolicyTest, TestCombineFrameAndHeaderPolicies) {
  // +-------------------------------------------------+
  // |(1)Origin A                                      |
  // |No Policy                                        |
  // |                                                 |
  // |<iframe policy='{"default-self": ["Origin B"]}'> |
  // | +---------------------------------------------+ |
  // | |(2)Origin B                                  | |
  // | |Policy: {"default-self": ["*"]}              | |
  // | |                                             | |
  // | |<iframe policy='{"default-self": []}'>       | |
  // | | +-------------+                             | |
  // | | |(3)Origin C  |                             | |
  // | | |No Policy    |                             | |
  // | | +-------------+                             | |
  // | |                                             | |
  // | |<iframe> (No frame policy)                   | |
  // | | +-------------+                             | |
  // | | |(4)Origin C  |                             | |
  // | | |No Policy    |                             | |
  // | | +-------------+                             | |
  // | +---------------------------------------------+ |
  // +-------------------------------------------------+
  // Default-self feature should be enabled in cross-origin child frames 2 and
  // 4. Feature should be disabled in frame 2 by frame policy.
  Vector<String> messages;
  std::unique_ptr<FeaturePolicy> policy1 =
      createFromParentPolicy(nullptr, m_originA);
  WebParsedFeaturePolicyHeader framePolicy1 = FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"" ORIGIN_B "\"]}", m_originA.get(), &messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy2 =
      createFromParentWithFramePolicy(policy1.get(), &framePolicy1, m_originB);
  policy2->setHeaderPolicy(FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"*\"]}", m_originB.get(), &messages));
  WebParsedFeaturePolicyHeader framePolicy2 = FeaturePolicy::parseFeaturePolicy(
      "{\"default-self\": [\"\"]}", m_originC.get(), &messages);
  EXPECT_EQ(0UL, messages.size());
  std::unique_ptr<FeaturePolicy> policy3 =
      createFromParentWithFramePolicy(policy2.get(), &framePolicy2, m_originC);
  std::unique_ptr<FeaturePolicy> policy4 =
      createFromParentWithFramePolicy(policy2.get(), nullptr, m_originC);
  EXPECT_TRUE(
      policy1->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originA));
  EXPECT_TRUE(
      policy2->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originB));
  EXPECT_FALSE(
      policy3->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originC));
  EXPECT_TRUE(
      policy4->isFeatureEnabledForOrigin(kDefaultSelfFeature, *m_originC));
}

}  // namespace blink
