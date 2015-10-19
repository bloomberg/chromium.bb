// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/web/WebElement.h"
#include "public/web/WebHitTestResult.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "web/tests/sim/SimCompositor.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimLayerTreeView.h"
#include "web/tests/sim/SimNetwork.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimWebViewClient.h"
#include <gtest/gtest.h>

namespace blink {

class FrameThrottlingTest : public ::testing::Test {
protected:
    FrameThrottlingTest()
        : m_webViewClient(m_layerTreeView)
        , m_compositor(m_layerTreeView)
    {
        m_webViewHelper.initialize(true, &m_webFrameClient, &m_webViewClient);
        m_compositor.setWebViewImpl(webView());
        m_layerTreeView.setDeferCommits(false);
        webView().resize(WebSize(640, 480));
    }

    ~FrameThrottlingTest() override { }

    void loadURL(const String& url)
    {
        WebURLRequest request;
        request.initialize();
        request.setURL(KURL(ParsedURLString, url));
        webView().mainFrameImpl()->loadRequest(request);
    }

    void loadHTML(const String& html)
    {
        SimRequest mainResource("https://example.com/test.html", "text/html");
        loadURL("https://example.com/test.html");
        mainResource.start();
        mainResource.write(html);
        mainResource.finish();
        m_webFrameClient.waitForLoadToComplete();
        webView().layout();
    }

    WebElement setupPageWithThrottleableFrame()
    {
        loadHTML("<iframe sandbox id=frame></iframe>");
        WebExceptionCode ec;
        WebElement frameElement = root().querySelector("#frame", ec);
        EXPECT_EQ(0, ec);
        return frameElement;
    }

    Document& document()
    {
        return *webView().mainFrameImpl()->frame()->document();
    }

    WebNode root()
    {
        return WebNode(document().documentElement());
    }

    WebViewImpl& webView()
    {
        return *m_webViewHelper.webViewImpl();
    }

    SimDisplayItemList compositeFrame()
    {
        SimDisplayItemList displayItems = m_compositor.beginFrame();
        // Ensure intersection observer notifications get delivered.
        testing::runPendingTasks();
        return displayItems;
    }

    SimNetwork m_network;
    SimLayerTreeView m_layerTreeView;
    SimWebViewClient m_webViewClient;
    SimCompositor m_compositor;
    FrameTestHelpers::TestWebFrameClient m_webFrameClient;
    FrameTestHelpers::WebViewHelper m_webViewHelper;
};

TEST_F(FrameThrottlingTest, throttleInvisibleFrames)
{
    WebElement frameElement = setupPageWithThrottleableFrame();

    // Initially both frames are visible.
    FrameView* frameView = document().view();
    FrameView* childFrameView = toLocalFrame(frameView->frame().tree().firstChild())->view();
    EXPECT_FALSE(frameView->isHiddenForThrottling());
    EXPECT_FALSE(childFrameView->isHiddenForThrottling());

    // Moving the child fully outside the parent makes it invisible.
    frameElement.setAttribute("style", "transform: translateY(480px)");
    compositeFrame();
    EXPECT_FALSE(frameView->isHiddenForThrottling());
    EXPECT_TRUE(childFrameView->isHiddenForThrottling());

    // A partially visible child is considered visible.
    frameElement.setAttribute("style", "transform: translate(-50px, 0px, 0px)");
    compositeFrame();
    EXPECT_FALSE(frameView->isHiddenForThrottling());
    EXPECT_FALSE(childFrameView->isHiddenForThrottling());
}

TEST_F(FrameThrottlingTest, viewportVisibilityFullyClipped)
{
    setupPageWithThrottleableFrame();

    // A child which is fully clipped away by its ancestor should become invisible.
    FrameView* frameView = document().view();
    FrameView* childFrameView = toLocalFrame(frameView->frame().tree().firstChild())->view();
    webView().resize(WebSize(0, 0));
    compositeFrame();
    EXPECT_TRUE(frameView->isHiddenForThrottling());
    EXPECT_TRUE(childFrameView->isHiddenForThrottling());
}

TEST_F(FrameThrottlingTest, hiddenSameOriginFramesAreNotThrottled)
{
    // Create a document with doubly nested iframes.
    loadHTML("<iframe id=frame srcdoc='<iframe></iframe>'></iframe>");
    WebExceptionCode ec;
    WebElement frameElement = root().querySelector("#frame", ec);
    EXPECT_EQ(0, ec);

    FrameView* frameView = document().view();
    LocalFrame* childFrame = toLocalFrame(frameView->frame().tree().firstChild());
    LocalFrame* grandChildFrame = toLocalFrame(childFrame->tree().firstChild());
    FrameView* childFrameView = childFrame->view();
    FrameView* grandChildFrameView = grandChildFrame->view();
    EXPECT_FALSE(frameView->canThrottleRendering());
    EXPECT_FALSE(childFrameView->canThrottleRendering());
    EXPECT_FALSE(grandChildFrameView->canThrottleRendering());

    // Hidden same origin frames are not throttled.
    frameElement.setAttribute("style", "transform: translateY(480px)");
    compositeFrame();
    EXPECT_FALSE(frameView->canThrottleRendering());
    EXPECT_FALSE(childFrameView->canThrottleRendering());
    EXPECT_FALSE(grandChildFrameView->canThrottleRendering());
}

TEST_F(FrameThrottlingTest, hiddenCrossOriginFramesAreThrottled)
{
    // Create a document with doubly nested iframes.
    loadHTML("<iframe id=frame srcdoc='<iframe sandbox></iframe>'></iframe>");
    WebExceptionCode ec;
    WebElement frameElement = root().querySelector("#frame", ec);
    EXPECT_EQ(0, ec);

    FrameView* frameView = document().view();
    LocalFrame* childFrame = toLocalFrame(frameView->frame().tree().firstChild());
    LocalFrame* grandChildFrame = toLocalFrame(childFrame->tree().firstChild());
    FrameView* childFrameView = childFrame->view();
    FrameView* grandChildFrameView = grandChildFrame->view();
    EXPECT_FALSE(frameView->canThrottleRendering());
    EXPECT_FALSE(childFrameView->canThrottleRendering());
    EXPECT_FALSE(grandChildFrameView->canThrottleRendering());

    // Hidden cross origin frames are throttled.
    frameElement.setAttribute("style", "transform: translateY(480px)");
    compositeFrame();
    EXPECT_FALSE(frameView->canThrottleRendering());
    EXPECT_FALSE(childFrameView->canThrottleRendering());
    EXPECT_TRUE(grandChildFrameView->canThrottleRendering());
}

TEST_F(FrameThrottlingTest, throttledLifecycleUpdate)
{
    WebElement frameElement = setupPageWithThrottleableFrame();
    FrameView* frameView = document().view();
    LocalFrame* childFrame = toLocalFrame(frameView->frame().tree().firstChild());
    FrameView* childFrameView = childFrame->view();

    // Enable throttling for the child frame.
    frameElement.setAttribute("style", "transform: translateY(480px)");
    compositeFrame();
    EXPECT_TRUE(childFrameView->canThrottleRendering());
    EXPECT_EQ(DocumentLifecycle::PaintInvalidationClean, childFrame->document()->lifecycle().state());

    // Mutating the throttled frame followed by a beginFrame will not result in
    // a complete lifecycle update.
    frameElement.setAttribute("width", "50");
    compositeFrame();
    EXPECT_EQ(DocumentLifecycle::StyleClean, childFrame->document()->lifecycle().state());

    // A hit test will force a complete lifecycle update.
    webView().hitTestResultAt(WebPoint(0, 0));
    EXPECT_EQ(DocumentLifecycle::CompositingClean, childFrame->document()->lifecycle().state());
}

TEST_F(FrameThrottlingTest, unthrottlingFrameSchedulesAnimation)
{
    WebElement frameElement = setupPageWithThrottleableFrame();
    FrameView* frameView = document().view();
    LocalFrame* childFrame = toLocalFrame(frameView->frame().tree().firstChild());
    FrameView* childFrameView = childFrame->view();

    // First make the child hidden to enable throttling.
    frameElement.setAttribute("style", "transform: translateY(480px)");
    compositeFrame();
    EXPECT_TRUE(childFrameView->canThrottleRendering());
    EXPECT_FALSE(m_layerTreeView.needsAnimate());

    // Then bring it back on-screen. This should schedule an animation update.
    frameElement.setAttribute("style", "");
    compositeFrame();
    EXPECT_TRUE(m_layerTreeView.needsAnimate());
}

TEST_F(FrameThrottlingTest, mutatingThrottledFrameDoesNotCauseAnimation)
{
    loadHTML("<iframe id=frame sandbox srcdoc='<style> html { background: red; } </style>'></iframe>");
    WebExceptionCode ec;
    WebElement frameElement = root().querySelector("#frame", ec);
    EXPECT_EQ(0, ec);

    // Check that the frame initially shows up.
    m_layerTreeView.setDeferCommits(false);
    SimDisplayItemList displayItems1 = compositeFrame();
    EXPECT_TRUE(displayItems1.contains(SimCanvas::Rect, "red"));

    // Move the frame offscreen to throttle it.
    FrameView* frameView = document().view();
    LocalFrame* childFrame = toLocalFrame(frameView->frame().tree().firstChild());
    frameElement.setAttribute("style", "transform: translateY(480px)");
    compositeFrame();
    EXPECT_TRUE(childFrame->view()->canThrottleRendering());

    // Mutating the throttled frame should not cause an animation to be scheduled.
    childFrame->document()->documentElement()->setInnerHTML("<style> html { background: green; } </style>", ASSERT_NO_EXCEPTION);
    EXPECT_FALSE(m_layerTreeView.needsAnimate());

    // Moving the frame back on screen to unthrottle it.
    frameElement.setAttribute("style", "");
    EXPECT_TRUE(m_layerTreeView.needsAnimate());

    // The first frame we composite after unthrottling won't contain the
    // frame's new contents because unthrottling happens at the end of the
    // lifecycle update. We need to do another composite to refresh the frame's
    // contents.
    SimDisplayItemList displayItems2 = compositeFrame();
    EXPECT_FALSE(displayItems2.contains(SimCanvas::Rect, "green"));
    EXPECT_TRUE(m_layerTreeView.needsAnimate());

    SimDisplayItemList displayItems3 = compositeFrame();
    EXPECT_TRUE(displayItems3.contains(SimCanvas::Rect, "green"));
}

TEST_F(FrameThrottlingTest, synchronousLayoutInThrottledFrame)
{
    // Create a hidden frame which is throttled.
    loadHTML("<iframe id=frame sandbox srcdoc='<div id=div></div>'></iframe>");
    WebExceptionCode ec;
    WebElement frameElement = root().querySelector("#frame", ec);
    EXPECT_EQ(0, ec);
    frameElement.setAttribute("style", "transform: translateY(480px)");
    compositeFrame();

    FrameView* frameView = document().view();
    LocalFrame* childFrame = toLocalFrame(frameView->frame().tree().firstChild());

    // Change the size of a div in the throttled frame.
    Element* divElement = childFrame->document()->getElementById("div");
    divElement->setAttribute(HTMLNames::styleAttr, "width: 50px");

    // Querying the width of the div should do a synchronous layout update even
    // though the frame is being throttled.
    EXPECT_EQ(50, divElement->clientWidth());
}

} // namespace blink
