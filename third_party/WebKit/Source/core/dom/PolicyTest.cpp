// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Policy.h"

#include "core/dom/Document.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {
constexpr char kSelfOrigin[] = "https://selforigin.com";
constexpr char kOriginA[] = "https://example.com";
constexpr char kOriginB[] = "https://example.net";
}  // namespace

using ::testing::UnorderedElementsAre;

class PolicyTest : public ::testing::Test {
 public:
  void SetUp() override {
    document_ = Document::CreateForTest();
    document_->SetSecurityOrigin(SecurityOrigin::CreateFromString(kSelfOrigin));
    document_->ApplyFeaturePolicyFromHeader(
        "fullscreen *; payment 'self'; midi 'none'; camera 'self' "
        "https://example.com https://example.net");
    policy_ = Policy::Create(document_);
  }

  Policy* GetPolicy() const { return policy_; }

 private:
  Persistent<Document> document_;
  Persistent<Policy> policy_;
};

TEST_F(PolicyTest, TestAllowsFeature) {
  EXPECT_FALSE(GetPolicy()->allowsFeature("badfeature"));
  EXPECT_FALSE(GetPolicy()->allowsFeature("midi"));
  EXPECT_FALSE(GetPolicy()->allowsFeature("midi", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature("fullscreen"));
  EXPECT_TRUE(GetPolicy()->allowsFeature("fullscreen", kOriginA));
  EXPECT_TRUE(GetPolicy()->allowsFeature("payment"));
  EXPECT_FALSE(GetPolicy()->allowsFeature("payment", kOriginA));
  EXPECT_FALSE(GetPolicy()->allowsFeature("payment", kOriginB));
  EXPECT_TRUE(GetPolicy()->allowsFeature("camera"));
  EXPECT_TRUE(GetPolicy()->allowsFeature("camera", kOriginA));
  EXPECT_TRUE(GetPolicy()->allowsFeature("camera", kOriginB));
  EXPECT_FALSE(GetPolicy()->allowsFeature("camera", "https://badorigin.com"));
  EXPECT_TRUE(GetPolicy()->allowsFeature("geolocation", kSelfOrigin));
}

TEST_F(PolicyTest, TestGetAllowList) {
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature("camera"),
              UnorderedElementsAre(kSelfOrigin, kOriginA, kOriginB));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature("payment"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature("geolocation"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature("fullscreen"),
              UnorderedElementsAre("*"));
  EXPECT_TRUE(GetPolicy()->getAllowlistForFeature("badfeature").IsEmpty());
  EXPECT_TRUE(GetPolicy()->getAllowlistForFeature("midi").IsEmpty());
}

TEST_F(PolicyTest, TestAllowedFeatures) {
  Vector<String> allowed_features = GetPolicy()->allowedFeatures();
  EXPECT_TRUE(allowed_features.Contains("fullscreen"));
  EXPECT_TRUE(allowed_features.Contains("payment"));
  EXPECT_TRUE(allowed_features.Contains("camera"));
  // "geolocation" has default policy as allowed on self origin.
  EXPECT_TRUE(allowed_features.Contains("geolocation"));
  EXPECT_FALSE(allowed_features.Contains("badfeature"));
  EXPECT_FALSE(allowed_features.Contains("midi"));
}

}  // namespace blink
