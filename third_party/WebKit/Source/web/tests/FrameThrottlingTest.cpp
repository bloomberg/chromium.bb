// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/paint/PaintLayer.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebDisplayItemList.h"
#include "public/platform/WebLayer.h"
#include "public/web/WebHitTestResult.h"
#include "public/web/WebSettings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebRemoteFrameImpl.h"
#include "web/tests/sim/SimCompositor.h"
#include "web/tests/sim/SimDisplayItemList.h"
#include "web/tests/sim/SimRequest.h"
#include "web/tests/sim/SimTest.h"

using testing::_;

namespace blink {

using namespace HTMLNames;

// NOTE: This test uses <iframe sandbox> to create cross origin iframes.

namespace {

class MockWebDisplayItemList : public WebDisplayItemList {
public:
    ~MockWebDisplayItemList() override {}

    MOCK_METHOD2(appendDrawingItem, void(const WebRect&, sk_sp<const SkPicture>));
};

void paintRecursively(GraphicsLayer* layer, WebDisplayItemList* displayItems)
{
    if (layer->drawsContent()) {
        layer->setNeedsDisplay();
        layer->contentLayerDelegateForTesting()->paintContents(displayItems, ContentLayerDelegate::PaintDefaultBehaviorForTest);
    }
    for (const auto& child : layer->children())
        paintRecursively(child, displayItems);
}

} // namespace

class FrameThrottlingTest : public SimTest {
protected:
    FrameThrottlingTest()
    {
        webView().resize(WebSize(640, 480));
    }

    SimDisplayItemList compositeFrame()
    {
        SimDisplayItemList displayItems = compositor().beginFrame();
        // Ensure intersection observer notifications get delivered.
        testing::runPendingTasks();
        return displayItems;
    }

    // Number of rectangles that make up the root layer's touch handler region.
    size_t touchHandlerRegionSize()
    {
        return webView().mainFrameImpl()->frame()->contentLayoutItem().layer()->graphicsLayerBacking()->platformLayer()->touchEventHandlerRegion().size();
    }
};

TEST_F(FrameThrottlingTest, ThrottleInvisibleFrames)
{
    SimRequest mainResource("https://example.com/", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe sandbox id=frame></iframe>");

    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
    auto* frameDocument = frameElement->contentDocument();

    // Initially both frames are visible.
    EXPECT_FALSE(document().view()->isHiddenForThrottling());
    EXPECT_FALSE(frameDocument->view()->isHiddenForThrottling());

    // Moving the child fully outside the parent makes it invisible.
    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    compositeFrame();
    EXPECT_FALSE(document().view()->isHiddenForThrottling());
    EXPECT_TRUE(frameDocument->view()->isHiddenForThrottling());

    // A partially visible child is considered visible.
    frameElement->setAttribute(styleAttr, "transform: translate(-50px, 0px, 0px)");
    compositeFrame();
    EXPECT_FALSE(document().view()->isHiddenForThrottling());
    EXPECT_FALSE(frameDocument->view()->isHiddenForThrottling());
}

TEST_F(FrameThrottlingTest, ViewportVisibilityFullyClipped)
{
    SimRequest mainResource("https://example.com/", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe sandbox id=frame></iframe>");

    // A child which is fully clipped away by its ancestor should become invisible.
    webView().resize(WebSize(0, 0));
    compositeFrame();

    EXPECT_TRUE(document().view()->isHiddenForThrottling());

    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
    auto* frameDocument = frameElement->contentDocument();
    EXPECT_TRUE(frameDocument->view()->isHiddenForThrottling());
}

TEST_F(FrameThrottlingTest, HiddenSameOriginFramesAreNotThrottled)
{
    SimRequest mainResource("https://example.com/", "text/html");
    SimRequest frameResource("https://example.com/iframe.html", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe id=frame src=iframe.html></iframe>");
    frameResource.complete("<iframe id=innerFrame></iframe>");

    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
    auto* frameDocument = frameElement->contentDocument();

    HTMLIFrameElement* innerFrameElement = toHTMLIFrameElement(frameDocument->getElementById("innerFrame"));
    auto* innerFrameDocument = innerFrameElement->contentDocument();

    EXPECT_FALSE(document().view()->canThrottleRendering());
    EXPECT_FALSE(frameDocument->view()->canThrottleRendering());
    EXPECT_FALSE(innerFrameDocument->view()->canThrottleRendering());

    // Hidden same origin frames are not throttled.
    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    compositeFrame();
    EXPECT_FALSE(document().view()->canThrottleRendering());
    EXPECT_FALSE(frameDocument->view()->canThrottleRendering());
    EXPECT_FALSE(innerFrameDocument->view()->canThrottleRendering());
}

TEST_F(FrameThrottlingTest, HiddenCrossOriginFramesAreThrottled)
{
    // Create a document with doubly nested iframes.
    SimRequest mainResource("https://example.com/", "text/html");
    SimRequest frameResource("https://example.com/iframe.html", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe id=frame src=iframe.html></iframe>");
    frameResource.complete("<iframe id=innerFrame sandbox></iframe>");

    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
    auto* frameDocument = frameElement->contentDocument();

    auto* innerFrameElement = toHTMLIFrameElement(frameDocument->getElementById("innerFrame"));
    auto* innerFrameDocument = innerFrameElement->contentDocument();

    EXPECT_FALSE(document().view()->canThrottleRendering());
    EXPECT_FALSE(frameDocument->view()->canThrottleRendering());
    EXPECT_FALSE(innerFrameDocument->view()->canThrottleRendering());

    // Hidden cross origin frames are throttled.
    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    compositeFrame();
    EXPECT_FALSE(document().view()->canThrottleRendering());
    EXPECT_FALSE(frameDocument->view()->canThrottleRendering());
    EXPECT_TRUE(innerFrameDocument->view()->canThrottleRendering());
}

TEST_F(FrameThrottlingTest, ThrottledLifecycleUpdate)
{
    SimRequest mainResource("https://example.com/", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe sandbox id=frame></iframe>");

    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
    auto* frameDocument = frameElement->contentDocument();

    // Enable throttling for the child frame.
    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    compositeFrame();
    EXPECT_TRUE(frameDocument->view()->canThrottleRendering());
    EXPECT_EQ(DocumentLifecycle::PaintClean, frameDocument->lifecycle().state());

    // Mutating the throttled frame followed by a beginFrame will not result in
    // a complete lifecycle update.
    // TODO(skyostil): these expectations are either wrong, or the test is
    // not exercising the code correctly. PaintClean means the entire lifecycle
    // ran.
    frameElement->setAttribute(widthAttr, "50");
    compositeFrame();
    EXPECT_EQ(DocumentLifecycle::PaintClean, frameDocument->lifecycle().state());

    // A hit test will not force a complete lifecycle update.
    webView().hitTestResultAt(WebPoint(0, 0));
    EXPECT_EQ(DocumentLifecycle::PaintClean, frameDocument->lifecycle().state());
}

TEST_F(FrameThrottlingTest, UnthrottlingFrameSchedulesAnimation)
{
    SimRequest mainResource("https://example.com/", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe sandbox id=frame></iframe>");

    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));

    // First make the child hidden to enable throttling.
    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    compositeFrame();
    EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());
    EXPECT_FALSE(compositor().needsAnimate());

    // Then bring it back on-screen. This should schedule an animation update.
    frameElement->setAttribute(styleAttr, "");
    compositeFrame();
    EXPECT_TRUE(compositor().needsAnimate());
}

TEST_F(FrameThrottlingTest, MutatingThrottledFrameDoesNotCauseAnimation)
{
    SimRequest mainResource("https://example.com/", "text/html");
    SimRequest frameResource("https://example.com/iframe.html", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe id=frame sandbox src=iframe.html></iframe>");
    frameResource.complete("<style> html { background: red; } </style>");

    // Check that the frame initially shows up.
    auto displayItems1 = compositeFrame();
    EXPECT_TRUE(displayItems1.contains(SimCanvas::Rect, "red"));

    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));

    // Move the frame offscreen to throttle it.
    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    compositeFrame();
    EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());

    // Mutating the throttled frame should not cause an animation to be scheduled.
    frameElement->contentDocument()->documentElement()->setAttribute(styleAttr, "background: green");
    EXPECT_FALSE(compositor().needsAnimate());

    // Move the frame back on screen to unthrottle it.
    frameElement->setAttribute(styleAttr, "");
    EXPECT_TRUE(compositor().needsAnimate());

    // The first frame we composite after unthrottling won't contain the
    // frame's new contents because unthrottling happens at the end of the
    // lifecycle update. We need to do another composite to refresh the frame's
    // contents.
    auto displayItems2 = compositeFrame();
    EXPECT_FALSE(displayItems2.contains(SimCanvas::Rect, "green"));
    EXPECT_TRUE(compositor().needsAnimate());

    auto displayItems3 = compositeFrame();
    EXPECT_TRUE(displayItems3.contains(SimCanvas::Rect, "green"));
}

TEST_F(FrameThrottlingTest, SynchronousLayoutInThrottledFrame)
{
    // Create a hidden frame which is throttled.
    SimRequest mainResource("https://example.com/", "text/html");
    SimRequest frameResource("https://example.com/iframe.html", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe id=frame sandbox src=iframe.html></iframe>");
    frameResource.complete("<div id=div></div>");

    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));

    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    compositeFrame();

    // Change the size of a div in the throttled frame.
    auto* divElement = frameElement->contentDocument()->getElementById("div");
    divElement->setAttribute(styleAttr, "width: 50px");

    // Querying the width of the div should do a synchronous layout update even
    // though the frame is being throttled.
    EXPECT_EQ(50, divElement->clientWidth());
}

TEST_F(FrameThrottlingTest, UnthrottlingTriggersRepaint)
{
    // Create a hidden frame which is throttled.
    SimRequest mainResource("https://example.com/", "text/html");
    SimRequest frameResource("https://example.com/iframe.html", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe id=frame sandbox src=iframe.html></iframe>");
    frameResource.complete("<style> html { background: green; } </style>");

    // Move the frame offscreen to throttle it.
    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
    compositeFrame();
    EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());

    // Scroll down to unthrottle the frame. The first frame we composite after
    // scrolling won't contain the frame yet, but will schedule another repaint.
    webView().mainFrameImpl()->frameView()->setScrollPosition(DoublePoint(0, 480), ProgrammaticScroll);
    auto displayItems = compositeFrame();
    EXPECT_FALSE(displayItems.contains(SimCanvas::Rect, "green"));

    // Now the frame contents should be visible again.
    auto displayItems2 = compositeFrame();
    EXPECT_TRUE(displayItems2.contains(SimCanvas::Rect, "green"));
}

TEST_F(FrameThrottlingTest, UnthrottlingTriggersRepaintInCompositedChild)
{
    // Create a hidden frame with a composited child layer.
    SimRequest mainResource("https://example.com/", "text/html");
    SimRequest frameResource("https://example.com/iframe.html", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe id=frame sandbox src=iframe.html></iframe>");
    frameResource.complete(
        "<style>"
        "div { "
        "  width: 100px;"
        "  height: 100px;"
        "  background-color: green;"
        "  transform: translateZ(0);"
        "}"
        "</style><div></div>");

    // Move the frame offscreen to throttle it.
    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
    compositeFrame();
    EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());

    // Scroll down to unthrottle the frame. The first frame we composite after
    // scrolling won't contain the frame yet, but will schedule another repaint.
    webView().mainFrameImpl()->frameView()->setScrollPosition(DoublePoint(0, 480), ProgrammaticScroll);
    auto displayItems = compositeFrame();
    EXPECT_FALSE(displayItems.contains(SimCanvas::Rect, "green"));

    // Now the composited child contents should be visible again.
    auto displayItems2 = compositeFrame();
    EXPECT_TRUE(displayItems2.contains(SimCanvas::Rect, "green"));
}

TEST_F(FrameThrottlingTest, ChangeStyleInThrottledFrame)
{
    // Create a hidden frame which is throttled.
    SimRequest mainResource("https://example.com/", "text/html");
    SimRequest frameResource("https://example.com/iframe.html", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe id=frame sandbox src=iframe.html></iframe>");
    frameResource.complete("<style> html { background: red; } </style>");

    // Move the frame offscreen to throttle it.
    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
    compositeFrame();
    EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());

    // Change the background color of the frame's contents from red to green.
    frameElement->contentDocument()->body()->setAttribute(styleAttr, "background: green");

    // Scroll down to unthrottle the frame.
    webView().mainFrameImpl()->frameView()->setScrollPosition(DoublePoint(0, 480), ProgrammaticScroll);
    auto displayItems = compositeFrame();
    EXPECT_FALSE(displayItems.contains(SimCanvas::Rect, "red"));
    EXPECT_FALSE(displayItems.contains(SimCanvas::Rect, "green"));

    // Make sure the new style shows up instead of the old one.
    auto displayItems2 = compositeFrame();
    EXPECT_TRUE(displayItems2.contains(SimCanvas::Rect, "green"));
}

TEST_F(FrameThrottlingTest, ThrottledFrameWithFocus)
{
    webView().settings()->setJavaScriptEnabled(true);
    webView().settings()->setAcceleratedCompositingEnabled(true);
    RuntimeEnabledFeatures::setCompositedSelectionUpdateEnabled(true);

    // Create a hidden frame which is throttled and has a text selection.
    SimRequest mainResource("https://example.com/", "text/html");
    SimRequest frameResource("https://example.com/iframe.html", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe id=frame sandbox=allow-scripts src=iframe.html></iframe>");
    frameResource.complete(
        "some text to select\n"
        "<script>\n"
        "var range = document.createRange();\n"
        "range.selectNode(document.body);\n"
        "window.getSelection().addRange(range);\n"
        "</script>\n");

    // Move the frame offscreen to throttle it.
    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
    compositeFrame();
    EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());

    // Give the frame focus and do another composite. The selection in the
    // compositor should be cleared because the frame is throttled.
    EXPECT_FALSE(compositor().hasSelection());
    document().page()->focusController().setFocusedFrame(frameElement->contentDocument()->frame());
    document().body()->setAttribute(styleAttr, "background: green");
    compositeFrame();
    EXPECT_FALSE(compositor().hasSelection());
}

TEST(RemoteFrameThrottlingTest, ThrottledLocalRoot)
{
    FrameTestHelpers::TestWebViewClient viewClient;
    WebViewImpl* webView = WebViewImpl::create(&viewClient);
    webView->resize(WebSize(640, 480));

    // Create a remote root frame with a local child frame.
    FrameTestHelpers::TestWebRemoteFrameClient remoteClient;
    webView->setMainFrame(remoteClient.frame());
    remoteClient.frame()->setReplicatedOrigin(WebSecurityOrigin::createUnique());

    WebFrameOwnerProperties properties;
    WebRemoteFrame* rootFrame = webView->mainFrame()->toWebRemoteFrame();
    WebLocalFrame* localFrame = FrameTestHelpers::createLocalChild(rootFrame);

    WebString baseURL("http://internal.test/");
    URLTestHelpers::registerMockedURLFromBaseURL(baseURL, "simple_div.html");
    FrameTestHelpers::loadFrame(localFrame, baseURL.utf8() + "simple_div.html");

    FrameView* frameView = toWebLocalFrameImpl(localFrame)->frameView();
    EXPECT_TRUE(frameView->frame().isLocalRoot());

    // Enable throttling for the child frame.
    frameView->setFrameRect(IntRect(0, 480, frameView->width(), frameView->height()));
    frameView->frame().securityContext()->setSecurityOrigin(SecurityOrigin::createUnique());
    frameView->updateAllLifecyclePhases();
    testing::runPendingTasks();
    EXPECT_TRUE(frameView->canThrottleRendering());

    Document* frameDocument = frameView->frame().document();
    EXPECT_EQ(DocumentLifecycle::PaintClean, frameDocument->lifecycle().state());

    // Mutate the local child frame contents.
    auto* divElement = frameDocument->getElementById("div");
    divElement->setAttribute(styleAttr, "width: 50px");
    EXPECT_EQ(DocumentLifecycle::VisualUpdatePending, frameDocument->lifecycle().state());

    // Update the lifecycle again. The frame's lifecycle should not advance
    // because of throttling even though it is the local root.
    DocumentLifecycle::AllowThrottlingScope throttlingScope(frameDocument->lifecycle());
    frameView->updateAllLifecyclePhases();
    testing::runPendingTasks();
    EXPECT_EQ(DocumentLifecycle::VisualUpdatePending, frameDocument->lifecycle().state());
    webView->close();
}

TEST_F(FrameThrottlingTest, ScrollingCoordinatorShouldSkipThrottledFrame)
{
    webView().settings()->setAcceleratedCompositingEnabled(true);

    // Create a hidden frame which is throttled.
    SimRequest mainResource("https://example.com/", "text/html");
    SimRequest frameResource("https://example.com/iframe.html", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe id=frame sandbox src=iframe.html></iframe>");
    frameResource.complete("<style> html { background-image: linear-gradient(red, blue); background-attachment: fixed; } </style>");

    // Move the frame offscreen to throttle it.
    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
    compositeFrame();
    EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());

    // Change style of the frame's content to make it in VisualUpdatePending state.
    frameElement->contentDocument()->body()->setAttribute(styleAttr, "background: green");
    // Change root frame's layout so that the next lifecycle update will call
    // ScrollingCoordinator::updateAfterCompositingChangeIfNeeded().
    document().body()->setAttribute(styleAttr, "margin: 20px");
    EXPECT_EQ(DocumentLifecycle::VisualUpdatePending, frameElement->contentDocument()->lifecycle().state());

    DocumentLifecycle::AllowThrottlingScope throttlingScope(document().lifecycle());
    // This will call ScrollingCoordinator::updateAfterCompositingChangeIfNeeded() and should not
    // cause assert failure about isAllowedToQueryCompositingState() in the throttled frame.
    document().view()->updateAllLifecyclePhases();
    testing::runPendingTasks();
    EXPECT_EQ(DocumentLifecycle::VisualUpdatePending, frameElement->contentDocument()->lifecycle().state());
    // The fixed background in the throttled sub frame should not cause main thread scrolling.
    EXPECT_FALSE(document().view()->shouldScrollOnMainThread());

    // Make the frame visible by changing its transform. This doesn't cause a
    // layout, but should still unthrottle the frame.
    frameElement->setAttribute(styleAttr, "transform: translateY(0px)");
    compositeFrame();
    EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
    // The fixed background in the throttled sub frame should be considered.
    EXPECT_TRUE(document().view()->shouldScrollOnMainThread());
}

TEST_F(FrameThrottlingTest, ScrollingCoordinatorShouldSkipCompositedThrottledFrame)
{
    webView().settings()->setAcceleratedCompositingEnabled(true);
    webView().settings()->setPreferCompositingToLCDTextEnabled(true);

    // Create a hidden frame which is throttled.
    SimRequest mainResource("https://example.com/", "text/html");
    SimRequest frameResource("https://example.com/iframe.html", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe id=frame sandbox src=iframe.html></iframe>");
    frameResource.complete("<div style='height: 2000px'></div>");

    // Move the frame offscreen to throttle it.
    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
    compositeFrame();
    EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());

    // Change style of the frame's content to make it in VisualUpdatePending state.
    frameElement->contentDocument()->body()->setAttribute(styleAttr, "background: green");
    // Change root frame's layout so that the next lifecycle update will call
    // ScrollingCoordinator::updateAfterCompositingChangeIfNeeded().
    document().body()->setAttribute(styleAttr, "margin: 20px");
    EXPECT_EQ(DocumentLifecycle::VisualUpdatePending, frameElement->contentDocument()->lifecycle().state());

    DocumentLifecycle::AllowThrottlingScope throttlingScope(document().lifecycle());
    // This will call ScrollingCoordinator::updateAfterCompositingChangeIfNeeded() and should not
    // cause assert failure about isAllowedToQueryCompositingState() in the throttled frame.
    compositeFrame();
    EXPECT_EQ(DocumentLifecycle::VisualUpdatePending, frameElement->contentDocument()->lifecycle().state());

    // Make the frame visible by changing its transform. This doesn't cause a
    // layout, but should still unthrottle the frame.
    frameElement->setAttribute(styleAttr, "transform: translateY(0px)");
    compositeFrame(); // Unthrottle the frame.
    compositeFrame(); // Handle the pending visual update of the unthrottled frame.
    EXPECT_EQ(DocumentLifecycle::PaintClean, frameElement->contentDocument()->lifecycle().state());
    EXPECT_TRUE(frameElement->contentDocument()->view()->usesCompositedScrolling());
}

TEST_F(FrameThrottlingTest, UnthrottleByTransformingWithoutLayout)
{
    webView().settings()->setAcceleratedCompositingEnabled(true);

    // Create a hidden frame which is throttled.
    SimRequest mainResource("https://example.com/", "text/html");
    SimRequest frameResource("https://example.com/iframe.html", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe id=frame sandbox src=iframe.html></iframe>");
    frameResource.complete("");

    // Move the frame offscreen to throttle it.
    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
    compositeFrame();
    EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());

    // Make the frame visible by changing its transform. This doesn't cause a
    // layout, but should still unthrottle the frame.
    frameElement->setAttribute(styleAttr, "transform: translateY(0px)");
    compositeFrame();
    EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
}

TEST_F(FrameThrottlingTest, ThrottledTopLevelEventHandlerIgnored)
{
    webView().settings()->setAcceleratedCompositingEnabled(true);
    webView().settings()->setJavaScriptEnabled(true);
    EXPECT_EQ(0u, touchHandlerRegionSize());

    // Create a frame which is throttled and has two different types of
    // top-level touchstart handlers.
    SimRequest mainResource("https://example.com/", "text/html");
    SimRequest frameResource("https://example.com/iframe.html", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe id=frame sandbox=allow-scripts src=iframe.html></iframe>");
    frameResource.complete(
        "<script>"
        "window.addEventListener('touchstart', function(){});"
        "document.addEventListener('touchstart', function(){});"
        "</script>");
    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    compositeFrame(); // Throttle the frame.
    compositeFrame(); // Update touch handler regions.

    // The touch handlers in the throttled frame should have been ignored.
    EXPECT_EQ(0u, touchHandlerRegionSize());

    // Unthrottling the frame makes the touch handlers active again. Note that
    // both handlers get combined into the same rectangle in the region, so
    // there is only one rectangle in total.
    frameElement->setAttribute(styleAttr, "transform: translateY(0px)");
    compositeFrame(); // Unthrottle the frame.
    compositeFrame(); // Update touch handler regions.
    EXPECT_EQ(1u, touchHandlerRegionSize());
}

TEST_F(FrameThrottlingTest, ThrottledEventHandlerIgnored)
{
    webView().settings()->setAcceleratedCompositingEnabled(true);
    webView().settings()->setJavaScriptEnabled(true);
    EXPECT_EQ(0u, touchHandlerRegionSize());

    // Create a frame which is throttled and has a non-top-level touchstart handler.
    SimRequest mainResource("https://example.com/", "text/html");
    SimRequest frameResource("https://example.com/iframe.html", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe id=frame sandbox=allow-scripts src=iframe.html></iframe>");
    frameResource.complete(
        "<div id=d>touch handler</div>"
        "<script>"
        "document.querySelector('#d').addEventListener('touchstart', function(){});"
        "</script>");
    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
    frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
    compositeFrame(); // Throttle the frame.
    compositeFrame(); // Update touch handler regions.

    // The touch handler in the throttled frame should have been ignored.
    EXPECT_EQ(0u, touchHandlerRegionSize());

    // Unthrottling the frame makes the touch handler active again.
    frameElement->setAttribute(styleAttr, "transform: translateY(0px)");
    compositeFrame(); // Unthrottle the frame.
    compositeFrame(); // Update touch handler regions.
    EXPECT_EQ(1u, touchHandlerRegionSize());
}

TEST_F(FrameThrottlingTest, PaintingViaContentLayerDelegateIsThrottled)
{
    webView().settings()->setAcceleratedCompositingEnabled(true);
    webView().settings()->setPreferCompositingToLCDTextEnabled(true);

    // Create a hidden frame which is throttled.
    SimRequest mainResource("https://example.com/", "text/html");
    SimRequest frameResource("https://example.com/iframe.html", "text/html");

    loadURL("https://example.com/");
    mainResource.complete("<iframe id=frame sandbox src=iframe.html></iframe>");
    frameResource.complete("throttled");
    compositeFrame();

    // Move the frame offscreen to throttle it and make sure it is backed by a
    // graphics layer.
    auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
    frameElement->setAttribute(styleAttr, "transform: translateY(480px) translateZ(0px)");
    EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
    compositeFrame();
    EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());

    // If painting of the iframe is throttled, we should only receive two
    // drawing items.
    MockWebDisplayItemList displayItems;
    EXPECT_CALL(displayItems, appendDrawingItem(_, _))
        .Times(2);

    GraphicsLayer* layer = webView().rootGraphicsLayer();
    paintRecursively(layer, &displayItems);
}

} // namespace blink
