// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/experiments/Experiments.h"

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
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

const char* kNonExistingAPIName = "This API does not exist";
const char* kFrobulateAPIName = "Frobulate";
const char* kFrobulateEnabledOrigin = "https://www.example.com";
const char* kFrobulateEnabledOriginUnsecure = "http://www.example.com";

} // namespace

class ExperimentsTest : public ::testing::Test {
protected:
    ExperimentsTest()
        : m_page(DummyPageHolder::create())
        , m_frameworkWasEnabled(RuntimeEnabledFeatures::experimentalFrameworkEnabled())
    {
        if (!RuntimeEnabledFeatures::experimentalFrameworkEnabled()) {
            RuntimeEnabledFeatures::setExperimentalFrameworkEnabled(true);
        }
    }

    ~ExperimentsTest()
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

    void addApiKey(const char* keyValue)
    {
        HTMLElement* head = document().head();
        ASSERT_TRUE(head);

        RefPtrWillBeRawPtr<HTMLMetaElement> meta = HTMLMetaElement::create(document());
        meta->setAttribute(HTMLNames::nameAttr, "api-experiments");
        meta->setAttribute(HTMLNames::contentAttr, keyValue);
        head->appendChild(meta.release());
    }

    bool isApiEnabled(const String& origin, const String& apiName, const char* apiKeyValue, String* errorMessage)
    {
        setPageOrigin(origin);
        addApiKey(apiKeyValue);
        return Experiments::isApiEnabled(executionContext(), apiName, errorMessage);
    }

    bool isApiEnabledWithoutErrorMessage(const String& origin, const String& apiName, const char* apiKeyValue)
    {
        return isApiEnabled(origin, apiName, apiKeyValue, nullptr);
    }

private:
    OwnPtr<DummyPageHolder> m_page;
    RefPtrWillBePersistent<HTMLDocument> m_document;
    const bool m_frameworkWasEnabled;
};

TEST_F(ExperimentsTest, EnabledNonExistingAPI)
{
    String errorMessage;
    bool isNonExistingApiEnabled = isApiEnabled(kFrobulateEnabledOrigin,
        kNonExistingAPIName,
        kFrobulateAPIName /* Use existing api name as the key value */,
        &errorMessage);
    EXPECT_FALSE(isNonExistingApiEnabled);
    EXPECT_EQ(("The provided key(s) are not valid for the 'This API does not exist' API."), errorMessage);
}

TEST_F(ExperimentsTest, EnabledNonExistingAPIWithoutErrorMessage)
{
    bool isNonExistingApiEnabled = isApiEnabledWithoutErrorMessage(
        kFrobulateEnabledOrigin,
        kNonExistingAPIName,
        kFrobulateAPIName /* Use existing api name as the key value */);
    EXPECT_FALSE(isNonExistingApiEnabled);
}

// The API should be enabled if a valid key for the origin is provided
TEST_F(ExperimentsTest, EnabledSecureRegisteredOrigin)
{
    String errorMessage;
    bool isOriginEnabled = isApiEnabled(kFrobulateEnabledOrigin,
        kFrobulateAPIName,
        kFrobulateAPIName /* Use just the api name as the key value */,
        &errorMessage);
    EXPECT_TRUE(isOriginEnabled);
    EXPECT_TRUE(errorMessage.isEmpty());
}

TEST_F(ExperimentsTest, EnabledSecureRegisteredOriginWithoutErrorMessage)
{
    bool isOriginEnabled = isApiEnabledWithoutErrorMessage(
        kFrobulateEnabledOrigin,
        kFrobulateAPIName,
        kFrobulateAPIName /* Use just the api name as the key value */);
    EXPECT_TRUE(isOriginEnabled);
}

// The API should not be enabled if the origin is unsecure, even if a valid
// key for the origin is provided
TEST_F(ExperimentsTest, EnabledNonSecureRegisteredOrigin)
{
    String errorMessage;
    bool isOriginEnabled = isApiEnabled(kFrobulateEnabledOriginUnsecure,
        kFrobulateAPIName,
        kFrobulateAPIName /* Use just the api name as the key value */,
        &errorMessage);
    EXPECT_FALSE(isOriginEnabled);
    EXPECT_TRUE(errorMessage.contains("secure origin")) << "Message should indicate only secure origins are allowed, was: " << errorMessage;
}

TEST_F(ExperimentsTest, DisabledException)
{
    DOMException* disabledException = Experiments::createApiDisabledException(kNonExistingAPIName);
    ASSERT_TRUE(disabledException) << "An exception should have been created";
    EXPECT_EQ(DOMException::getErrorName(NotSupportedError), disabledException->name());
    EXPECT_TRUE(disabledException->message().contains(kNonExistingAPIName)) << "Message should contain the API name, was: " << disabledException->message();
}

} // namespace blink
