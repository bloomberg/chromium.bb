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
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebTrialTokenValidator.h"
#include "testing/gtest/include/gtest/gtest.h"

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

} // namespace

class OriginTrialContextTest : public ::testing::Test {
protected:
    OriginTrialContextTest()
        : m_page(DummyPageHolder::create())
        , m_frameworkWasEnabled(RuntimeEnabledFeatures::experimentalFrameworkEnabled())
        , m_tokenValidator(adoptPtr(new MockTokenValidator()))
    {
        if (!RuntimeEnabledFeatures::experimentalFrameworkEnabled()) {
            RuntimeEnabledFeatures::setExperimentalFrameworkEnabled(true);
        }
    }

    ~OriginTrialContextTest()
    {
        if (!m_frameworkWasEnabled) {
            RuntimeEnabledFeatures::setExperimentalFrameworkEnabled(false);
        }

        m_page.clear();
    }

    void SetUp() override
    {
        m_document = toHTMLDocument(&m_page->document());
        setInnerHTML(
            "<html>"
            "<head>"
            "</head>"
            "<body>"
            "</body>"
            "</html>");
    }

    ExecutionContext* executionContext() { return &(m_page->document()); }
    MockTokenValidator* tokenValidator() { return m_tokenValidator.get(); }
    HTMLDocument& document() const { return *m_document; }

    void setPageOrigin(const String& origin)
    {
        KURL pageURL(ParsedURLString, origin);
        RefPtr<SecurityOrigin> pageOrigin = SecurityOrigin::create(pageURL);
        m_page->document().updateSecurityOrigin(pageOrigin);
    }

    void setInnerHTML(const char* htmlContent)
    {
        document().documentElement()->setInnerHTML(String::fromUTF8(htmlContent), ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
    }

    void addTrialToken(const String& token)
    {
        HTMLElement* head = document().head();
        ASSERT_TRUE(head);

        RefPtrWillBeRawPtr<HTMLMetaElement> meta = HTMLMetaElement::create(document());
        meta->setAttribute(HTMLNames::nameAttr, "origin-trials");
        AtomicString value(token);
        meta->setAttribute(HTMLNames::contentAttr, value);
        head->appendChild(meta.release());
    }

    bool isFeatureEnabled(const String& origin, const String& featureName, const String& token, String* errorMessage)
    {
        setPageOrigin(origin);
        addTrialToken(token);
        return OriginTrialContext::isFeatureEnabled(executionContext(), featureName, errorMessage, tokenValidator());
    }

    bool isFeatureEnabledWithoutErrorMessage(const String& origin, const String& featureName, const char* token)
    {
        return isFeatureEnabled(origin, featureName, token, nullptr);
    }

private:
    OwnPtr<DummyPageHolder> m_page;
    RefPtrWillBePersistent<HTMLDocument> m_document;
    const bool m_frameworkWasEnabled;
    OwnPtr<MockTokenValidator> m_tokenValidator;
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
    EXPECT_TRUE(errorMessage.contains("secure origin")) << "Message should indicate only secure origins are allowed, was: " << errorMessage;
    EXPECT_EQ(0, tokenValidator()->callCount());
}

} // namespace blink
