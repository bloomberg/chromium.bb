// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "public/web/WebDocument.h"

#include "core/CSSPropertyNames.h"
#include "core/dom/NodeComputedStyle.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLElement.h"
#include "core/style/ComputedStyle.h"
#include "core/page/Page.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/Color.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "web/tests/FrameTestHelpers.h"
#include <gtest/gtest.h>

namespace {

using blink::FrameTestHelpers::WebViewHelper;
using blink::URLTestHelpers::toKURL;
using namespace blink;

TEST(WebDocumentTest, InsertStyleSheet)
{
    WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad("about:blank");

    WebDocument webDoc = webViewHelper.webView()->mainFrame()->document();
    Document* coreDoc = toLocalFrame(webViewHelper.webViewImpl()->page()->mainFrame())->document();

    webDoc.insertStyleSheet("body { color: green }");

    // Check insertStyleSheet did not cause a synchronous style recalc.
    unsigned accessCount = coreDoc->styleEngine().resolverAccessCount();
    ASSERT_EQ(0U, accessCount);

    HTMLElement* bodyElement = coreDoc->body();
    ASSERT(bodyElement);

    const ComputedStyle& styleBeforeInsertion = bodyElement->computedStyleRef();

    // Inserted stylesheet not yet applied.
    ASSERT_EQ(Color(0, 0, 0), styleBeforeInsertion.visitedDependentColor(CSSPropertyColor));

    // Apply inserted stylesheet.
    coreDoc->updateLayoutTreeIfNeeded();

    const ComputedStyle& styleAfterInsertion = bodyElement->computedStyleRef();

    // Inserted stylesheet applied.
    ASSERT_EQ(Color(0, 128, 0), styleAfterInsertion.visitedDependentColor(CSSPropertyColor));
}


namespace {
    const char* baseURLOriginA = "http://example.test:0/";
    const char* baseURLOriginSubA = "http://subdomain.example.test:0/";
    const char* baseURLOriginB = "http://not-example.test:0/";
    const char* emptyFile = "first_party/empty.html";
    const char* nestedData = "first_party/nested-data.html";
    const char* nestedOriginA = "first_party/nested-originA.html";
    const char* nestedOriginSubA = "first_party/nested-originSubA.html";
    const char* nestedOriginAInOriginA = "first_party/nested-originA-in-originA.html";
    const char* nestedOriginAInOriginB = "first_party/nested-originA-in-originB.html";
    const char* nestedOriginB = "first_party/nested-originB.html";
    const char* nestedOriginBInOriginA = "first_party/nested-originB-in-originA.html";
    const char* nestedOriginBInOriginB = "first_party/nested-originB-in-originB.html";
    const char* nestedSrcDoc = "first_party/nested-srcdoc.html";

    static KURL toOriginA(const char* file)
    {
        return toKURL(std::string(baseURLOriginA) + file);
    }

    static KURL toOriginSubA(const char* file)
    {
        return toKURL(std::string(baseURLOriginSubA) + file);
    }

    static KURL toOriginB(const char* file)
    {
        return toKURL(std::string(baseURLOriginB) + file);
    }
}

class WebDocumentFirstPartyTest : public ::testing::Test {
public:
    static void SetUpTestCase();

protected:
    void load(const char*);
    Document* topDocument() const;
    Document* nestedDocument() const;
    Document* nestedNestedDocument() const;

    WebViewHelper m_webViewHelper;
};

void WebDocumentFirstPartyTest::SetUpTestCase()
{
    URLTestHelpers::registerMockedURLLoad(toOriginA(emptyFile), WebString::fromUTF8(emptyFile));
    URLTestHelpers::registerMockedURLLoad(toOriginA(nestedData), WebString::fromUTF8(nestedData));
    URLTestHelpers::registerMockedURLLoad(toOriginA(nestedOriginA), WebString::fromUTF8(nestedOriginA));
    URLTestHelpers::registerMockedURLLoad(toOriginA(nestedOriginSubA), WebString::fromUTF8(nestedOriginSubA));
    URLTestHelpers::registerMockedURLLoad(toOriginA(nestedOriginAInOriginA), WebString::fromUTF8(nestedOriginAInOriginA));
    URLTestHelpers::registerMockedURLLoad(toOriginA(nestedOriginAInOriginB), WebString::fromUTF8(nestedOriginAInOriginB));
    URLTestHelpers::registerMockedURLLoad(toOriginA(nestedOriginB), WebString::fromUTF8(nestedOriginB));
    URLTestHelpers::registerMockedURLLoad(toOriginA(nestedOriginBInOriginA), WebString::fromUTF8(nestedOriginBInOriginA));
    URLTestHelpers::registerMockedURLLoad(toOriginA(nestedOriginBInOriginB), WebString::fromUTF8(nestedOriginBInOriginB));
    URLTestHelpers::registerMockedURLLoad(toOriginA(nestedSrcDoc), WebString::fromUTF8(nestedSrcDoc));

    URLTestHelpers::registerMockedURLLoad(toOriginSubA(emptyFile), WebString::fromUTF8(emptyFile));

    URLTestHelpers::registerMockedURLLoad(toOriginB(emptyFile), WebString::fromUTF8(emptyFile));
    URLTestHelpers::registerMockedURLLoad(toOriginB(nestedOriginA), WebString::fromUTF8(nestedOriginA));
    URLTestHelpers::registerMockedURLLoad(toOriginB(nestedOriginB), WebString::fromUTF8(nestedOriginB));
}

void WebDocumentFirstPartyTest::load(const char* file)
{
    m_webViewHelper.initializeAndLoad(std::string(baseURLOriginA) + file);
}

Document* WebDocumentFirstPartyTest::topDocument() const
{
    return toLocalFrame(m_webViewHelper.webViewImpl()->page()->mainFrame())->document();
}

Document* WebDocumentFirstPartyTest::nestedDocument() const
{
    return toLocalFrame(m_webViewHelper.webViewImpl()->page()->mainFrame()->tree().firstChild())->document();
}

Document* WebDocumentFirstPartyTest::nestedNestedDocument() const
{
    return toLocalFrame(m_webViewHelper.webViewImpl()->page()->mainFrame()->tree().firstChild()->tree().firstChild())->document();
}

TEST_F(WebDocumentFirstPartyTest, Empty)
{
    load(emptyFile);

    ASSERT_EQ(toOriginA(emptyFile), topDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginA)
{
    load(nestedOriginA);

    ASSERT_EQ(toOriginA(nestedOriginA), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginA), nestedDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginSubA)
{
    load(nestedOriginSubA);

    ASSERT_EQ(toOriginA(nestedOriginSubA), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginSubA), nestedDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginAInOriginA)
{
    load(nestedOriginAInOriginA);

    ASSERT_EQ(toOriginA(nestedOriginAInOriginA), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginAInOriginA), nestedDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginAInOriginA), nestedNestedDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginAInOriginB)
{
    load(nestedOriginAInOriginB);

    ASSERT_EQ(toOriginA(nestedOriginAInOriginB), topDocument()->firstPartyForCookies());
    ASSERT_EQ(SecurityOrigin::urlWithUniqueSecurityOrigin(), nestedDocument()->firstPartyForCookies());
    ASSERT_EQ(SecurityOrigin::urlWithUniqueSecurityOrigin(), nestedNestedDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginB)
{
    load(nestedOriginB);

    ASSERT_EQ(toOriginA(nestedOriginB), topDocument()->firstPartyForCookies());
    ASSERT_EQ(SecurityOrigin::urlWithUniqueSecurityOrigin(), nestedDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginBInOriginA)
{
    load(nestedOriginBInOriginA);

    ASSERT_EQ(toOriginA(nestedOriginBInOriginA), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginBInOriginA), nestedDocument()->firstPartyForCookies());
    ASSERT_EQ(SecurityOrigin::urlWithUniqueSecurityOrigin(), nestedNestedDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginBInOriginB)
{
    load(nestedOriginBInOriginB);

    ASSERT_EQ(toOriginA(nestedOriginBInOriginB), topDocument()->firstPartyForCookies());
    ASSERT_EQ(SecurityOrigin::urlWithUniqueSecurityOrigin(), nestedDocument()->firstPartyForCookies());
    ASSERT_EQ(SecurityOrigin::urlWithUniqueSecurityOrigin(), nestedNestedDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedSrcdoc)
{
    load(nestedSrcDoc);

    ASSERT_EQ(toOriginA(nestedSrcDoc), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedSrcDoc), nestedDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedData)
{
    load(nestedData);

    ASSERT_EQ(toOriginA(nestedData), topDocument()->firstPartyForCookies());
    ASSERT_EQ(SecurityOrigin::urlWithUniqueSecurityOrigin(), nestedDocument()->firstPartyForCookies());
}

}
