// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/permissions_policy/dom_feature_policy.h"
#include "third_party/blink/renderer/core/permissions_policy/iframe_policy.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {
constexpr char kSelfOrigin[] = "https://selforigin.com";
constexpr char kOriginA[] = "https://example.com";
constexpr char kOriginB[] = "https://example.net";
}  // namespace

using testing::UnorderedElementsAre;

class PolicyTest : public testing::Test {
 public:
  void SetUp() override {
    page_holder_ = std::make_unique<DummyPageHolder>();

    auto origin = SecurityOrigin::CreateFromString(kSelfOrigin);

    auto permissions_policy = PermissionsPolicy::CreateFromParentPolicy(
        nullptr, ParsedPermissionsPolicy(), origin->ToUrlOrigin());
    auto header = PermissionsPolicyParser::ParseHeader(
        "fullscreen *; payment 'self'; midi 'none'; camera 'self' "
        "https://example.com https://example.net",
        /* permissions_policy_header */ g_empty_string, origin.get(),
        dummy_logger_, dummy_logger_);
    permissions_policy->SetHeaderPolicy(header);

    auto& security_context =
        page_holder_->GetFrame().DomWindow()->GetSecurityContext();
    security_context.SetSecurityOriginForTesting(origin);
    security_context.SetPermissionsPolicy(std::move(permissions_policy));
  }

  DOMFeaturePolicy* GetPolicy() const { return policy_; }

  PolicyParserMessageBuffer dummy_logger_ =
      PolicyParserMessageBuffer("", true /* discard_message */);

 protected:
  std::unique_ptr<DummyPageHolder> page_holder_;
  Persistent<DOMFeaturePolicy> policy_;
};

class DOMFeaturePolicyTest : public PolicyTest {
 public:
  void SetUp() override {
    PolicyTest::SetUp();
    policy_ = MakeGarbageCollected<DOMFeaturePolicy>(
        page_holder_->GetFrame().DomWindow());
  }
};

class IFramePolicyTest : public PolicyTest {
 public:
  void SetUp() override {
    PolicyTest::SetUp();
    policy_ = MakeGarbageCollected<IFramePolicy>(
        page_holder_->GetFrame().DomWindow(), ParsedPermissionsPolicy(),
        SecurityOrigin::CreateFromString(kSelfOrigin));
  }
};

TEST_F(DOMFeaturePolicyTest, TestAllowsFeature) {
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "badfeature"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "midi"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "midi", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "fullscreen"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "fullscreen", kOriginA));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "payment"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "payment", kOriginA));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "payment", kOriginB));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "camera"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "camera", kOriginA));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "camera", kOriginB));
  EXPECT_FALSE(
      GetPolicy()->allowsFeature(nullptr, "camera", "https://badorigin.com"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "geolocation", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "sync-xhr"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "sync-xhr", kOriginA));
}

TEST_F(DOMFeaturePolicyTest, TestGetAllowList) {
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "camera"),
              UnorderedElementsAre(kSelfOrigin, kOriginA, kOriginB));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "payment"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "geolocation"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "fullscreen"),
              UnorderedElementsAre("*"));
  EXPECT_TRUE(
      GetPolicy()->getAllowlistForFeature(nullptr, "badfeature").IsEmpty());
  EXPECT_TRUE(GetPolicy()->getAllowlistForFeature(nullptr, "midi").IsEmpty());
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "sync-xhr"),
              UnorderedElementsAre("*"));
}

TEST_F(DOMFeaturePolicyTest, TestAllowedFeatures) {
  Vector<String> allowed_features = GetPolicy()->allowedFeatures(nullptr);
  EXPECT_TRUE(allowed_features.Contains("fullscreen"));
  EXPECT_TRUE(allowed_features.Contains("payment"));
  EXPECT_TRUE(allowed_features.Contains("camera"));
  // "geolocation" has default policy as allowed on self origin.
  EXPECT_TRUE(allowed_features.Contains("geolocation"));
  EXPECT_FALSE(allowed_features.Contains("badfeature"));
  EXPECT_FALSE(allowed_features.Contains("midi"));
  // "sync-xhr" is allowed on all origins
  EXPECT_TRUE(allowed_features.Contains("sync-xhr"));
}

TEST_F(IFramePolicyTest, TestAllowsFeature) {
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "badfeature"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "midi"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "midi", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "fullscreen"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "fullscreen", kOriginA));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "fullscreen", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "payment"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "payment", kOriginA));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "payment", kOriginB));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "camera"));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "camera", kOriginA));
  EXPECT_FALSE(GetPolicy()->allowsFeature(nullptr, "camera", kOriginB));
  EXPECT_FALSE(
      GetPolicy()->allowsFeature(nullptr, "camera", "https://badorigin.com"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "geolocation", kSelfOrigin));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "sync-xhr"));
  EXPECT_TRUE(GetPolicy()->allowsFeature(nullptr, "sync-xhr", kOriginA));
}

TEST_F(IFramePolicyTest, TestGetAllowList) {
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "camera"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "payment"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "geolocation"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "fullscreen"),
              UnorderedElementsAre(kSelfOrigin));
  EXPECT_TRUE(
      GetPolicy()->getAllowlistForFeature(nullptr, "badfeature").IsEmpty());
  EXPECT_TRUE(GetPolicy()->getAllowlistForFeature(nullptr, "midi").IsEmpty());
  EXPECT_THAT(GetPolicy()->getAllowlistForFeature(nullptr, "sync-xhr"),
              UnorderedElementsAre("*"));
}

TEST_F(IFramePolicyTest, TestSameOriginAllowedFeatures) {
  Vector<String> allowed_features = GetPolicy()->allowedFeatures(nullptr);
  // These features are allowed in a same origin context, and not restricted by
  // the parent document's policy.
  EXPECT_TRUE(allowed_features.Contains("fullscreen"));
  EXPECT_TRUE(allowed_features.Contains("payment"));
  EXPECT_TRUE(allowed_features.Contains("camera"));
  EXPECT_TRUE(allowed_features.Contains("geolocation"));
  // "midi" is restricted by the parent document's policy.
  EXPECT_FALSE(allowed_features.Contains("midi"));
  // "sync-xhr" is allowed on all origins.
  EXPECT_TRUE(allowed_features.Contains("sync-xhr"));
  // This feature does not exist, so should not be advertised as allowed.
  EXPECT_FALSE(allowed_features.Contains("badfeature"));
}

TEST_F(IFramePolicyTest, TestCrossOriginAllowedFeatures) {
  // Update the iframe's policy, given a new origin.
  GetPolicy()->UpdateContainerPolicy(
      ParsedPermissionsPolicy(), SecurityOrigin::CreateFromString(kOriginA));
  Vector<String> allowed_features = GetPolicy()->allowedFeatures(nullptr);
  // None of these features should be allowed in a cross-origin context.
  EXPECT_FALSE(allowed_features.Contains("fullscreen"));
  EXPECT_FALSE(allowed_features.Contains("payment"));
  EXPECT_FALSE(allowed_features.Contains("camera"));
  EXPECT_FALSE(allowed_features.Contains("geolocation"));
  EXPECT_FALSE(allowed_features.Contains("midi"));
  // "sync-xhr" is allowed on all origins.
  EXPECT_TRUE(allowed_features.Contains("sync-xhr"));
  // This feature does not exist, so should not be advertised as allowed.
  EXPECT_FALSE(allowed_features.Contains("badfeature"));
}

TEST_F(IFramePolicyTest, TestCombinedPolicy) {
  ParsedPermissionsPolicy container_policy =
      PermissionsPolicyParser::ParseAttribute(
          "geolocation 'src'; payment 'none'; midi; camera 'src'",
          SecurityOrigin::CreateFromString(kSelfOrigin),
          SecurityOrigin::CreateFromString(kOriginA), dummy_logger_);
  GetPolicy()->UpdateContainerPolicy(
      container_policy, SecurityOrigin::CreateFromString(kOriginA));
  Vector<String> allowed_features = GetPolicy()->allowedFeatures(nullptr);
  // These features are not explicitly allowed.
  EXPECT_FALSE(allowed_features.Contains("fullscreen"));
  EXPECT_FALSE(allowed_features.Contains("payment"));
  // These features are explicitly allowed.
  EXPECT_TRUE(allowed_features.Contains("geolocation"));
  EXPECT_TRUE(allowed_features.Contains("camera"));
  // "midi" is allowed by the attribute, but still blocked by the parent
  // document's policy.
  EXPECT_FALSE(allowed_features.Contains("midi"));
  // "sync-xhr" is still implicitly allowed on all origins.
  EXPECT_TRUE(allowed_features.Contains("sync-xhr"));
  // This feature does not exist, so should not be advertised as allowed.
  EXPECT_FALSE(allowed_features.Contains("badfeature"));
}

}  // namespace blink
