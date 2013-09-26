/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/rendering/RenderLayerBacking.h"

#include <gtest/gtest.h>
#include "FrameTestHelpers.h"
#include "URLTestHelpers.h"
#include "WebDocument.h"
#include "WebElement.h"
#include "WebFrameClient.h"
#include "WebFrameImpl.h"
#include "WebSettings.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include "core/dom/Document.h"
#include "core/page/FrameView.h"
#include "public/platform/Platform.h"
#include "public/platform/WebUnitTestSupport.h"

using namespace WebKit;

namespace WebCore {

namespace {

class MockWebViewClient : public WebViewClient {
public:
    virtual void initializeLayerTreeView()
    {
        m_layerTreeView = adoptPtr(Platform::current()->unitTestSupport()->createLayerTreeViewForTesting(WebUnitTestSupport::TestViewTypeUnitTest));
        ASSERT(m_layerTreeView);
    }

    virtual WebLayerTreeView* layerTreeView()
    {
        return m_layerTreeView.get();
    }

private:
    OwnPtr<WebLayerTreeView> m_layerTreeView;
};

class MockWebFrameClient : public WebFrameClient {
};

class RenderLayerBackingTest : public testing::Test {
public:
    RenderLayerBackingTest()
        : m_baseURL("http://www.test.com/")
    {
        // We cannot reuse FrameTestHelpers::createWebViewAndLoad here because the compositing
        // settings need to be set before the page is loaded.
        m_mainFrame = WebFrame::create(&m_mockWebFrameClient);
        m_webViewImpl = toWebViewImpl(WebView::create(&m_mockWebViewClient));
        m_webViewImpl->settings()->setForceCompositingMode(true);
        m_webViewImpl->settings()->setAcceleratedCompositingEnabled(true);
        m_webViewImpl->settings()->setAcceleratedCompositingForFixedPositionEnabled(true);
        m_webViewImpl->settings()->setAcceleratedCompositingForOverflowScrollEnabled(true);
        m_webViewImpl->settings()->setAcceleratedCompositingForScrollableFramesEnabled(true);
        m_webViewImpl->settings()->setCompositedScrollingForFramesEnabled(true);
        m_webViewImpl->settings()->setFixedPositionCreatesStackingContext(true);
        m_webViewImpl->setMainFrame(m_mainFrame);
        m_webViewImpl->resize(IntSize(320, 240));
    }

    virtual void TearDown()
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
        m_webViewImpl->close();
        m_mainFrame->close();
    }

    void navigateTo(const std::string& url)
    {
        FrameTestHelpers::loadFrame(m_webViewImpl->mainFrame(), url);
        Platform::current()->unitTestSupport()->serveAsynchronousMockedRequests();
    }

    void registerMockedHttpURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8(fileName.c_str()));
    }

protected:
    std::string m_baseURL;
    MockWebFrameClient m_mockWebFrameClient;
    MockWebViewClient m_mockWebViewClient;
    WebViewImpl* m_webViewImpl;
    WebFrame* m_mainFrame;
};

TEST_F(RenderLayerBackingTest, DISABLED_GraphicsLayerBackgroundColor)
{
    registerMockedHttpURLLoad("layer_background_color.html");
    navigateTo(m_baseURL + "layer_background_color.html");
    m_webViewImpl->layout();

    Document* document = m_webViewImpl->mainFrameImpl()->frame()->document();
    Element* layerElement = document->getElementById("layer");
    RenderLayerModelObject* renderer = toRenderLayerModelObject(layerElement->renderer());
    EXPECT_EQ(renderer->style()->visitedDependentColor(CSSPropertyBackgroundColor),
              renderer->layer()->backing()->graphicsLayer()->backgroundColor());

    layerElement = document->getElementById("layer-solid-color");
    renderer = toRenderLayerModelObject(layerElement->renderer());
    // RenderLayerBacking::graphicsLayer's background color is unset if SolidColorLayer is created.
    EXPECT_EQ(Color(), renderer->layer()->backing()->graphicsLayer()->backgroundColor());
}

}

} // namespace WebCore
