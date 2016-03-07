// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/origin_trials/DocumentOriginTrialContext.h"

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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using ::testing::UnorderedElementsAre;

namespace {

// API Key which will appear valid
const char* kGoodTrialToken = "1|AnySignatureWillDo|https://www.example.com|Frobulate|2000000000";
const char* kAnotherTrialToken = "1|AnySignatureWillDo|https://www.example.com|FrobulateV2|2000000000";

} // namespace

class DocumentOriginTrialContextTest : public ::testing::Test {
protected:
    DocumentOriginTrialContextTest()
        : m_pageHolder(DummyPageHolder::create())
        , m_frameworkWasEnabled(RuntimeEnabledFeatures::experimentalFrameworkEnabled())
    {
        RuntimeEnabledFeatures::setExperimentalFrameworkEnabled(true);
    }

    ~DocumentOriginTrialContextTest()
    {
        RuntimeEnabledFeatures::setExperimentalFrameworkEnabled(m_frameworkWasEnabled);
        m_pageHolder.clear();
    }

    void SetUp() override
    {
        setInnerHTML(
            "<html>"
            "<head>"
            "</head>"
            "<body>"
            "</body>"
            "</html>");
    }

    Document& document() { return m_pageHolder->document(); }

    void setPageOrigin(const String& origin)
    {
        KURL pageURL(ParsedURLString, origin);
        RefPtr<SecurityOrigin> pageOrigin = SecurityOrigin::create(pageURL);
        document().updateSecurityOrigin(pageOrigin);
    }

    void setInnerHTML(const char* htmlContent)
    {
        document().documentElement()->setInnerHTML(String::fromUTF8(htmlContent), ASSERT_NO_EXCEPTION);
        document().view()->updateAllLifecyclePhases();
    }

    void addTrialToken(const AtomicString& token)
    {
        HTMLElement* head = document().head();
        ASSERT_TRUE(head);

        RefPtrWillBeRawPtr<HTMLMetaElement> meta = HTMLMetaElement::create(document());
        meta->setAttribute(HTMLNames::http_equivAttr, OriginTrialContext::kTrialHeaderName);
        meta->setAttribute(HTMLNames::contentAttr, token);
        head->appendChild(meta.release());
    }

    Vector<String> getTokens()
    {
        return document().createOriginTrialContext()->getTokens();
    }

private:
    OwnPtr<DummyPageHolder> m_pageHolder;
    const bool m_frameworkWasEnabled;
};

TEST_F(DocumentOriginTrialContextTest, DetectsZeroTokens)
{
    String errorMessage;
    Vector<String> tokens = getTokens();
    EXPECT_EQ(0UL, tokens.size());
}

TEST_F(DocumentOriginTrialContextTest, ExtractsSingleToken)
{
    addTrialToken(kGoodTrialToken);
    Vector<String> tokens = getTokens();
    EXPECT_EQ(1UL, tokens.size());
    EXPECT_EQ(kGoodTrialToken, tokens[0]);
}

TEST_F(DocumentOriginTrialContextTest, ExtractsAllTokens)
{
    addTrialToken(kGoodTrialToken);
    addTrialToken(kAnotherTrialToken);
    EXPECT_THAT(getTokens(), UnorderedElementsAre(kGoodTrialToken, kAnotherTrialToken));
}

} // namespace blink
