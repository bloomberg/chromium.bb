// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "public/web/WebDocument.h"

#include "core/CSSPropertyNames.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLElement.h"
#include "core/page/Page.h"
#include "core/rendering/style/RenderStyle.h"
#include "platform/graphics/Color.h"
#include "web/tests/FrameTestHelpers.h"

#include <gtest/gtest.h>

using blink::Color;
using blink::Document;
using blink::HTMLElement;
using blink::RenderStyle;
using blink::FrameTestHelpers::WebViewHelper;
using blink::WebDocument;

namespace {

TEST(WebDocumentTest, InsertStyleSheet)
{
    WebViewHelper webViewHelper;
    webViewHelper.initializeAndLoad("about:blank");

    WebDocument webDoc = webViewHelper.webView()->mainFrame()->document();
    Document* coreDoc = toLocalFrame(webViewHelper.webViewImpl()->page()->mainFrame())->document();

    webDoc.insertStyleSheet("body { color: green }");

    // Check insertStyleSheet did not cause a synchronous style recalc.
    unsigned accessCount = coreDoc->styleEngine()->resolverAccessCount();
    ASSERT_EQ(0U, accessCount);

    HTMLElement* bodyElement = coreDoc->body();
    ASSERT(bodyElement);

    RenderStyle* style = bodyElement->renderStyle();
    ASSERT(style);

    // Inserted stylesheet not yet applied.
    ASSERT_EQ(Color(0, 0, 0), style->visitedDependentColor(blink::CSSPropertyColor));

    // Apply inserted stylesheet.
    coreDoc->updateRenderTreeIfNeeded();

    style = bodyElement->renderStyle();
    ASSERT(style);

    // Inserted stylesheet applied.
    ASSERT_EQ(Color(0, 128, 0), style->visitedDependentColor(blink::CSSPropertyColor));
}

}
