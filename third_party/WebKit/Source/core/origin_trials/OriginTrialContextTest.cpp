// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/origin_trials/OriginTrialContext.h"

#include <memory>
#include "common/origin_trials/trial_token.h"
#include "core/dom/DOMException.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLMetaElement.h"
#include "core/html_names.h"
#include "core/testing/DummyPageHolder.h"
#include "core/testing/NullExecutionContext.h"
#include "platform/testing/HistogramTester.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "public/platform/WebTrialTokenValidator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

const char kNonExistingTrialName[] = "This trial does not exist";
const char kFrobulateTrialName[] = "Frobulate";
const char kFrobulateEnabledOrigin[] = "https://www.example.com";
const char kFrobulateEnabledOriginUnsecure[] = "http://www.example.com";

// Names of UMA histograms
const char kResultHistogram[] = "OriginTrials.ValidationResult";

// Trial token placeholder for mocked calls to validator
const char kTokenPlaceholder[] = "The token contents are not used";

class MockTokenValidator : public WebTrialTokenValidator {
 public:
  MockTokenValidator()
      : response_(OriginTrialTokenStatus::kNotSupported), call_count_(0) {}
  ~MockTokenValidator() override {}

  // blink::WebTrialTokenValidator implementation
  OriginTrialTokenStatus ValidateToken(const WebString& token,
                                       const WebSecurityOrigin& origin,
                                       WebString* feature_name) override {
    call_count_++;
    *feature_name = feature_;
    return response_;
  }

  // Useful methods for controlling the validator
  void SetResponse(OriginTrialTokenStatus response, const WebString& feature) {
    response_ = response;
    feature_ = feature;
  }
  int CallCount() { return call_count_; }

 private:
  OriginTrialTokenStatus response_;
  WebString feature_;
  int call_count_;

  DISALLOW_COPY_AND_ASSIGN(MockTokenValidator);
};

}  // namespace

class OriginTrialContextTest : public ::testing::Test,
                               private ScopedOriginTrialsForTest {
 protected:
  OriginTrialContextTest()
      : ScopedOriginTrialsForTest(true),
        execution_context_(new NullExecutionContext()),
        token_validator_(new MockTokenValidator),
        origin_trial_context_(new OriginTrialContext(
            *execution_context_,
            std::unique_ptr<MockTokenValidator>(token_validator_))),
        histogram_tester_(new HistogramTester()) {}

  MockTokenValidator* TokenValidator() { return token_validator_; }

  void UpdateSecurityOrigin(const String& origin) {
    KURL page_url(kParsedURLString, origin);
    RefPtr<SecurityOrigin> page_origin = SecurityOrigin::Create(page_url);
    execution_context_->SetSecurityOrigin(page_origin);
    execution_context_->SetIsSecureContext(SecurityOrigin::IsSecure(page_url));
  }

  bool IsTrialEnabled(const String& origin, const String& feature_name) {
    UpdateSecurityOrigin(origin);
    // Need at least one token to ensure the token validator is called.
    origin_trial_context_->AddToken(kTokenPlaceholder);
    return origin_trial_context_->IsTrialEnabled(feature_name);
  }

  void ExpectStatusUniqueMetric(OriginTrialTokenStatus status, int count) {
    histogram_tester_->ExpectUniqueSample(kResultHistogram,
                                          static_cast<int>(status), count);
  }

  void ExpecStatusTotalMetric(int total) {
    histogram_tester_->ExpectTotalCount(kResultHistogram, total);
  }

 private:
  Persistent<NullExecutionContext> execution_context_;
  MockTokenValidator* token_validator_;
  Persistent<OriginTrialContext> origin_trial_context_;
  std::unique_ptr<HistogramTester> histogram_tester_;
};

TEST_F(OriginTrialContextTest, EnabledNonExistingTrial) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateTrialName);
  bool is_non_existing_trial_enabled =
      IsTrialEnabled(kFrobulateEnabledOrigin, kNonExistingTrialName);
  EXPECT_FALSE(is_non_existing_trial_enabled);

  // Status metric should be updated.
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kSuccess, 1);
}

// The feature should be enabled if a valid token for the origin is provided
TEST_F(OriginTrialContextTest, EnabledSecureRegisteredOrigin) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateTrialName);
  bool is_origin_enabled =
      IsTrialEnabled(kFrobulateEnabledOrigin, kFrobulateTrialName);
  EXPECT_TRUE(is_origin_enabled);
  EXPECT_EQ(1, TokenValidator()->CallCount());

  // Status metric should be updated.
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kSuccess, 1);
}

// ... but if the browser says it's invalid for any reason, that's enough to
// reject.
TEST_F(OriginTrialContextTest, InvalidTokenResponseFromPlatform) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kMalformed,
                                kFrobulateTrialName);
  bool is_origin_enabled =
      IsTrialEnabled(kFrobulateEnabledOrigin, kFrobulateTrialName);
  EXPECT_FALSE(is_origin_enabled);
  EXPECT_EQ(1, TokenValidator()->CallCount());

  // Status metric should be updated.
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kMalformed, 1);
}

// The feature should not be enabled if the origin is insecure, even if a valid
// token for the origin is provided
TEST_F(OriginTrialContextTest, EnabledNonSecureRegisteredOrigin) {
  TokenValidator()->SetResponse(OriginTrialTokenStatus::kSuccess,
                                kFrobulateTrialName);
  bool is_origin_enabled =
      IsTrialEnabled(kFrobulateEnabledOriginUnsecure, kFrobulateTrialName);
  EXPECT_FALSE(is_origin_enabled);
  EXPECT_EQ(0, TokenValidator()->CallCount());
  ExpectStatusUniqueMetric(OriginTrialTokenStatus::kInsecure, 1);
}

TEST_F(OriginTrialContextTest, ParseHeaderValue) {
  std::unique_ptr<Vector<String>> tokens;
  ASSERT_TRUE(tokens = OriginTrialContext::ParseHeaderValue(" foo\t "));
  ASSERT_EQ(1u, tokens->size());
  EXPECT_EQ("foo", (*tokens)[0]);

  ASSERT_TRUE(tokens = OriginTrialContext::ParseHeaderValue(" \" bar \" "));
  ASSERT_EQ(1u, tokens->size());
  EXPECT_EQ(" bar ", (*tokens)[0]);

  ASSERT_TRUE(tokens = OriginTrialContext::ParseHeaderValue(" foo, bar"));
  ASSERT_EQ(2u, tokens->size());
  EXPECT_EQ("foo", (*tokens)[0]);
  EXPECT_EQ("bar", (*tokens)[1]);

  ASSERT_TRUE(tokens =
                  OriginTrialContext::ParseHeaderValue(",foo, ,bar,,'  ', ''"));
  ASSERT_EQ(3u, tokens->size());
  EXPECT_EQ("foo", (*tokens)[0]);
  EXPECT_EQ("bar", (*tokens)[1]);
  EXPECT_EQ("  ", (*tokens)[2]);

  ASSERT_TRUE(tokens =
                  OriginTrialContext::ParseHeaderValue("  \"abc\"  , 'def',g"));
  ASSERT_EQ(3u, tokens->size());
  EXPECT_EQ("abc", (*tokens)[0]);
  EXPECT_EQ("def", (*tokens)[1]);
  EXPECT_EQ("g", (*tokens)[2]);

  ASSERT_TRUE(tokens = OriginTrialContext::ParseHeaderValue(
                  " \"a\\b\\\"c'd\", 'e\\f\\'g' "));
  ASSERT_EQ(2u, tokens->size());
  EXPECT_EQ("ab\"c'd", (*tokens)[0]);
  EXPECT_EQ("ef'g", (*tokens)[1]);

  ASSERT_TRUE(tokens =
                  OriginTrialContext::ParseHeaderValue("\"ab,c\" , 'd,e'"));
  ASSERT_EQ(2u, tokens->size());
  EXPECT_EQ("ab,c", (*tokens)[0]);
  EXPECT_EQ("d,e", (*tokens)[1]);

  ASSERT_TRUE(tokens = OriginTrialContext::ParseHeaderValue("  "));
  EXPECT_EQ(0u, tokens->size());

  ASSERT_TRUE(tokens = OriginTrialContext::ParseHeaderValue(""));
  EXPECT_EQ(0u, tokens->size());

  ASSERT_TRUE(tokens = OriginTrialContext::ParseHeaderValue(" ,, \"\" "));
  EXPECT_EQ(0u, tokens->size());
}

TEST_F(OriginTrialContextTest, ParseHeaderValue_NotCommaSeparated) {
  EXPECT_FALSE(OriginTrialContext::ParseHeaderValue("foo bar"));
  EXPECT_FALSE(OriginTrialContext::ParseHeaderValue("\"foo\" 'bar'"));
  EXPECT_FALSE(OriginTrialContext::ParseHeaderValue("foo 'bar'"));
  EXPECT_FALSE(OriginTrialContext::ParseHeaderValue("\"foo\" bar"));
}

}  // namespace blink
