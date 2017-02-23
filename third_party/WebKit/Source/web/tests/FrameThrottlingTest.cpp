// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/WebDisplayItemList.h"
#include "public/platform/WebLayer.h"
#include "public/web/WebFrameContentDumper.h"
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

  MOCK_METHOD2(appendDrawingItem,
               void(const WebRect&, sk_sp<const PaintRecord>));
};

void paintRecursively(GraphicsLayer* layer, WebDisplayItemList* displayItems) {
  if (layer->drawsContent()) {
    layer->setNeedsDisplay();
    layer->contentLayerDelegateForTesting()->paintContents(
        displayItems, ContentLayerDelegate::PaintDefaultBehaviorForTest);
  }
  for (const auto& child : layer->children())
    paintRecursively(child, displayItems);
}

}  // namespace

class FrameThrottlingTest : public SimTest,
                            public ::testing::WithParamInterface<bool>,
                            private ScopedRootLayerScrollingForTest {
 protected:
  FrameThrottlingTest() : ScopedRootLayerScrollingForTest(GetParam()) {
    webView().resize(WebSize(640, 480));
  }

  SimDisplayItemList compositeFrame() {
    SimDisplayItemList displayItems = compositor().beginFrame();
    // Ensure intersection observer notifications get delivered.
    testing::runPendingTasks();
    return displayItems;
  }

  // Number of rectangles that make up the root layer's touch handler region.
  size_t touchHandlerRegionSize() {
    size_t result = 0;
    PaintLayer* layer =
        webView().mainFrameImpl()->frame()->contentLayoutObject()->layer();
    GraphicsLayer* ownGraphicsLayer =
        layer->graphicsLayerBacking(&layer->layoutObject());
    if (ownGraphicsLayer) {
      result +=
          ownGraphicsLayer->platformLayer()->touchEventHandlerRegion().size();
    }
    GraphicsLayer* childGraphicsLayer = layer->graphicsLayerBacking();
    if (childGraphicsLayer && childGraphicsLayer != ownGraphicsLayer) {
      result +=
          childGraphicsLayer->platformLayer()->touchEventHandlerRegion().size();
    }
    return result;
  }
};

INSTANTIATE_TEST_CASE_P(All, FrameThrottlingTest, ::testing::Bool());

TEST_P(FrameThrottlingTest, ThrottleInvisibleFrames) {
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
  frameElement->setAttribute(styleAttr,
                             "transform: translate(-50px, 0px, 0px)");
  compositeFrame();
  EXPECT_FALSE(document().view()->isHiddenForThrottling());
  EXPECT_FALSE(frameDocument->view()->isHiddenForThrottling());
}

TEST_P(FrameThrottlingTest, HiddenSameOriginFramesAreNotThrottled) {
  SimRequest mainResource("https://example.com/", "text/html");
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  loadURL("https://example.com/");
  mainResource.complete("<iframe id=frame src=iframe.html></iframe>");
  frameResource.complete("<iframe id=innerFrame></iframe>");

  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
  auto* frameDocument = frameElement->contentDocument();

  HTMLIFrameElement* innerFrameElement =
      toHTMLIFrameElement(frameDocument->getElementById("innerFrame"));
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

TEST_P(FrameThrottlingTest, HiddenCrossOriginFramesAreThrottled) {
  // Create a document with doubly nested iframes.
  SimRequest mainResource("https://example.com/", "text/html");
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  loadURL("https://example.com/");
  mainResource.complete("<iframe id=frame src=iframe.html></iframe>");
  frameResource.complete("<iframe id=innerFrame sandbox></iframe>");

  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
  auto* frameDocument = frameElement->contentDocument();

  auto* innerFrameElement =
      toHTMLIFrameElement(frameDocument->getElementById("innerFrame"));
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

TEST_P(FrameThrottlingTest, HiddenCrossOriginZeroByZeroFramesAreNotThrottled) {
  // Create a document with doubly nested iframes.
  SimRequest mainResource("https://example.com/", "text/html");
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  loadURL("https://example.com/");
  mainResource.complete("<iframe id=frame src=iframe.html></iframe>");
  frameResource.complete(
      "<iframe id=innerFrame width=0 height=0 sandbox></iframe>");

  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
  auto* frameDocument = frameElement->contentDocument();

  auto* innerFrameElement =
      toHTMLIFrameElement(frameDocument->getElementById("innerFrame"));
  auto* innerFrameDocument = innerFrameElement->contentDocument();

  EXPECT_FALSE(document().view()->canThrottleRendering());
  EXPECT_FALSE(frameDocument->view()->canThrottleRendering());
  EXPECT_FALSE(innerFrameDocument->view()->canThrottleRendering());

  // The frame is not throttled because its dimensions are 0x0.
  frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
  compositeFrame();
  EXPECT_FALSE(document().view()->canThrottleRendering());
  EXPECT_FALSE(frameDocument->view()->canThrottleRendering());
  EXPECT_FALSE(innerFrameDocument->view()->canThrottleRendering());
}

TEST_P(FrameThrottlingTest, ThrottledLifecycleUpdate) {
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

TEST_P(FrameThrottlingTest, UnthrottlingFrameSchedulesAnimation) {
  SimRequest mainResource("https://example.com/", "text/html");

  loadURL("https://example.com/");
  mainResource.complete("<iframe sandbox id=frame></iframe>");

  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));

  // First make the child hidden to enable throttling.
  frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
  compositeFrame();
  EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());
  EXPECT_FALSE(compositor().needsBeginFrame());

  // Then bring it back on-screen. This should schedule an animation update.
  frameElement->setAttribute(styleAttr, "");
  compositeFrame();
  EXPECT_TRUE(compositor().needsBeginFrame());
}

TEST_P(FrameThrottlingTest, MutatingThrottledFrameDoesNotCauseAnimation) {
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
  frameElement->contentDocument()->documentElement()->setAttribute(
      styleAttr, "background: green");
  EXPECT_FALSE(compositor().needsBeginFrame());

  // Move the frame back on screen to unthrottle it.
  frameElement->setAttribute(styleAttr, "");
  EXPECT_TRUE(compositor().needsBeginFrame());

  // The first frame we composite after unthrottling won't contain the
  // frame's new contents because unthrottling happens at the end of the
  // lifecycle update. We need to do another composite to refresh the frame's
  // contents.
  auto displayItems2 = compositeFrame();
  EXPECT_FALSE(displayItems2.contains(SimCanvas::Rect, "green"));
  EXPECT_TRUE(compositor().needsBeginFrame());

  auto displayItems3 = compositeFrame();
  EXPECT_TRUE(displayItems3.contains(SimCanvas::Rect, "green"));
}

TEST_P(FrameThrottlingTest, SynchronousLayoutInThrottledFrame) {
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

TEST_P(FrameThrottlingTest, UnthrottlingTriggersRepaint) {
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
  webView()
      .mainFrameImpl()
      ->frameView()
      ->layoutViewportScrollableArea()
      ->setScrollOffset(ScrollOffset(0, 480), ProgrammaticScroll);
  auto displayItems = compositeFrame();
  EXPECT_FALSE(displayItems.contains(SimCanvas::Rect, "green"));

  // Now the frame contents should be visible again.
  auto displayItems2 = compositeFrame();
  EXPECT_TRUE(displayItems2.contains(SimCanvas::Rect, "green"));
}

TEST_P(FrameThrottlingTest, UnthrottlingTriggersRepaintInCompositedChild) {
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
  webView()
      .mainFrameImpl()
      ->frameView()
      ->layoutViewportScrollableArea()
      ->setScrollOffset(ScrollOffset(0, 480), ProgrammaticScroll);
  auto displayItems = compositeFrame();
  EXPECT_FALSE(displayItems.contains(SimCanvas::Rect, "green"));

  // Now the composited child contents should be visible again.
  auto displayItems2 = compositeFrame();
  EXPECT_TRUE(displayItems2.contains(SimCanvas::Rect, "green"));
}

TEST_P(FrameThrottlingTest, ChangeStyleInThrottledFrame) {
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
  frameElement->contentDocument()->body()->setAttribute(styleAttr,
                                                        "background: green");

  // Scroll down to unthrottle the frame.
  webView()
      .mainFrameImpl()
      ->frameView()
      ->layoutViewportScrollableArea()
      ->setScrollOffset(ScrollOffset(0, 480), ProgrammaticScroll);
  auto displayItems = compositeFrame();
  EXPECT_FALSE(displayItems.contains(SimCanvas::Rect, "red"));
  EXPECT_FALSE(displayItems.contains(SimCanvas::Rect, "green"));

  // Make sure the new style shows up instead of the old one.
  auto displayItems2 = compositeFrame();
  EXPECT_TRUE(displayItems2.contains(SimCanvas::Rect, "green"));
}

TEST_P(FrameThrottlingTest, ChangeOriginInThrottledFrame) {
  // Create a hidden frame which is throttled.
  SimRequest mainResource("http://example.com/", "text/html");
  SimRequest frameResource("http://sub.example.com/iframe.html", "text/html");
  loadURL("http://example.com/");
  mainResource.complete(
      "<iframe style='position: absolute; top: 10000px' id=frame "
      "src=http://sub.example.com/iframe.html></iframe>");
  frameResource.complete("");

  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));

  compositeFrame();

  EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());
  EXPECT_TRUE(
      frameElement->contentDocument()->frame()->isCrossOriginSubframe());
  EXPECT_FALSE(frameElement->contentDocument()
                   ->view()
                   ->layoutView()
                   ->needsPaintPropertyUpdate());

  NonThrowableExceptionState exceptionState;

  // Security policy requires setting domain on both frames.
  document().setDomain(String("example.com"), exceptionState);
  frameElement->contentDocument()->setDomain(String("example.com"),
                                             exceptionState);

  EXPECT_FALSE(
      frameElement->contentDocument()->frame()->isCrossOriginSubframe());
  EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
  EXPECT_TRUE(frameElement->contentDocument()
                  ->view()
                  ->layoutView()
                  ->needsPaintPropertyUpdate());
}

TEST_P(FrameThrottlingTest, ThrottledFrameWithFocus) {
  webView().settings()->setJavaScriptEnabled(true);
  webView().settings()->setAcceleratedCompositingEnabled(true);
  RuntimeEnabledFeatures::setCompositedSelectionUpdateEnabled(true);

  // Create a hidden frame which is throttled and has a text selection.
  SimRequest mainResource("https://example.com/", "text/html");
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  loadURL("https://example.com/");
  mainResource.complete(
      "<iframe id=frame sandbox=allow-scripts src=iframe.html></iframe>");
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
  document().page()->focusController().setFocusedFrame(
      frameElement->contentDocument()->frame());
  document().body()->setAttribute(styleAttr, "background: green");
  compositeFrame();
  EXPECT_FALSE(compositor().hasSelection());
}

TEST_P(FrameThrottlingTest, ScrollingCoordinatorShouldSkipThrottledFrame) {
  webView().settings()->setAcceleratedCompositingEnabled(true);

  // Create a hidden frame which is throttled.
  SimRequest mainResource("https://example.com/", "text/html");
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  loadURL("https://example.com/");
  mainResource.complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frameResource.complete(
      "<style> html { background-image: linear-gradient(red, blue); "
      "background-attachment: fixed; } </style>");

  // Move the frame offscreen to throttle it.
  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
  frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
  EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
  compositeFrame();
  EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());

  // Change style of the frame's content to make it in VisualUpdatePending
  // state.
  frameElement->contentDocument()->body()->setAttribute(styleAttr,
                                                        "background: green");
  // Change root frame's layout so that the next lifecycle update will call
  // ScrollingCoordinator::updateAfterCompositingChangeIfNeeded().
  document().body()->setAttribute(styleAttr, "margin: 20px");
  EXPECT_EQ(DocumentLifecycle::VisualUpdatePending,
            frameElement->contentDocument()->lifecycle().state());

  DocumentLifecycle::AllowThrottlingScope throttlingScope(
      document().lifecycle());
  // This will call ScrollingCoordinator::updateAfterCompositingChangeIfNeeded()
  // and should not cause assert failure about
  // isAllowedToQueryCompositingState() in the throttled frame.
  document().view()->updateAllLifecyclePhases();
  testing::runPendingTasks();
  EXPECT_EQ(DocumentLifecycle::VisualUpdatePending,
            frameElement->contentDocument()->lifecycle().state());
  // The fixed background in the throttled sub frame should not cause main
  // thread scrolling.
  EXPECT_FALSE(document().view()->shouldScrollOnMainThread());

  // Make the frame visible by changing its transform. This doesn't cause a
  // layout, but should still unthrottle the frame.
  frameElement->setAttribute(styleAttr, "transform: translateY(0px)");
  compositeFrame();
  EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
  // The fixed background in the throttled sub frame should be considered.
  EXPECT_TRUE(
      frameElement->contentDocument()->view()->shouldScrollOnMainThread());
  EXPECT_FALSE(document().view()->shouldScrollOnMainThread());
}

TEST_P(FrameThrottlingTest, ScrollingCoordinatorShouldSkipThrottledLayer) {
  webView().settings()->setJavaScriptEnabled(true);
  webView().settings()->setAcceleratedCompositingEnabled(true);
  webView().settings()->setPreferCompositingToLCDTextEnabled(true);

  // Create a hidden frame which is throttled and has a touch handler inside a
  // composited layer.
  SimRequest mainResource("https://example.com/", "text/html");
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  loadURL("https://example.com/");
  mainResource.complete(
      "<iframe id=frame sandbox=allow-scripts src=iframe.html></iframe>");
  frameResource.complete(
      "<div id=div style='transform: translateZ(0)' ontouchstart='foo()'>touch "
      "handler</div>");

  // Move the frame offscreen to throttle it.
  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
  frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
  EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
  compositeFrame();
  EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());

  // Change style of the frame's content to make it in VisualUpdatePending
  // state.
  frameElement->contentDocument()->body()->setAttribute(styleAttr,
                                                        "background: green");
  // Change root frame's layout so that the next lifecycle update will call
  // ScrollingCoordinator::updateAfterCompositingChangeIfNeeded().
  document().body()->setAttribute(styleAttr, "margin: 20px");
  EXPECT_EQ(DocumentLifecycle::VisualUpdatePending,
            frameElement->contentDocument()->lifecycle().state());

  DocumentLifecycle::AllowThrottlingScope throttlingScope(
      document().lifecycle());
  // This will call ScrollingCoordinator::updateAfterCompositingChangeIfNeeded()
  // and should not cause assert failure about
  // isAllowedToQueryCompositingState() in the throttled frame.
  document().view()->updateAllLifecyclePhases();
  testing::runPendingTasks();
  EXPECT_EQ(DocumentLifecycle::VisualUpdatePending,
            frameElement->contentDocument()->lifecycle().state());
}

TEST_P(FrameThrottlingTest,
       ScrollingCoordinatorShouldSkipCompositedThrottledFrame) {
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

  // Change style of the frame's content to make it in VisualUpdatePending
  // state.
  frameElement->contentDocument()->body()->setAttribute(styleAttr,
                                                        "background: green");
  // Change root frame's layout so that the next lifecycle update will call
  // ScrollingCoordinator::updateAfterCompositingChangeIfNeeded().
  document().body()->setAttribute(styleAttr, "margin: 20px");
  EXPECT_EQ(DocumentLifecycle::VisualUpdatePending,
            frameElement->contentDocument()->lifecycle().state());

  DocumentLifecycle::AllowThrottlingScope throttlingScope(
      document().lifecycle());
  // This will call ScrollingCoordinator::updateAfterCompositingChangeIfNeeded()
  // and should not cause assert failure about
  // isAllowedToQueryCompositingState() in the throttled frame.
  compositeFrame();
  EXPECT_EQ(DocumentLifecycle::VisualUpdatePending,
            frameElement->contentDocument()->lifecycle().state());

  // Make the frame visible by changing its transform. This doesn't cause a
  // layout, but should still unthrottle the frame.
  frameElement->setAttribute(styleAttr, "transform: translateY(0px)");
  compositeFrame();  // Unthrottle the frame.
  compositeFrame();  // Handle the pending visual update of the unthrottled
                     // frame.
  EXPECT_EQ(DocumentLifecycle::PaintClean,
            frameElement->contentDocument()->lifecycle().state());
  EXPECT_TRUE(
      frameElement->contentDocument()->view()->usesCompositedScrolling());
}

TEST_P(FrameThrottlingTest, UnthrottleByTransformingWithoutLayout) {
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

TEST_P(FrameThrottlingTest, ThrottledTopLevelEventHandlerIgnored) {
  webView().settings()->setAcceleratedCompositingEnabled(true);
  webView().settings()->setJavaScriptEnabled(true);
  EXPECT_EQ(0u, touchHandlerRegionSize());

  // Create a frame which is throttled and has two different types of
  // top-level touchstart handlers.
  SimRequest mainResource("https://example.com/", "text/html");
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  loadURL("https://example.com/");
  mainResource.complete(
      "<iframe id=frame sandbox=allow-scripts src=iframe.html></iframe>");
  frameResource.complete(
      "<script>"
      "window.addEventListener('touchstart', function(){}, {passive: false});"
      "document.addEventListener('touchstart', function(){}, {passive: false});"
      "</script>");
  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
  frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
  compositeFrame();  // Throttle the frame.
  compositeFrame();  // Update touch handler regions.

  // The touch handlers in the throttled frame should have been ignored.
  EXPECT_EQ(0u, touchHandlerRegionSize());

  // Unthrottling the frame makes the touch handlers active again. Note that
  // both handlers get combined into the same rectangle in the region, so
  // there is only one rectangle in total.
  frameElement->setAttribute(styleAttr, "transform: translateY(0px)");
  compositeFrame();  // Unthrottle the frame.
  compositeFrame();  // Update touch handler regions.
  EXPECT_EQ(1u, touchHandlerRegionSize());
}

TEST_P(FrameThrottlingTest, ThrottledEventHandlerIgnored) {
  webView().settings()->setAcceleratedCompositingEnabled(true);
  webView().settings()->setJavaScriptEnabled(true);
  EXPECT_EQ(0u, touchHandlerRegionSize());

  // Create a frame which is throttled and has a non-top-level touchstart
  // handler.
  SimRequest mainResource("https://example.com/", "text/html");
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  loadURL("https://example.com/");
  mainResource.complete(
      "<iframe id=frame sandbox=allow-scripts src=iframe.html></iframe>");
  frameResource.complete(
      "<div id=d>touch handler</div>"
      "<script>"
      "document.querySelector('#d').addEventListener('touchstart', "
      "function(){});"
      "</script>");
  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
  frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
  compositeFrame();  // Throttle the frame.
  compositeFrame();  // Update touch handler regions.

  // The touch handler in the throttled frame should have been ignored.
  EXPECT_EQ(0u, touchHandlerRegionSize());

  // Unthrottling the frame makes the touch handler active again.
  frameElement->setAttribute(styleAttr, "transform: translateY(0px)");
  compositeFrame();  // Unthrottle the frame.
  compositeFrame();  // Update touch handler regions.
  EXPECT_EQ(1u, touchHandlerRegionSize());
}

TEST_P(FrameThrottlingTest, DumpThrottledFrame) {
  webView().settings()->setJavaScriptEnabled(true);

  // Create a frame which is throttled.
  SimRequest mainResource("https://example.com/", "text/html");
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  loadURL("https://example.com/");
  mainResource.complete(
      "main <iframe id=frame sandbox=allow-scripts src=iframe.html></iframe>");
  frameResource.complete("");
  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
  frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
  compositeFrame();
  EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());

  LocalFrame* localFrame = toLocalFrame(frameElement->contentFrame());
  localFrame->script().executeScriptInMainWorld(
      "document.body.innerHTML = 'throttled'");
  EXPECT_FALSE(compositor().needsBeginFrame());

  // The dumped contents should not include the throttled frame.
  DocumentLifecycle::AllowThrottlingScope throttlingScope(
      document().lifecycle());
  WebString result = WebFrameContentDumper::deprecatedDumpFrameTreeAsText(
      webView().mainFrameImpl(), 1024);
  EXPECT_NE(std::string::npos, result.utf8().find("main"));
  EXPECT_EQ(std::string::npos, result.utf8().find("throttled"));
}

TEST_P(FrameThrottlingTest, PaintingViaContentLayerDelegateIsThrottled) {
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
  frameElement->setAttribute(styleAttr,
                             "transform: translateY(480px) translateZ(0px)");
  EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
  compositeFrame();
  EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());

  // If painting of the iframe is throttled, we should only receive two
  // drawing items.
  MockWebDisplayItemList displayItems;
  EXPECT_CALL(displayItems, appendDrawingItem(_, _)).Times(2);

  GraphicsLayer* layer = webView().rootGraphicsLayer();
  paintRecursively(layer, &displayItems);
}

TEST_P(FrameThrottlingTest, ThrottleSubtreeAtomically) {
  // Create two nested frames which are throttled.
  SimRequest mainResource("https://example.com/", "text/html");
  SimRequest frameResource("https://example.com/iframe.html", "text/html");
  SimRequest childFrameResource("https://example.com/child-iframe.html",
                                "text/html");

  loadURL("https://example.com/");
  mainResource.complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frameResource.complete(
      "<iframe id=child-frame sandbox src=child-iframe.html></iframe>");
  childFrameResource.complete("");

  // Move both frames offscreen, but don't run the intersection observers yet.
  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
  auto* childFrameElement = toHTMLIFrameElement(
      frameElement->contentDocument()->getElementById("child-frame"));
  frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
  compositor().beginFrame();
  EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
  EXPECT_FALSE(
      childFrameElement->contentDocument()->view()->canThrottleRendering());

  // Only run the intersection observer for the parent frame. Both frames
  // should immediately become throttled. This simulates the case where a task
  // such as BeginMainFrame runs in the middle of dispatching intersection
  // observer notifications.
  frameElement->contentDocument()
      ->view()
      ->updateRenderThrottlingStatusForTesting();
  EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());
  EXPECT_TRUE(
      childFrameElement->contentDocument()->view()->canThrottleRendering());

  // Both frames should still be throttled after the second notification.
  childFrameElement->contentDocument()
      ->view()
      ->updateRenderThrottlingStatusForTesting();
  EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());
  EXPECT_TRUE(
      childFrameElement->contentDocument()->view()->canThrottleRendering());

  // Move the frame back on screen but don't update throttling yet.
  frameElement->setAttribute(styleAttr, "transform: translateY(0px)");
  compositor().beginFrame();
  EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());
  EXPECT_TRUE(
      childFrameElement->contentDocument()->view()->canThrottleRendering());

  // Update throttling for the child. It should remain throttled because the
  // parent is still throttled.
  childFrameElement->contentDocument()
      ->view()
      ->updateRenderThrottlingStatusForTesting();
  EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());
  EXPECT_TRUE(
      childFrameElement->contentDocument()->view()->canThrottleRendering());

  // Updating throttling on the parent should unthrottle both frames.
  frameElement->contentDocument()
      ->view()
      ->updateRenderThrottlingStatusForTesting();
  EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
  EXPECT_FALSE(
      childFrameElement->contentDocument()->view()->canThrottleRendering());
}

TEST_P(FrameThrottlingTest, SkipPaintingLayersInThrottledFrames) {
  webView().settings()->setAcceleratedCompositingEnabled(true);
  webView().settings()->setPreferCompositingToLCDTextEnabled(true);

  SimRequest mainResource("https://example.com/", "text/html");
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  loadURL("https://example.com/");
  mainResource.complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frameResource.complete(
      "<div id=div style='transform: translateZ(0); background: "
      "red'>layer</div>");
  auto displayItems = compositeFrame();
  EXPECT_TRUE(displayItems.contains(SimCanvas::Rect, "red"));

  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
  frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
  compositeFrame();
  EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());

  auto* frameDocument = frameElement->contentDocument();
  EXPECT_EQ(DocumentLifecycle::PaintClean, frameDocument->lifecycle().state());

  // Simulate the paint for a graphics layer being externally invalidated
  // (e.g., by video playback).
  frameDocument->view()
      ->layoutViewItem()
      .invalidatePaintForViewAndCompositedLayers();

  // The layer inside the throttled frame should not get painted.
  auto displayItems2 = compositeFrame();
  EXPECT_FALSE(displayItems2.contains(SimCanvas::Rect, "red"));
}

TEST_P(FrameThrottlingTest, SynchronousLayoutInAnimationFrameCallback) {
  webView().settings()->setJavaScriptEnabled(true);

  // Prepare a page with two cross origin frames (from the same origin so they
  // are able to access eachother).
  SimRequest mainResource("https://example.com/", "text/html");
  SimRequest firstFrameResource("https://thirdparty.com/first.html",
                                "text/html");
  SimRequest secondFrameResource("https://thirdparty.com/second.html",
                                 "text/html");
  loadURL("https://example.com/");
  mainResource.complete(
      "<iframe id=first name=first "
      "src='https://thirdparty.com/first.html'></iframe>\n"
      "<iframe id=second name=second "
      "src='https://thirdparty.com/second.html'></iframe>");

  // The first frame contains just a simple div. This frame will be made
  // throttled.
  firstFrameResource.complete("<div id=d>first frame</div>");

  // The second frame just used to execute a requestAnimationFrame callback.
  secondFrameResource.complete("");

  // Throttle the first frame.
  auto* firstFrameElement =
      toHTMLIFrameElement(document().getElementById("first"));
  firstFrameElement->setAttribute(styleAttr, "transform: translateY(480px)");
  compositeFrame();
  EXPECT_TRUE(
      firstFrameElement->contentDocument()->view()->canThrottleRendering());

  // Run a animation frame callback in the second frame which mutates the
  // contents of the first frame and causes a synchronous style update. This
  // should not result in an unexpected lifecycle state even if the first
  // frame is throttled during the animation frame callback.
  auto* secondFrameElement =
      toHTMLIFrameElement(document().getElementById("second"));
  LocalFrame* localFrame = toLocalFrame(secondFrameElement->contentFrame());
  localFrame->script().executeScriptInMainWorld(
      "window.requestAnimationFrame(function() {\n"
      "  var throttledFrame = window.parent.frames.first;\n"
      "  throttledFrame.document.documentElement.style = 'margin: 50px';\n"
      "  throttledFrame.document.querySelector('#d').getBoundingClientRect();\n"
      "});\n");
  compositeFrame();
}

TEST_P(FrameThrottlingTest, AllowOneAnimationFrame) {
  webView().settings()->setJavaScriptEnabled(true);

  // Prepare a page with two cross origin frames (from the same origin so they
  // are able to access eachother).
  SimRequest mainResource("https://example.com/", "text/html");
  SimRequest frameResource("https://thirdparty.com/frame.html", "text/html");
  loadURL("https://example.com/");
  mainResource.complete(
      "<iframe id=frame style=\"position: fixed; top: -10000px\" "
      "src='https://thirdparty.com/frame.html'></iframe>");

  frameResource.complete(
      "<script>"
      "window.requestAnimationFrame(() => { window.didRaf = true; });"
      "</script>");

  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
  compositeFrame();
  EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());

  LocalFrame* localFrame = toLocalFrame(frameElement->contentFrame());
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Local<v8::Value> result =
      localFrame->script().executeScriptInMainWorldAndReturnValue(
          ScriptSourceCode("window.didRaf;"));
  EXPECT_TRUE(result->IsTrue());
}

TEST_P(FrameThrottlingTest, UpdatePaintPropertiesOnUnthrottling) {
  if (!RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
    return;

  SimRequest mainResource("https://example.com/", "text/html");
  SimRequest frameResource("https://example.com/iframe.html", "text/html");

  loadURL("https://example.com/");
  mainResource.complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frameResource.complete("<div id='div'>Inner</div>");
  compositeFrame();

  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
  auto* frameDocument = frameElement->contentDocument();
  auto* innerDiv = frameDocument->getElementById("div");
  auto* innerDivObject = innerDiv->layoutObject();
  EXPECT_FALSE(frameDocument->view()->shouldThrottleRendering());

  frameElement->setAttribute(HTMLNames::styleAttr,
                             "transform: translateY(1000px)");
  compositeFrame();
  EXPECT_TRUE(frameDocument->view()->canThrottleRendering());
  EXPECT_FALSE(innerDivObject->paintProperties()->transform());

  // Mutating the throttled frame should not cause paint property update.
  innerDiv->setAttribute(HTMLNames::styleAttr, "transform: translateY(20px)");
  EXPECT_FALSE(compositor().needsBeginFrame());
  EXPECT_TRUE(frameDocument->view()->canThrottleRendering());
  {
    DocumentLifecycle::AllowThrottlingScope throttlingScope(
        document().lifecycle());
    document().view()->updateAllLifecyclePhases();
  }
  EXPECT_FALSE(innerDivObject->paintProperties()->transform());

  // Move the frame back on screen to unthrottle it.
  frameElement->setAttribute(HTMLNames::styleAttr, "");
  // The first update unthrottles the frame, the second actually update layout
  // and paint properties etc.
  compositeFrame();
  compositeFrame();
  EXPECT_FALSE(frameDocument->view()->canThrottleRendering());
  EXPECT_EQ(TransformationMatrix().translate(0, 20),
            innerDiv->layoutObject()->paintProperties()->transform()->matrix());
}

TEST_P(FrameThrottlingTest, DisplayNoneNotThrottled) {
  SimRequest mainResource("https://example.com/", "text/html");

  loadURL("https://example.com/");
  mainResource.complete(
      "<style>iframe { transform: translateY(480px); }</style>"
      "<iframe sandbox id=frame></iframe>");

  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
  auto* frameDocument = frameElement->contentDocument();

  // Initially the frame is throttled as it is offscreen.
  compositeFrame();
  EXPECT_TRUE(frameDocument->view()->canThrottleRendering());

  // Setting display:none unthrottles the frame.
  frameElement->setAttribute(styleAttr, "display: none");
  compositeFrame();
  EXPECT_FALSE(frameDocument->view()->canThrottleRendering());
}

TEST_P(FrameThrottlingTest, DisplayNoneChildrenRemainThrottled) {
  // Create two nested frames which are throttled.
  SimRequest mainResource("https://example.com/", "text/html");
  SimRequest frameResource("https://example.com/iframe.html", "text/html");
  SimRequest childFrameResource("https://example.com/child-iframe.html",
                                "text/html");

  loadURL("https://example.com/");
  mainResource.complete("<iframe id=frame sandbox src=iframe.html></iframe>");
  frameResource.complete(
      "<iframe id=child-frame sandbox src=child-iframe.html></iframe>");
  childFrameResource.complete("");

  // Move both frames offscreen to make them throttled.
  auto* frameElement = toHTMLIFrameElement(document().getElementById("frame"));
  auto* childFrameElement = toHTMLIFrameElement(
      frameElement->contentDocument()->getElementById("child-frame"));
  frameElement->setAttribute(styleAttr, "transform: translateY(480px)");
  compositeFrame();
  EXPECT_TRUE(frameElement->contentDocument()->view()->canThrottleRendering());
  EXPECT_TRUE(
      childFrameElement->contentDocument()->view()->canThrottleRendering());

  // Setting display:none for the parent frame unthrottles the parent but not
  // the child. This behavior matches Safari.
  frameElement->setAttribute(styleAttr, "display: none");
  compositeFrame();
  EXPECT_FALSE(frameElement->contentDocument()->view()->canThrottleRendering());
  EXPECT_TRUE(
      childFrameElement->contentDocument()->view()->canThrottleRendering());
}

}  // namespace blink
