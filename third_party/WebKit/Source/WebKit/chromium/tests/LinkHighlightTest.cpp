/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "LinkHighlight.h"

#include <gtest/gtest.h>
#include "FrameTestHelpers.h"
#include "URLTestHelpers.h"
#include "WebCompositorInitializer.h"
#include "WebFrame.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebInputEvent.h"
#include "WebInputEventConversion.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "core/dom/Node.h"
#include "core/page/FrameView.h"
#include "core/platform/graphics/IntRect.h"
#include <public/WebContentLayer.h>
#include <public/WebFloatPoint.h>
#include <public/WebSize.h>
#include <public/WebUnitTestSupport.h>
#include <wtf/PassOwnPtr.h>


using namespace WebKit;
using namespace WebCore;

namespace {

TEST(LinkHighlightTest, verifyWebViewImplIntegration)
{
    WebKitTests::WebCompositorInitializer compositorInitializer(0);

    const std::string baseURL("http://www.test.com/");
    const std::string fileName("test_touch_link_highlight.html");

    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(baseURL.c_str()), WebString::fromUTF8("test_touch_link_highlight.html"));
    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(FrameTestHelpers::createWebViewAndLoad(baseURL + fileName, true));
    int pageWidth = 640;
    int pageHeight = 480;
    webViewImpl->resize(WebSize(pageWidth, pageHeight));
    webViewImpl->layout();

    WebGestureEvent touchEvent;
    touchEvent.type = WebInputEvent::GestureTapDown;

    // The coordinates below are linked to absolute positions in the referenced .html file.
    touchEvent.x = 20;
    touchEvent.y = 20;

    {
        PlatformGestureEventBuilder platformEvent(webViewImpl->mainFrameImpl()->frameView(), touchEvent);
        Node* touchNode = webViewImpl->bestTapNode(platformEvent);
        ASSERT_TRUE(touchNode);
    }

    touchEvent.y = 40;
    {
        PlatformGestureEventBuilder platformEvent(webViewImpl->mainFrameImpl()->frameView(), touchEvent);
        EXPECT_FALSE(webViewImpl->bestTapNode(platformEvent));
    }

    touchEvent.y = 20;
    // Shouldn't crash.

    {
        PlatformGestureEventBuilder platformEvent(webViewImpl->mainFrameImpl()->frameView(), touchEvent);
        webViewImpl->enableTapHighlight(platformEvent);
    }

    EXPECT_TRUE(webViewImpl->linkHighlight());
    EXPECT_TRUE(webViewImpl->linkHighlight()->contentLayer());
    EXPECT_TRUE(webViewImpl->linkHighlight()->clipLayer());

    // Find a target inside a scrollable div

    touchEvent.y = 100;
    {
        PlatformGestureEventBuilder platformEvent(webViewImpl->mainFrameImpl()->frameView(), touchEvent);
        webViewImpl->enableTapHighlight(platformEvent);
    }

    ASSERT_TRUE(webViewImpl->linkHighlight());

    // Don't highlight if no "hand cursor"
    touchEvent.y = 220; // An A-link with cross-hair cursor.
    {
        PlatformGestureEventBuilder platformEvent(webViewImpl->mainFrameImpl()->frameView(), touchEvent);
        webViewImpl->enableTapHighlight(platformEvent);
    }
    ASSERT_FALSE(webViewImpl->linkHighlight());

    touchEvent.y = 260; // A text input box.
    {
        PlatformGestureEventBuilder platformEvent(webViewImpl->mainFrameImpl()->frameView(), touchEvent);
        webViewImpl->enableTapHighlight(platformEvent);
    }
    ASSERT_FALSE(webViewImpl->linkHighlight());

    webViewImpl->close();
    Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
}

class FakeWebFrameClient : public WebFrameClient {
    // To make the destructor public.
};

class FakeCompositingWebViewClient : public WebViewClient {
public:
    virtual ~FakeCompositingWebViewClient()
    {
    }

    virtual void initializeLayerTreeView() OVERRIDE
    {
        m_layerTreeView = adoptPtr(Platform::current()->unitTestSupport()->createLayerTreeViewForTesting(WebUnitTestSupport::TestViewTypeUnitTest));
        ASSERT(m_layerTreeView);
    }

    virtual WebLayerTreeView* layerTreeView() OVERRIDE
    {
        return m_layerTreeView.get();
    }

    FakeWebFrameClient m_fakeWebFrameClient;

private:
    OwnPtr<WebLayerTreeView> m_layerTreeView;
};

static WebViewClient* compositingWebViewClient()
{
    DEFINE_STATIC_LOCAL(FakeCompositingWebViewClient, client, ());
    return &client;
}

TEST(LinkHighlightTest, resetDuringNodeRemoval)
{
    WebKitTests::WebCompositorInitializer compositorInitializer(0);

    const std::string baseURL("http://www.test.com/");
    const std::string fileName("test_touch_link_highlight.html");

    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(baseURL.c_str()), WebString::fromUTF8("test_touch_link_highlight.html"));
    WebViewImpl* webViewImpl = static_cast<WebViewImpl*>(FrameTestHelpers::createWebViewAndLoad(baseURL + fileName, true, 0, compositingWebViewClient()));

    int pageWidth = 640;
    int pageHeight = 480;
    webViewImpl->resize(WebSize(pageWidth, pageHeight));
    webViewImpl->layout();

    WebGestureEvent touchEvent;
    touchEvent.type = WebInputEvent::GestureTapDown;
    touchEvent.x = 20;
    touchEvent.y = 20;

    PlatformGestureEventBuilder platformEvent(webViewImpl->mainFrameImpl()->frameView(), touchEvent);
    Node* touchNode = webViewImpl->bestTapNode(platformEvent);
    ASSERT_TRUE(touchNode);

    webViewImpl->enableTapHighlight(platformEvent);
    ASSERT_TRUE(webViewImpl->linkHighlight());

    GraphicsLayerChromium* highlightLayer = webViewImpl->linkHighlight()->currentGraphicsLayerForTesting();
    ASSERT_TRUE(highlightLayer);
    EXPECT_TRUE(highlightLayer->linkHighlight());

    touchNode->remove(IGNORE_EXCEPTION);
    webViewImpl->layout();
    EXPECT_FALSE(highlightLayer->linkHighlight());

    webViewImpl->close();
    Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
}

} // namespace
