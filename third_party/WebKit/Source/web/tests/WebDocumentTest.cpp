// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "public/web/WebDocument.h"

#include "core/CSSPropertyNames.h"
#include "core/dom/NodeLayoutStyle.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLElement.h"
#include "core/layout/style/LayoutStyle.h"
#include "core/page/Page.h"
#include "platform/graphics/Color.h"
#include "platform/testing/URLTestHelpers.h"
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

    LayoutStyle* style = bodyElement->layoutStyle();
    ASSERT(style);

    // Inserted stylesheet not yet applied.
    ASSERT_EQ(Color(0, 0, 0), style->visitedDependentColor(CSSPropertyColor));

    // Apply inserted stylesheet.
    coreDoc->updateRenderTreeIfNeeded();

    style = bodyElement->layoutStyle();
    ASSERT(style);

    // Inserted stylesheet applied.
    ASSERT_EQ(Color(0, 128, 0), style->visitedDependentColor(CSSPropertyColor));
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

    LayoutStyle* transitionStyle = transitionElement->layoutStyle();
    ASSERT(transitionStyle);

    HTMLElement* bodyElement = coreDoc->body();
    ASSERT(bodyElement);

    LayoutStyle* bodyStyle = bodyElement->layoutStyle();
    ASSERT(bodyStyle);
    // The transition_exit.css stylesheet should not have been applied at this point.
    ASSERT_EQ(Color(0, 0, 0), bodyStyle->visitedDependentColor(CSSPropertyColor));

    frame->document().beginExitTransition("#foo", false);

    // Make sure the stylesheet load request gets processed.
    FrameTestHelpers::pumpPendingRequestsDoNotUse(frame);
    coreDoc->updateRenderTreeIfNeeded();

    // The element should now be hidden.
    transitionStyle = transitionElement->layoutStyle();
    ASSERT_TRUE(transitionStyle);
    ASSERT_EQ(transitionStyle->opacity(), 0);

    // The stylesheet should now have been applied.
    bodyStyle = bodyElement->layoutStyle();
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

    LayoutStyle* transitionStyle = transitionElement->layoutStyle();
    ASSERT(transitionStyle);

    HTMLElement* bodyElement = coreDoc->body();
    ASSERT(bodyElement);

    LayoutStyle* bodyStyle = bodyElement->layoutStyle();
    ASSERT(bodyStyle);
    // The transition_exit.css stylesheet should not have been applied at this point.
    ASSERT_EQ(Color(0, 0, 0), bodyStyle->visitedDependentColor(CSSPropertyColor));

    frame->document().beginExitTransition("#foo", true);

    // Make sure the stylesheet load request gets processed.
    FrameTestHelpers::pumpPendingRequestsDoNotUse(frame);
    coreDoc->updateRenderTreeIfNeeded();

    // The element should not be hidden.
    transitionStyle = transitionElement->layoutStyle();
    ASSERT_TRUE(transitionStyle);
    ASSERT_EQ(transitionStyle->opacity(), 1);

    // The stylesheet should now have been applied.
    bodyStyle = bodyElement->layoutStyle();
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

    LayoutStyle* transitionStyle = transitionElement->layoutStyle();
    ASSERT(transitionStyle);
    EXPECT_EQ(transitionStyle->opacity(), 1);

    // Hide transition elements
    frame->document().hideTransitionElements("#foo");
    FrameTestHelpers::pumpPendingRequestsDoNotUse(frame);
    coreDoc->updateRenderTreeIfNeeded();
    transitionStyle = transitionElement->layoutStyle();
    ASSERT_TRUE(transitionStyle);
    EXPECT_EQ(transitionStyle->opacity(), 0);

    // Show transition elements
    frame->document().showTransitionElements("#foo");
    FrameTestHelpers::pumpPendingRequestsDoNotUse(frame);
    coreDoc->updateRenderTreeIfNeeded();
    transitionStyle = transitionElement->layoutStyle();
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

}
