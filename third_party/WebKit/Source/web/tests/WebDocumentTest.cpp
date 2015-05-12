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

TEST(WebDocumentTest, BeginExitTransition)
{
    std::string baseURL = "http://www.test.com:0/";
    const char* htmlURL = "transition_exit.html";
    const char* cssURL = "transition_exit.css";
    URLTestHelpers::registerMockedURLLoad(toKURL(baseURL + htmlURL), WebString::fromUTF8(htmlURL));
    URLTestHelpers::registerMockedURLLoad(toKURL(baseURL + cssURL), WebString::fromUTF8(cssURL));

    WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(baseURL + htmlURL);

    WebFrame* frame = webViewHelper.webView()->mainFrame();
    Document* coreDoc = toLocalFrame(webViewHelper.webViewImpl()->page()->mainFrame())->document();
    Element* transitionElement = coreDoc->getElementById("foo");
    ASSERT(transitionElement);

    const ComputedStyle* transitionStyle = transitionElement->computedStyle();
    ASSERT(transitionStyle);

    HTMLElement* bodyElement = coreDoc->body();
    ASSERT(bodyElement);

    const ComputedStyle* bodyStyle = bodyElement->computedStyle();
    ASSERT(bodyStyle);
    // The transition_exit.css stylesheet should not have been applied at this point.
    ASSERT_EQ(Color(0, 0, 0), bodyStyle->visitedDependentColor(CSSPropertyColor));

    frame->document().beginExitTransition("#foo", false);

    // Make sure the stylesheet load request gets processed.
    FrameTestHelpers::pumpPendingRequestsDoNotUse(frame);
    coreDoc->updateLayoutTreeIfNeeded();

    // The element should now be hidden.
    transitionStyle = transitionElement->computedStyle();
    ASSERT_TRUE(transitionStyle);
    ASSERT_EQ(transitionStyle->opacity(), 0);

    // The stylesheet should now have been applied.
    bodyStyle = bodyElement->computedStyle();
    ASSERT(bodyStyle);
    ASSERT_EQ(Color(0, 128, 0), bodyStyle->visitedDependentColor(CSSPropertyColor));
}


TEST(WebDocumentTest, BeginExitTransitionToNativeApp)
{
    std::string baseURL = "http://www.test.com:0/";
    const char* htmlURL = "transition_exit.html";
    const char* cssURL = "transition_exit.css";
    URLTestHelpers::registerMockedURLLoad(toKURL(baseURL + htmlURL), WebString::fromUTF8(htmlURL));
    URLTestHelpers::registerMockedURLLoad(toKURL(baseURL + cssURL), WebString::fromUTF8(cssURL));

    WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(baseURL + htmlURL);

    WebFrame* frame = webViewHelper.webView()->mainFrame();
    Document* coreDoc = toLocalFrame(webViewHelper.webViewImpl()->page()->mainFrame())->document();
    Element* transitionElement = coreDoc->getElementById("foo");
    ASSERT(transitionElement);

    const ComputedStyle* transitionStyle = transitionElement->computedStyle();
    ASSERT(transitionStyle);

    HTMLElement* bodyElement = coreDoc->body();
    ASSERT(bodyElement);

    const ComputedStyle* bodyStyle = bodyElement->computedStyle();
    ASSERT(bodyStyle);
    // The transition_exit.css stylesheet should not have been applied at this point.
    ASSERT_EQ(Color(0, 0, 0), bodyStyle->visitedDependentColor(CSSPropertyColor));

    frame->document().beginExitTransition("#foo", true);

    // Make sure the stylesheet load request gets processed.
    FrameTestHelpers::pumpPendingRequestsDoNotUse(frame);
    coreDoc->updateLayoutTreeIfNeeded();

    // The element should not be hidden.
    transitionStyle = transitionElement->computedStyle();
    ASSERT_TRUE(transitionStyle);
    ASSERT_EQ(transitionStyle->opacity(), 1);

    // The stylesheet should now have been applied.
    bodyStyle = bodyElement->computedStyle();
    ASSERT(bodyStyle);
    ASSERT_EQ(Color(0, 128, 0), bodyStyle->visitedDependentColor(CSSPropertyColor));
}


TEST(WebDocumentTest, HideAndShowTransitionElements)
{
    std::string baseURL = "http://www.test.com:0/";
    const char* htmlURL = "transition_hide_and_show.html";
    URLTestHelpers::registerMockedURLLoad(toKURL(baseURL + htmlURL), WebString::fromUTF8(htmlURL));

    WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(baseURL + htmlURL);

    WebFrame* frame = webViewHelper.webView()->mainFrame();
    Document* coreDoc = toLocalFrame(webViewHelper.webViewImpl()->page()->mainFrame())->document();
    Element* transitionElement = coreDoc->getElementById("foo");
    ASSERT(transitionElement);

    const ComputedStyle* transitionStyle = transitionElement->computedStyle();
    ASSERT(transitionStyle);
    EXPECT_EQ(transitionStyle->opacity(), 1);

    // Hide transition elements
    frame->document().hideTransitionElements("#foo");
    FrameTestHelpers::pumpPendingRequestsDoNotUse(frame);
    coreDoc->updateLayoutTreeIfNeeded();
    transitionStyle = transitionElement->computedStyle();
    ASSERT_TRUE(transitionStyle);
    EXPECT_EQ(transitionStyle->opacity(), 0);

    // Show transition elements
    frame->document().showTransitionElements("#foo");
    FrameTestHelpers::pumpPendingRequestsDoNotUse(frame);
    coreDoc->updateLayoutTreeIfNeeded();
    transitionStyle = transitionElement->computedStyle();
    ASSERT_TRUE(transitionStyle);
    EXPECT_EQ(transitionStyle->opacity(), 1);
}


TEST(WebDocumentTest, SetIsTransitionDocument)
{
    std::string baseURL = "http://www.test.com:0/";
    const char* htmlURL = "transition_exit.html";
    const char* cssURL = "transition_exit.css";
    URLTestHelpers::registerMockedURLLoad(toKURL(baseURL + htmlURL), WebString::fromUTF8(htmlURL));
    URLTestHelpers::registerMockedURLLoad(toKURL(baseURL + cssURL), WebString::fromUTF8(cssURL));

    WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad(baseURL + htmlURL);

    WebFrame* frame = webViewHelper.webView()->mainFrame();
    Document* coreDoc = toLocalFrame(webViewHelper.webViewImpl()->page()->mainFrame())->document();

    ASSERT_FALSE(coreDoc->isTransitionDocument());

    frame->document().setIsTransitionDocument(true);
    ASSERT_TRUE(coreDoc->isTransitionDocument());

    frame->document().setIsTransitionDocument(false);
    ASSERT_FALSE(coreDoc->isTransitionDocument());
}

namespace {
    const char* baseURLOriginA = "http://example.test:0/";
    const char* baseURLOriginB = "http://not-example.test:0/";
    const char* emptyFile = "first_party/empty.html";
    const char* nestedData = "first_party/nested-data.html";
    const char* nestedOriginA = "first_party/nested-originA.html";
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
    URLTestHelpers::registerMockedURLLoad(toOriginA(nestedOriginAInOriginA), WebString::fromUTF8(nestedOriginAInOriginA));
    URLTestHelpers::registerMockedURLLoad(toOriginA(nestedOriginAInOriginB), WebString::fromUTF8(nestedOriginAInOriginB));
    URLTestHelpers::registerMockedURLLoad(toOriginA(nestedOriginB), WebString::fromUTF8(nestedOriginB));
    URLTestHelpers::registerMockedURLLoad(toOriginA(nestedOriginBInOriginA), WebString::fromUTF8(nestedOriginBInOriginA));
    URLTestHelpers::registerMockedURLLoad(toOriginA(nestedOriginBInOriginB), WebString::fromUTF8(nestedOriginBInOriginB));
    URLTestHelpers::registerMockedURLLoad(toOriginA(nestedSrcDoc), WebString::fromUTF8(nestedSrcDoc));

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

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(false);
    ASSERT_EQ(toOriginA(emptyFile), topDocument()->firstPartyForCookies());

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(true);
    ASSERT_EQ(toOriginA(emptyFile), topDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginA)
{
    load(nestedOriginA);

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(false);
    ASSERT_EQ(toOriginA(nestedOriginA), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginA), nestedDocument()->firstPartyForCookies());

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(true);
    ASSERT_EQ(toOriginA(nestedOriginA), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginA), nestedDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginAInOriginA)
{
    load(nestedOriginAInOriginA);

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(false);
    ASSERT_EQ(toOriginA(nestedOriginAInOriginA), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginAInOriginA), nestedDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginAInOriginA), nestedNestedDocument()->firstPartyForCookies());

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(true);
    ASSERT_EQ(toOriginA(nestedOriginAInOriginA), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginAInOriginA), nestedDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginAInOriginA), nestedNestedDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginAInOriginB)
{
    load(nestedOriginAInOriginB);

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(false);
    ASSERT_EQ(toOriginA(nestedOriginAInOriginB), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginAInOriginB), nestedDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginAInOriginB), nestedNestedDocument()->firstPartyForCookies());

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(true);
    ASSERT_EQ(toOriginA(nestedOriginAInOriginB), topDocument()->firstPartyForCookies());
    ASSERT_EQ(SecurityOrigin::urlWithUniqueSecurityOrigin(), nestedDocument()->firstPartyForCookies());
    ASSERT_EQ(SecurityOrigin::urlWithUniqueSecurityOrigin(), nestedNestedDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginB)
{
    load(nestedOriginB);

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(false);
    ASSERT_EQ(toOriginA(nestedOriginB), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginB), nestedDocument()->firstPartyForCookies());

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(true);
    ASSERT_EQ(toOriginA(nestedOriginB), topDocument()->firstPartyForCookies());
    ASSERT_EQ(SecurityOrigin::urlWithUniqueSecurityOrigin(), nestedDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginBInOriginA)
{
    load(nestedOriginBInOriginA);

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(false);
    ASSERT_EQ(toOriginA(nestedOriginBInOriginA), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginBInOriginA), nestedDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginBInOriginA), nestedNestedDocument()->firstPartyForCookies());

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(true);
    ASSERT_EQ(toOriginA(nestedOriginBInOriginA), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginBInOriginA), nestedDocument()->firstPartyForCookies());
    ASSERT_EQ(SecurityOrigin::urlWithUniqueSecurityOrigin(), nestedNestedDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedOriginBInOriginB)
{
    load(nestedOriginBInOriginB);

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(false);
    ASSERT_EQ(toOriginA(nestedOriginBInOriginB), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginBInOriginB), nestedDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedOriginBInOriginB), nestedNestedDocument()->firstPartyForCookies());

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(true);
    ASSERT_EQ(toOriginA(nestedOriginBInOriginB), topDocument()->firstPartyForCookies());
    ASSERT_EQ(SecurityOrigin::urlWithUniqueSecurityOrigin(), nestedDocument()->firstPartyForCookies());
    ASSERT_EQ(SecurityOrigin::urlWithUniqueSecurityOrigin(), nestedNestedDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedSrcdoc)
{
    load(nestedSrcDoc);

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(false);
    ASSERT_EQ(toOriginA(nestedSrcDoc), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedSrcDoc), nestedDocument()->firstPartyForCookies());

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(true);
    ASSERT_EQ(toOriginA(nestedSrcDoc), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedSrcDoc), nestedDocument()->firstPartyForCookies());
}

TEST_F(WebDocumentFirstPartyTest, NestedData)
{
    load(nestedData);

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(false);
    ASSERT_EQ(toOriginA(nestedData), topDocument()->firstPartyForCookies());
    ASSERT_EQ(toOriginA(nestedData), nestedDocument()->firstPartyForCookies());

    RuntimeEnabledFeatures::setFirstPartyIncludesAncestorsEnabled(true);
    ASSERT_EQ(toOriginA(nestedData), topDocument()->firstPartyForCookies());
    ASSERT_EQ(SecurityOrigin::urlWithUniqueSecurityOrigin(), nestedDocument()->firstPartyForCookies());
}
}
