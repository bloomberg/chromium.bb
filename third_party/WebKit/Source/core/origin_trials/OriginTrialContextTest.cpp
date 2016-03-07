// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/origin_trials/OriginTrialContext.h"

#include "core/HTMLNames.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLMetaElement.h"
#include "core/testing/DummyPageHolder.h"
#include "core/testing/NullExecutionContext.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebTrialTokenValidator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"

namespace blink {
namespace {

const char kNonExistingFeatureName[] = "This feature does not exist";
const char kFrobulateFeatureName[] = "Frobulate";
const char kFrobulateEnabledOrigin[] = "https://www.example.com";
const char kFrobulateEnabledOriginUnsecure[] = "http://www.example.com";

// Trial token which will appear valid
const char kGoodToken[] = "AnySignatureWillDo|https://www.example.com|Frobulate|2000000000";

class MockTokenValidator : public WebTrialTokenValidator {
public:
    MockTokenValidator()
        : m_response(false)
        , m_callCount(0)
    {
    }
    ~MockTokenValidator() override {}

    // blink::WebTrialTokenValidator implementation
    bool validateToken(const blink::WebString& token, const blink::WebString& origin, const blink::WebString& featureName) override
    {
        m_callCount++;
        return m_response;
    }

    // Useful methods for controlling the validator
    void setResponse(bool response)
    {
        m_response = response;
    }
    void reset()
    {
        m_response = false;
        m_callCount = 0;
    }
    int callCount()
    {
        return m_callCount;
    }

private:
    bool m_response;
    int m_callCount;

    DISALLOW_COPY_AND_ASSIGN(MockTokenValidator);
};

// Concrete subclass of OriginTrialContext which simply maintains a vector of
// token strings to use for tests.
class TestOriginTrialContext : public OriginTrialContext {
public:
    explicit TestOriginTrialContext()
        : m_parent(adoptRefWillBeNoop(new NullExecutionContext()))
    {
    }

    ~TestOriginTrialContext() override = default;

    ExecutionContext* executionContext() override { return m_parent.get(); }

    void addToken(const String& token)
    {
        m_tokens.append(token);
    }

    void updateSecurityOrigin(const String& origin)
    {
        KURL pageURL(ParsedURLString, origin);
        RefPtr<SecurityOrigin> pageOrigin = SecurityOrigin::create(pageURL);
        m_parent->setSecurityOrigin(pageOrigin);
        m_parent->setIsSecureContext(SecurityOrigin::isSecure(pageURL));
    }

    Vector<String> getTokens() override
    {
        Vector<String> tokens;
        for (String token : m_tokens) {
            tokens.append(token);
        }
        return tokens;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_parent);
        OriginTrialContext::trace(visitor);
    }

private:
    RefPtrWillBeMember<NullExecutionContext> m_parent;
    Vector<String> m_tokens;
};

} // namespace

class OriginTrialContextTest : public ::testing::Test {
protected:
    OriginTrialContextTest()
        : m_frameworkWasEnabled(RuntimeEnabledFeatures::experimentalFrameworkEnabled())
        , m_tokenValidator(adoptPtr(new MockTokenValidator()))
        , m_originTrialContext(adoptPtrWillBeNoop(new TestOriginTrialContext))
    {
        RuntimeEnabledFeatures::setExperimentalFrameworkEnabled(true);
    }

    ~OriginTrialContextTest()
    {
        RuntimeEnabledFeatures::setExperimentalFrameworkEnabled(m_frameworkWasEnabled);
    }

    MockTokenValidator* tokenValidator() { return m_tokenValidator.get(); }

    bool isFeatureEnabled(const String& origin, const String& featureName, const String& token, String* errorMessage)
    {
        m_originTrialContext->updateSecurityOrigin(origin);
        m_originTrialContext->addToken(token);
        return m_originTrialContext->isFeatureEnabled(featureName, errorMessage, tokenValidator());
    }

    bool isFeatureEnabledWithoutErrorMessage(const String& origin, const String& featureName, const char* token)
    {
        return isFeatureEnabled(origin, featureName, token, nullptr);
    }

private:
    const bool m_frameworkWasEnabled;
    OwnPtr<MockTokenValidator> m_tokenValidator;
    OwnPtrWillBePersistent<TestOriginTrialContext> m_originTrialContext;
};

TEST_F(OriginTrialContextTest, EnabledNonExistingFeature)
{
    String errorMessage;
    bool isNonExistingFeatureEnabled = isFeatureEnabled(kFrobulateEnabledOrigin,
        kNonExistingFeatureName,
        kGoodToken,
        &errorMessage);
    EXPECT_FALSE(isNonExistingFeatureEnabled);
    EXPECT_EQ(("The provided token(s) are not valid for the 'This feature does not exist' feature."), errorMessage);
}

TEST_F(OriginTrialContextTest, EnabledNonExistingFeatureWithoutErrorMessage)
{
    bool isNonExistingFeatureEnabled = isFeatureEnabledWithoutErrorMessage(
        kFrobulateEnabledOrigin,
        kNonExistingFeatureName,
        kGoodToken);
    EXPECT_FALSE(isNonExistingFeatureEnabled);
}

// The feature should be enabled if a valid token for the origin is provided
TEST_F(OriginTrialContextTest, EnabledSecureRegisteredOrigin)
{
    String errorMessage;
    tokenValidator()->setResponse(true);
    bool isOriginEnabled = isFeatureEnabled(kFrobulateEnabledOrigin,
        kFrobulateFeatureName,
        kGoodToken,
        &errorMessage);
    EXPECT_TRUE(isOriginEnabled);
    EXPECT_TRUE(errorMessage.isEmpty()) << "Message should be empty, was: " << errorMessage;
    EXPECT_EQ(1, tokenValidator()->callCount());
}

// ... but if the browser says it's invalid for any reason, that's enough to
// reject.
TEST_F(OriginTrialContextTest, InvalidTokenResponseFromPlatform)
{
    String errorMessage;
    tokenValidator()->setResponse(false);
    bool isOriginEnabled = isFeatureEnabled(kFrobulateEnabledOrigin,
        kFrobulateFeatureName,
        kGoodToken,
        &errorMessage);
    EXPECT_FALSE(isOriginEnabled);
    EXPECT_EQ(("The provided token(s) are not valid for the 'Frobulate' feature."), errorMessage);
    EXPECT_EQ(1, tokenValidator()->callCount());
}

TEST_F(OriginTrialContextTest, OnlyOneErrorMessageGenerated)
{
    String errorMessage1;
    String errorMessage2;
    tokenValidator()->setResponse(false);
    isFeatureEnabled(kFrobulateEnabledOrigin, kFrobulateFeatureName, kGoodToken, &errorMessage1);
    isFeatureEnabled(kFrobulateEnabledOrigin, kFrobulateFeatureName, kGoodToken, &errorMessage2);
    EXPECT_FALSE(errorMessage1.isEmpty());
    EXPECT_TRUE(errorMessage2.isEmpty());
}

TEST_F(OriginTrialContextTest, ErrorMessageClearedIfStringReused)
{
    String errorMessage;
    tokenValidator()->setResponse(false);
    isFeatureEnabled(kFrobulateEnabledOrigin, kFrobulateFeatureName, kGoodToken, &errorMessage);
    EXPECT_FALSE(errorMessage.isEmpty());
    isFeatureEnabled(kFrobulateEnabledOrigin, kFrobulateFeatureName, kGoodToken, &errorMessage);
    EXPECT_TRUE(errorMessage.isEmpty());
}

TEST_F(OriginTrialContextTest, ErrorMessageGeneratedPerFeature)
{
    String errorMessage1;
    String errorMessage2;
    tokenValidator()->setResponse(false);
    isFeatureEnabled(kFrobulateEnabledOrigin, kFrobulateFeatureName, kGoodToken, &errorMessage1);
    isFeatureEnabled(kFrobulateEnabledOrigin, kNonExistingFeatureName, kGoodToken, &errorMessage2);
    EXPECT_FALSE(errorMessage1.isEmpty());
    EXPECT_FALSE(errorMessage2.isEmpty());
}

TEST_F(OriginTrialContextTest, EnabledSecureRegisteredOriginWithoutErrorMessage)
{
    tokenValidator()->setResponse(true);
    bool isOriginEnabled = isFeatureEnabledWithoutErrorMessage(
        kFrobulateEnabledOrigin,
        kFrobulateFeatureName,
        kGoodToken);
    EXPECT_TRUE(isOriginEnabled);
    EXPECT_EQ(1, tokenValidator()->callCount());
}

// The feature should not be enabled if the origin is unsecure, even if a valid
// token for the origin is provided
TEST_F(OriginTrialContextTest, EnabledNonSecureRegisteredOrigin)
{
    String errorMessage;
    bool isOriginEnabled = isFeatureEnabled(kFrobulateEnabledOriginUnsecure,
        kFrobulateFeatureName,
        kGoodToken,
        &errorMessage);
    EXPECT_FALSE(isOriginEnabled);
    EXPECT_EQ(0, tokenValidator()->callCount());
    EXPECT_FALSE(errorMessage.isEmpty());
}

TEST_F(OriginTrialContextTest, EnabledNonSecureRegisteredOriginWithoutErrorMessage)
{
    bool isOriginEnabled = isFeatureEnabledWithoutErrorMessage(
        kFrobulateEnabledOriginUnsecure,
        kFrobulateFeatureName,
        kGoodToken);
    EXPECT_FALSE(isOriginEnabled);
    EXPECT_EQ(0, tokenValidator()->callCount());
}

} // namespace blink
