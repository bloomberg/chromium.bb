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

#include "core/page/scrolling/ScrollingCoordinator.h"

#include "core/page/Page.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/RenderWidget.h"
#include "core/rendering/compositing/CompositedLayerMapping.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "platform/graphics/GraphicsLayer.h"
#include "public/platform/Platform.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerPositionConstraint.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/web/WebSettings.h"
#include "public/web/WebViewClient.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "web/tests/URLTestHelpers.h"
#include <gtest/gtest.h>

using namespace blink;
using namespace blink;

namespace {

class ScrollingCoordinatorChromiumTest : public testing::Test {
public:
    ScrollingCoordinatorChromiumTest()
        : m_baseURL("http://www.test.com/")
    {
        m_helper.initialize(true, 0, &m_mockWebViewClient, &configureSettings);
        webViewImpl()->resize(IntSize(320, 240));
    }

    virtual ~ScrollingCoordinatorChromiumTest()
    {
        Platform::current()->unitTestSupport()->unregisterAllMockedURLs();
    }

    void navigateTo(const std::string& url)
    {
        FrameTestHelpers::loadFrame(webViewImpl()->mainFrame(), url);
    }

    void forceFullCompositingUpdate()
    {
        webViewImpl()->layout();
    }

    void registerMockedHttpURLLoad(const std::string& fileName)
    {
        URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8(fileName.c_str()));
    }

    WebLayer* getRootScrollLayer()
    {
        RenderLayerCompositor* compositor = frame()->contentRenderer()->compositor();
        ASSERT(compositor);
        ASSERT(compositor->scrollLayer());

        WebLayer* webScrollLayer = compositor->scrollLayer()->platformLayer();
        return webScrollLayer;
    }

    WebViewImpl* webViewImpl() const { return m_helper.webViewImpl(); }
    LocalFrame* frame() const { return m_helper.webViewImpl()->mainFrameImpl()->frame(); }

protected:
    std::string m_baseURL;
    FrameTestHelpers::TestWebViewClient m_mockWebViewClient;

private:
    static void configureSettings(WebSettings* settings)
    {
        settings->setJavaScriptEnabled(true);
        settings->setAcceleratedCompositingEnabled(true);
        settings->setPreferCompositingToLCDTextEnabled(true);
        settings->setAcceleratedCompositingForOverflowScrollEnabled(true);
    }

    FrameTestHelpers::WebViewHelper m_helper;
};

TEST_F(ScrollingCoordinatorChromiumTest, fastScrollingByDefault)
{
    navigateTo("about:blank");
    forceFullCompositingUpdate();

    // Make sure the scrolling coordinator is active.
    FrameView* frameView = frame()->view();
    Page* page = frame()->page();
    ASSERT_TRUE(page->scrollingCoordinator());
    ASSERT_TRUE(page->scrollingCoordinator()->coordinatesScrollingForFrameView(frameView));

    // Fast scrolling should be enabled by default.
    WebLayer* rootScrollLayer = getRootScrollLayer();
    ASSERT_TRUE(rootScrollLayer->scrollable());
    ASSERT_FALSE(rootScrollLayer->shouldScrollOnMainThread());
    ASSERT_FALSE(rootScrollLayer->haveWheelEventHandlers());
}

static WebLayer* webLayerFromElement(Element* element)
{
    if (!element)
        return 0;
    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isBoxModelObject())
        return 0;
    RenderLayer* layer = toRenderBoxModelObject(renderer)->layer();
    if (!layer)
        return 0;
    if (!layer->hasCompositedLayerMapping())
        return 0;
    CompositedLayerMapping* compositedLayerMapping = layer->compositedLayerMapping();
    GraphicsLayer* graphicsLayer = compositedLayerMapping->mainGraphicsLayer();
    if (!graphicsLayer)
        return 0;
    return graphicsLayer->platformLayer();
}

TEST_F(ScrollingCoordinatorChromiumTest, fastScrollingForFixedPosition)
{
    registerMockedHttpURLLoad("fixed-position.html");
    navigateTo(m_baseURL + "fixed-position.html");
    forceFullCompositingUpdate();

    // Fixed position should not fall back to main thread scrolling.
    WebLayer* rootScrollLayer = getRootScrollLayer();
    ASSERT_FALSE(rootScrollLayer->shouldScrollOnMainThread());

    Document* document = frame()->document();
    {
        Element* element = document->getElementById("div-tl");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(!constraint.isFixedToRightEdge && !constraint.isFixedToBottomEdge);
    }
    {
        Element* element = document->getElementById("div-tr");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(constraint.isFixedToRightEdge && !constraint.isFixedToBottomEdge);
    }
    {
        Element* element = document->getElementById("div-bl");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(!constraint.isFixedToRightEdge && constraint.isFixedToBottomEdge);
    }
    {
        Element* element = document->getElementById("div-br");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(constraint.isFixedToRightEdge && constraint.isFixedToBottomEdge);
    }
    {
        Element* element = document->getElementById("span-tl");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(!constraint.isFixedToRightEdge && !constraint.isFixedToBottomEdge);
    }
    {
        Element* element = document->getElementById("span-tr");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(constraint.isFixedToRightEdge && !constraint.isFixedToBottomEdge);
    }
    {
        Element* element = document->getElementById("span-bl");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(!constraint.isFixedToRightEdge && constraint.isFixedToBottomEdge);
    }
    {
        Element* element = document->getElementById("span-br");
        ASSERT_TRUE(element);
        WebLayer* layer = webLayerFromElement(element);
        ASSERT_TRUE(layer);
        WebLayerPositionConstraint constraint = layer->positionConstraint();
        ASSERT_TRUE(constraint.isFixedPosition);
        ASSERT_TRUE(constraint.isFixedToRightEdge && constraint.isFixedToBottomEdge);
    }
}

TEST_F(ScrollingCoordinatorChromiumTest, wheelEventHandler)
{
    registerMockedHttpURLLoad("wheel-event-handler.html");
    navigateTo(m_baseURL + "wheel-event-handler.html");
    forceFullCompositingUpdate();

    WebLayer* rootScrollLayer = getRootScrollLayer();
    ASSERT_TRUE(rootScrollLayer->haveWheelEventHandlers());
}

TEST_F(ScrollingCoordinatorChromiumTest, scrollEventHandler)
{
    registerMockedHttpURLLoad("scroll-event-handler.html");
    navigateTo(m_baseURL + "scroll-event-handler.html");
    forceFullCompositingUpdate();

    WebLayer* rootScrollLayer = getRootScrollLayer();
    ASSERT_TRUE(rootScrollLayer->haveScrollEventHandlers());
}

TEST_F(ScrollingCoordinatorChromiumTest, updateEventHandlersDuringTeardown)
{
    registerMockedHttpURLLoad("scroll-event-handler-window.html");
    navigateTo(m_baseURL + "scroll-event-handler-window.html");
    forceFullCompositingUpdate();

    // Simulate detaching the document from its DOM window. This should not
    // cause a crash when the WebViewImpl is closed by the test runner.
    frame()->document()->prepareForDestruction();
}

TEST_F(ScrollingCoordinatorChromiumTest, clippedBodyTest)
{
    registerMockedHttpURLLoad("clipped-body.html");
    navigateTo(m_baseURL + "clipped-body.html");
    forceFullCompositingUpdate();

    WebLayer* rootScrollLayer = getRootScrollLayer();
    ASSERT_EQ(0u, rootScrollLayer->nonFastScrollableRegion().size());
}

TEST_F(ScrollingCoordinatorChromiumTest, overflowScrolling)
{
    registerMockedHttpURLLoad("overflow-scrolling.html");
    navigateTo(m_baseURL + "overflow-scrolling.html");
    forceFullCompositingUpdate();

    // Verify the properties of the accelerated scrolling element starting from the RenderObject
    // all the way to the WebLayer.
    Element* scrollableElement = frame()->document()->getElementById("scrollable");
    ASSERT(scrollableElement);

    RenderObject* renderer = scrollableElement->renderer();
    ASSERT_TRUE(renderer->isBox());
    ASSERT_TRUE(renderer->hasLayer());

    RenderBox* box = toRenderBox(renderer);
    ASSERT_TRUE(box->usesCompositedScrolling());
    ASSERT_EQ(PaintsIntoOwnBacking, box->layer()->compositingState());

    CompositedLayerMapping* compositedLayerMapping = box->layer()->compositedLayerMapping();
    ASSERT_TRUE(compositedLayerMapping->hasScrollingLayer());
    ASSERT(compositedLayerMapping->scrollingContentsLayer());

    GraphicsLayer* graphicsLayer = compositedLayerMapping->scrollingContentsLayer();
    ASSERT_EQ(box->layer()->scrollableArea(), graphicsLayer->scrollableArea());

    WebLayer* webScrollLayer = compositedLayerMapping->scrollingContentsLayer()->platformLayer();
    ASSERT_TRUE(webScrollLayer->scrollable());
    ASSERT_TRUE(webScrollLayer->userScrollableHorizontal());
    ASSERT_TRUE(webScrollLayer->userScrollableVertical());

#if OS(ANDROID)
    // Now verify we've attached impl-side scrollbars onto the scrollbar layers
    ASSERT_TRUE(compositedLayerMapping->layerForHorizontalScrollbar());
    ASSERT_TRUE(compositedLayerMapping->layerForHorizontalScrollbar()->hasContentsLayer());
    ASSERT_TRUE(compositedLayerMapping->layerForVerticalScrollbar());
    ASSERT_TRUE(compositedLayerMapping->layerForVerticalScrollbar()->hasContentsLayer());
#endif
}

TEST_F(ScrollingCoordinatorChromiumTest, overflowHidden)
{
    registerMockedHttpURLLoad("overflow-hidden.html");
    navigateTo(m_baseURL + "overflow-hidden.html");
    forceFullCompositingUpdate();

    // Verify the properties of the accelerated scrolling element starting from the RenderObject
    // all the way to the WebLayer.
    Element* overflowElement = frame()->document()->getElementById("unscrollable-y");
    ASSERT(overflowElement);

    RenderObject* renderer = overflowElement->renderer();
    ASSERT_TRUE(renderer->isBox());
    ASSERT_TRUE(renderer->hasLayer());

    RenderBox* box = toRenderBox(renderer);
    ASSERT_TRUE(box->usesCompositedScrolling());
    ASSERT_EQ(PaintsIntoOwnBacking, box->layer()->compositingState());

    CompositedLayerMapping* compositedLayerMapping = box->layer()->compositedLayerMapping();
    ASSERT_TRUE(compositedLayerMapping->hasScrollingLayer());
    ASSERT(compositedLayerMapping->scrollingContentsLayer());

    GraphicsLayer* graphicsLayer = compositedLayerMapping->scrollingContentsLayer();
    ASSERT_EQ(box->layer()->scrollableArea(), graphicsLayer->scrollableArea());

    WebLayer* webScrollLayer = compositedLayerMapping->scrollingContentsLayer()->platformLayer();
    ASSERT_TRUE(webScrollLayer->scrollable());
    ASSERT_TRUE(webScrollLayer->userScrollableHorizontal());
    ASSERT_FALSE(webScrollLayer->userScrollableVertical());

    overflowElement = frame()->document()->getElementById("unscrollable-x");
    ASSERT(overflowElement);

    renderer = overflowElement->renderer();
    ASSERT_TRUE(renderer->isBox());
    ASSERT_TRUE(renderer->hasLayer());

    box = toRenderBox(renderer);
    ASSERT_TRUE(box->scrollableArea()->usesCompositedScrolling());
    ASSERT_EQ(PaintsIntoOwnBacking, box->layer()->compositingState());

    compositedLayerMapping = box->layer()->compositedLayerMapping();
    ASSERT_TRUE(compositedLayerMapping->hasScrollingLayer());
    ASSERT(compositedLayerMapping->scrollingContentsLayer());

    graphicsLayer = compositedLayerMapping->scrollingContentsLayer();
    ASSERT_EQ(box->layer()->scrollableArea(), graphicsLayer->scrollableArea());

    webScrollLayer = compositedLayerMapping->scrollingContentsLayer()->platformLayer();
    ASSERT_TRUE(webScrollLayer->scrollable());
    ASSERT_FALSE(webScrollLayer->userScrollableHorizontal());
    ASSERT_TRUE(webScrollLayer->userScrollableVertical());
}

TEST_F(ScrollingCoordinatorChromiumTest, iframeScrolling)
{
    registerMockedHttpURLLoad("iframe-scrolling.html");
    registerMockedHttpURLLoad("iframe-scrolling-inner.html");
    navigateTo(m_baseURL + "iframe-scrolling.html");
    forceFullCompositingUpdate();

    // Verify the properties of the accelerated scrolling element starting from the RenderObject
    // all the way to the WebLayer.
    Element* scrollableFrame = frame()->document()->getElementById("scrollable");
    ASSERT_TRUE(scrollableFrame);

    RenderObject* renderer = scrollableFrame->renderer();
    ASSERT_TRUE(renderer);
    ASSERT_TRUE(renderer->isWidget());

    RenderWidget* renderWidget = toRenderWidget(renderer);
    ASSERT_TRUE(renderWidget);
    ASSERT_TRUE(renderWidget->widget());
    ASSERT_TRUE(renderWidget->widget()->isFrameView());

    FrameView* innerFrameView = toFrameView(renderWidget->widget());
    RenderView* innerRenderView = innerFrameView->renderView();
    ASSERT_TRUE(innerRenderView);

    RenderLayerCompositor* innerCompositor = innerRenderView->compositor();
    ASSERT_TRUE(innerCompositor->inCompositingMode());
    ASSERT_TRUE(innerCompositor->scrollLayer());

    GraphicsLayer* scrollLayer = innerCompositor->scrollLayer();
    ASSERT_EQ(innerFrameView, scrollLayer->scrollableArea());

    WebLayer* webScrollLayer = scrollLayer->platformLayer();
    ASSERT_TRUE(webScrollLayer->scrollable());

#if OS(ANDROID)
    // Now verify we've attached impl-side scrollbars onto the scrollbar layers
    ASSERT_TRUE(innerCompositor->layerForHorizontalScrollbar());
    ASSERT_TRUE(innerCompositor->layerForHorizontalScrollbar()->hasContentsLayer());
    ASSERT_TRUE(innerCompositor->layerForVerticalScrollbar());
    ASSERT_TRUE(innerCompositor->layerForVerticalScrollbar()->hasContentsLayer());
#endif
}

TEST_F(ScrollingCoordinatorChromiumTest, rtlIframe)
{
    registerMockedHttpURLLoad("rtl-iframe.html");
    registerMockedHttpURLLoad("rtl-iframe-inner.html");
    navigateTo(m_baseURL + "rtl-iframe.html");
    forceFullCompositingUpdate();

    // Verify the properties of the accelerated scrolling element starting from the RenderObject
    // all the way to the WebLayer.
    Element* scrollableFrame = frame()->document()->getElementById("scrollable");
    ASSERT_TRUE(scrollableFrame);

    RenderObject* renderer = scrollableFrame->renderer();
    ASSERT_TRUE(renderer);
    ASSERT_TRUE(renderer->isWidget());

    RenderWidget* renderWidget = toRenderWidget(renderer);
    ASSERT_TRUE(renderWidget);
    ASSERT_TRUE(renderWidget->widget());
    ASSERT_TRUE(renderWidget->widget()->isFrameView());

    FrameView* innerFrameView = toFrameView(renderWidget->widget());
    RenderView* innerRenderView = innerFrameView->renderView();
    ASSERT_TRUE(innerRenderView);

    RenderLayerCompositor* innerCompositor = innerRenderView->compositor();
    ASSERT_TRUE(innerCompositor->inCompositingMode());
    ASSERT_TRUE(innerCompositor->scrollLayer());

    GraphicsLayer* scrollLayer = innerCompositor->scrollLayer();
    ASSERT_EQ(innerFrameView, scrollLayer->scrollableArea());

    WebLayer* webScrollLayer = scrollLayer->platformLayer();
    ASSERT_TRUE(webScrollLayer->scrollable());

    int expectedScrollPosition = 958 + (innerFrameView->verticalScrollbar()->isOverlayScrollbar() ? 0 : 15);
    ASSERT_EQ(expectedScrollPosition, webScrollLayer->scrollPosition().x);
}

TEST_F(ScrollingCoordinatorChromiumTest, setupScrollbarLayerShouldNotCrash)
{
    registerMockedHttpURLLoad("setup_scrollbar_layer_crash.html");
    navigateTo(m_baseURL + "setup_scrollbar_layer_crash.html");
    forceFullCompositingUpdate();
    // This test document setup an iframe with scrollbars, then switch to
    // an empty document by javascript.
}

} // namespace
