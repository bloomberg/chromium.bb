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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/page/scrolling/ScrollingCoordinator.h"

#include "core/css/CSSStyleSheet.h"
#include "core/css/StyleSheetList.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/page/Page.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCache.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebLayerPositionConstraint.h"
#include "public/platform/WebLayerTreeView.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/web/WebSettings.h"
#include "public/web/WebViewClient.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebViewImpl.h"
#include "web/tests/FrameTestHelpers.h"

namespace blink {

class ScrollingCoordinatorTest : public ::testing::Test,
                                 public ::testing::WithParamInterface<bool>,
                                 private ScopedRootLayerScrollingForTest {
 public:
  ScrollingCoordinatorTest()
      : ScopedRootLayerScrollingForTest(GetParam()),
        m_baseURL("http://www.test.com/") {
    m_helper.initialize(true, nullptr, &m_mockWebViewClient, nullptr,
                        &configureSettings);
    webViewImpl()->resize(IntSize(320, 240));

    // macOS attaches main frame scrollbars to the VisualViewport so the
    // VisualViewport layers need to be initialized.
    webViewImpl()->updateAllLifecyclePhases();
    WebFrameWidgetBase* mainFrameWidget =
        webViewImpl()->mainFrameImpl()->frameWidget();
    mainFrameWidget->setRootGraphicsLayer(webViewImpl()
                                              ->mainFrameImpl()
                                              ->frame()
                                              ->view()
                                              ->layoutViewItem()
                                              .compositor()
                                              ->rootGraphicsLayer());
  }

  ~ScrollingCoordinatorTest() override {
    Platform::current()
        ->getURLLoaderMockFactory()
        ->unregisterAllURLsAndClearMemoryCache();
  }

  void navigateTo(const std::string& url) {
    FrameTestHelpers::loadFrame(webViewImpl()->mainFrame(), url);
  }

  void loadHTML(const std::string& html) {
    FrameTestHelpers::loadHTMLString(webViewImpl()->mainFrame(), html,
                                     URLTestHelpers::toKURL("about:blank"));
  }

  void forceFullCompositingUpdate() {
    webViewImpl()->updateAllLifecyclePhases();
  }

  void registerMockedHttpURLLoad(const std::string& fileName) {
    URLTestHelpers::registerMockedURLLoadFromBase(
        WebString::fromUTF8(m_baseURL), testing::webTestDataPath(),
        WebString::fromUTF8(fileName));
  }

  WebLayer* getRootScrollLayer() {
    GraphicsLayer* layer =
        frame()->view()->layoutViewportScrollableArea()->layerForScrolling();
    return layer ? layer->platformLayer() : nullptr;
  }

  WebViewImpl* webViewImpl() const { return m_helper.webView(); }
  LocalFrame* frame() const {
    return m_helper.webView()->mainFrameImpl()->frame();
  }

  WebLayerTreeView* webLayerTreeView() const {
    return webViewImpl()->layerTreeView();
  }

 protected:
  std::string m_baseURL;
  FrameTestHelpers::TestWebViewClient m_mockWebViewClient;

 private:
  static void configureSettings(WebSettings* settings) {
    settings->setJavaScriptEnabled(true);
    settings->setAcceleratedCompositingEnabled(true);
    settings->setPreferCompositingToLCDTextEnabled(true);
  }

  FrameTestHelpers::WebViewHelper m_helper;
};

INSTANTIATE_TEST_CASE_P(All, ScrollingCoordinatorTest, ::testing::Bool());

TEST_P(ScrollingCoordinatorTest, fastScrollingByDefault) {
  webViewImpl()->resize(WebSize(800, 600));
  loadHTML("<div id='spacer' style='height: 1000px'></div>");
  forceFullCompositingUpdate();

  // Make sure the scrolling coordinator is active.
  FrameView* frameView = frame()->view();
  Page* page = frame()->page();
  ASSERT_TRUE(page->scrollingCoordinator());
  ASSERT_TRUE(page->scrollingCoordinator()->coordinatesScrollingForFrameView(
      frameView));

  // Fast scrolling should be enabled by default.
  WebLayer* rootScrollLayer = getRootScrollLayer();
  ASSERT_TRUE(rootScrollLayer);
  ASSERT_TRUE(rootScrollLayer->scrollable());
  ASSERT_FALSE(rootScrollLayer->shouldScrollOnMainThread());
  ASSERT_EQ(WebEventListenerProperties::Nothing,
            webLayerTreeView()->eventListenerProperties(
                WebEventListenerClass::TouchStartOrMove));
  ASSERT_EQ(WebEventListenerProperties::Nothing,
            webLayerTreeView()->eventListenerProperties(
                WebEventListenerClass::MouseWheel));

  WebLayer* innerViewportScrollLayer =
      page->visualViewport().scrollLayer()->platformLayer();
  ASSERT_TRUE(innerViewportScrollLayer->scrollable());
  ASSERT_FALSE(innerViewportScrollLayer->shouldScrollOnMainThread());
}

TEST_P(ScrollingCoordinatorTest, fastScrollingCanBeDisabledWithSetting) {
  webViewImpl()->resize(WebSize(800, 600));
  loadHTML("<div id='spacer' style='height: 1000px'></div>");
  webViewImpl()->settings()->setThreadedScrollingEnabled(false);
  forceFullCompositingUpdate();

  // Make sure the scrolling coordinator is active.
  FrameView* frameView = frame()->view();
  Page* page = frame()->page();
  ASSERT_TRUE(page->scrollingCoordinator());
  ASSERT_TRUE(page->scrollingCoordinator()->coordinatesScrollingForFrameView(
      frameView));

  // Main scrolling should be enabled with the setting override.
  WebLayer* rootScrollLayer = getRootScrollLayer();
  ASSERT_TRUE(rootScrollLayer);
  ASSERT_TRUE(rootScrollLayer->scrollable());
  ASSERT_TRUE(rootScrollLayer->shouldScrollOnMainThread());

  // Main scrolling should also propagate to inner viewport layer.
  WebLayer* innerViewportScrollLayer =
      page->visualViewport().scrollLayer()->platformLayer();
  ASSERT_TRUE(innerViewportScrollLayer->scrollable());
  ASSERT_TRUE(innerViewportScrollLayer->shouldScrollOnMainThread());
}

TEST_P(ScrollingCoordinatorTest, fastFractionalScrollingDiv) {
  bool origFractionalOffsetsEnabled =
      RuntimeEnabledFeatures::fractionalScrollOffsetsEnabled();
  RuntimeEnabledFeatures::setFractionalScrollOffsetsEnabled(true);

  registerMockedHttpURLLoad("fractional-scroll-div.html");
  navigateTo(m_baseURL + "fractional-scroll-div.html");
  forceFullCompositingUpdate();

  Document* document = frame()->document();
  Element* scrollableElement = document->getElementById("scroller");
  DCHECK(scrollableElement);

  scrollableElement->setScrollTop(1.0);
  scrollableElement->setScrollLeft(1.0);
  forceFullCompositingUpdate();

  // Make sure the fractional scroll offset change 1.0 -> 1.2 gets propagated
  // to compositor.
  scrollableElement->setScrollTop(1.2);
  scrollableElement->setScrollLeft(1.2);
  forceFullCompositingUpdate();

  LayoutObject* layoutObject = scrollableElement->layoutObject();
  ASSERT_TRUE(layoutObject->isBox());
  LayoutBox* box = toLayoutBox(layoutObject);
  ASSERT_TRUE(box->usesCompositedScrolling());
  CompositedLayerMapping* compositedLayerMapping =
      box->layer()->compositedLayerMapping();
  ASSERT_TRUE(compositedLayerMapping->hasScrollingLayer());
  DCHECK(compositedLayerMapping->scrollingContentsLayer());
  WebLayer* webScrollLayer =
      compositedLayerMapping->scrollingContentsLayer()->platformLayer();
  ASSERT_TRUE(webScrollLayer);
  ASSERT_NEAR(1.2f, webScrollLayer->scrollPosition().x, 0.01f);
  ASSERT_NEAR(1.2f, webScrollLayer->scrollPosition().y, 0.01f);

  RuntimeEnabledFeatures::setFractionalScrollOffsetsEnabled(
      origFractionalOffsetsEnabled);
}

static WebLayer* webLayerFromElement(Element* element) {
  if (!element)
    return 0;
  LayoutObject* layoutObject = element->layoutObject();
  if (!layoutObject || !layoutObject->isBoxModelObject())
    return 0;
  PaintLayer* layer = toLayoutBoxModelObject(layoutObject)->layer();
  if (!layer)
    return 0;
  if (!layer->hasCompositedLayerMapping())
    return 0;
  CompositedLayerMapping* compositedLayerMapping =
      layer->compositedLayerMapping();
  GraphicsLayer* graphicsLayer = compositedLayerMapping->mainGraphicsLayer();
  if (!graphicsLayer)
    return 0;
  return graphicsLayer->platformLayer();
}

TEST_P(ScrollingCoordinatorTest, fastScrollingForFixedPosition) {
  registerMockedHttpURLLoad("fixed-position.html");
  navigateTo(m_baseURL + "fixed-position.html");
  forceFullCompositingUpdate();

  // Fixed position should not fall back to main thread scrolling.
  WebLayer* rootScrollLayer = getRootScrollLayer();
  ASSERT_TRUE(rootScrollLayer);
  ASSERT_FALSE(rootScrollLayer->shouldScrollOnMainThread());

  Document* document = frame()->document();
  {
    Element* element = document->getElementById("div-tl");
    ASSERT_TRUE(element);
    WebLayer* layer = webLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->positionConstraint();
    ASSERT_TRUE(constraint.isFixedPosition);
    ASSERT_TRUE(!constraint.isFixedToRightEdge &&
                !constraint.isFixedToBottomEdge);
  }
  {
    Element* element = document->getElementById("div-tr");
    ASSERT_TRUE(element);
    WebLayer* layer = webLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->positionConstraint();
    ASSERT_TRUE(constraint.isFixedPosition);
    ASSERT_TRUE(constraint.isFixedToRightEdge &&
                !constraint.isFixedToBottomEdge);
  }
  {
    Element* element = document->getElementById("div-bl");
    ASSERT_TRUE(element);
    WebLayer* layer = webLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->positionConstraint();
    ASSERT_TRUE(constraint.isFixedPosition);
    ASSERT_TRUE(!constraint.isFixedToRightEdge &&
                constraint.isFixedToBottomEdge);
  }
  {
    Element* element = document->getElementById("div-br");
    ASSERT_TRUE(element);
    WebLayer* layer = webLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->positionConstraint();
    ASSERT_TRUE(constraint.isFixedPosition);
    ASSERT_TRUE(constraint.isFixedToRightEdge &&
                constraint.isFixedToBottomEdge);
  }
  {
    Element* element = document->getElementById("span-tl");
    ASSERT_TRUE(element);
    WebLayer* layer = webLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->positionConstraint();
    ASSERT_TRUE(constraint.isFixedPosition);
    ASSERT_TRUE(!constraint.isFixedToRightEdge &&
                !constraint.isFixedToBottomEdge);
  }
  {
    Element* element = document->getElementById("span-tr");
    ASSERT_TRUE(element);
    WebLayer* layer = webLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->positionConstraint();
    ASSERT_TRUE(constraint.isFixedPosition);
    ASSERT_TRUE(constraint.isFixedToRightEdge &&
                !constraint.isFixedToBottomEdge);
  }
  {
    Element* element = document->getElementById("span-bl");
    ASSERT_TRUE(element);
    WebLayer* layer = webLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->positionConstraint();
    ASSERT_TRUE(constraint.isFixedPosition);
    ASSERT_TRUE(!constraint.isFixedToRightEdge &&
                constraint.isFixedToBottomEdge);
  }
  {
    Element* element = document->getElementById("span-br");
    ASSERT_TRUE(element);
    WebLayer* layer = webLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->positionConstraint();
    ASSERT_TRUE(constraint.isFixedPosition);
    ASSERT_TRUE(constraint.isFixedToRightEdge &&
                constraint.isFixedToBottomEdge);
  }
}

TEST_P(ScrollingCoordinatorTest, fastScrollingForStickyPosition) {
  registerMockedHttpURLLoad("sticky-position.html");
  navigateTo(m_baseURL + "sticky-position.html");
  forceFullCompositingUpdate();

  // Sticky position should not fall back to main thread scrolling.
  WebLayer* rootScrollLayer = getRootScrollLayer();
  ASSERT_TRUE(rootScrollLayer);
  EXPECT_FALSE(rootScrollLayer->shouldScrollOnMainThread());

  Document* document = frame()->document();
  {
    Element* element = document->getElementById("div-tl");
    ASSERT_TRUE(element);
    WebLayer* layer = webLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerStickyPositionConstraint constraint =
        layer->stickyPositionConstraint();
    ASSERT_TRUE(constraint.isSticky);
    EXPECT_TRUE(constraint.isAnchoredTop && constraint.isAnchoredLeft &&
                !constraint.isAnchoredRight && !constraint.isAnchoredBottom);
    EXPECT_EQ(1.f, constraint.topOffset);
    EXPECT_EQ(1.f, constraint.leftOffset);
    EXPECT_EQ(IntRect(100, 100, 10, 10),
              IntRect(constraint.scrollContainerRelativeStickyBoxRect));
    EXPECT_EQ(IntRect(100, 100, 200, 200),
              IntRect(constraint.scrollContainerRelativeContainingBlockRect));
    EXPECT_EQ(IntPoint(100, 100),
              IntPoint(constraint.parentRelativeStickyBoxOffset));
  }
  {
    Element* element = document->getElementById("div-tr");
    ASSERT_TRUE(element);
    WebLayer* layer = webLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerStickyPositionConstraint constraint =
        layer->stickyPositionConstraint();
    ASSERT_TRUE(constraint.isSticky);
    EXPECT_TRUE(constraint.isAnchoredTop && !constraint.isAnchoredLeft &&
                constraint.isAnchoredRight && !constraint.isAnchoredBottom);
  }
  {
    Element* element = document->getElementById("div-bl");
    ASSERT_TRUE(element);
    WebLayer* layer = webLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerStickyPositionConstraint constraint =
        layer->stickyPositionConstraint();
    ASSERT_TRUE(constraint.isSticky);
    EXPECT_TRUE(!constraint.isAnchoredTop && constraint.isAnchoredLeft &&
                !constraint.isAnchoredRight && constraint.isAnchoredBottom);
  }
  {
    Element* element = document->getElementById("div-br");
    ASSERT_TRUE(element);
    WebLayer* layer = webLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerStickyPositionConstraint constraint =
        layer->stickyPositionConstraint();
    ASSERT_TRUE(constraint.isSticky);
    EXPECT_TRUE(!constraint.isAnchoredTop && !constraint.isAnchoredLeft &&
                constraint.isAnchoredRight && constraint.isAnchoredBottom);
  }
  {
    Element* element = document->getElementById("span-tl");
    ASSERT_TRUE(element);
    WebLayer* layer = webLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerStickyPositionConstraint constraint =
        layer->stickyPositionConstraint();
    ASSERT_TRUE(constraint.isSticky);
    EXPECT_TRUE(constraint.isAnchoredTop && constraint.isAnchoredLeft &&
                !constraint.isAnchoredRight && !constraint.isAnchoredBottom);
  }
  {
    Element* element = document->getElementById("span-tlbr");
    ASSERT_TRUE(element);
    WebLayer* layer = webLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerStickyPositionConstraint constraint =
        layer->stickyPositionConstraint();
    ASSERT_TRUE(constraint.isSticky);
    EXPECT_TRUE(constraint.isAnchoredTop && constraint.isAnchoredLeft &&
                constraint.isAnchoredRight && constraint.isAnchoredBottom);
    EXPECT_EQ(1.f, constraint.topOffset);
    EXPECT_EQ(1.f, constraint.leftOffset);
    EXPECT_EQ(1.f, constraint.rightOffset);
    EXPECT_EQ(1.f, constraint.bottomOffset);
  }
  {
    Element* element = document->getElementById("composited-top");
    ASSERT_TRUE(element);
    WebLayer* layer = webLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerStickyPositionConstraint constraint =
        layer->stickyPositionConstraint();
    ASSERT_TRUE(constraint.isSticky);
    EXPECT_TRUE(constraint.isAnchoredTop);
    EXPECT_EQ(IntRect(100, 110, 10, 10),
              IntRect(constraint.scrollContainerRelativeStickyBoxRect));
    EXPECT_EQ(IntRect(100, 100, 200, 200),
              IntRect(constraint.scrollContainerRelativeContainingBlockRect));
    EXPECT_EQ(IntPoint(0, 10),
              IntPoint(constraint.parentRelativeStickyBoxOffset));
  }
}

TEST_P(ScrollingCoordinatorTest, touchEventHandler) {
  registerMockedHttpURLLoad("touch-event-handler.html");
  navigateTo(m_baseURL + "touch-event-handler.html");
  forceFullCompositingUpdate();

  ASSERT_EQ(WebEventListenerProperties::Blocking,
            webLayerTreeView()->eventListenerProperties(
                WebEventListenerClass::TouchStartOrMove));
}

TEST_P(ScrollingCoordinatorTest, touchEventHandlerPassive) {
  registerMockedHttpURLLoad("touch-event-handler-passive.html");
  navigateTo(m_baseURL + "touch-event-handler-passive.html");
  forceFullCompositingUpdate();

  ASSERT_EQ(WebEventListenerProperties::Passive,
            webLayerTreeView()->eventListenerProperties(
                WebEventListenerClass::TouchStartOrMove));
}

TEST_P(ScrollingCoordinatorTest, touchEventHandlerBoth) {
  registerMockedHttpURLLoad("touch-event-handler-both.html");
  navigateTo(m_baseURL + "touch-event-handler-both.html");
  forceFullCompositingUpdate();

  ASSERT_EQ(WebEventListenerProperties::BlockingAndPassive,
            webLayerTreeView()->eventListenerProperties(
                WebEventListenerClass::TouchStartOrMove));
}

TEST_P(ScrollingCoordinatorTest, wheelEventHandler) {
  registerMockedHttpURLLoad("wheel-event-handler.html");
  navigateTo(m_baseURL + "wheel-event-handler.html");
  forceFullCompositingUpdate();

  ASSERT_EQ(WebEventListenerProperties::Blocking,
            webLayerTreeView()->eventListenerProperties(
                WebEventListenerClass::MouseWheel));
}

TEST_P(ScrollingCoordinatorTest, wheelEventHandlerPassive) {
  registerMockedHttpURLLoad("wheel-event-handler-passive.html");
  navigateTo(m_baseURL + "wheel-event-handler-passive.html");
  forceFullCompositingUpdate();

  ASSERT_EQ(WebEventListenerProperties::Passive,
            webLayerTreeView()->eventListenerProperties(
                WebEventListenerClass::MouseWheel));
}

TEST_P(ScrollingCoordinatorTest, wheelEventHandlerBoth) {
  registerMockedHttpURLLoad("wheel-event-handler-both.html");
  navigateTo(m_baseURL + "wheel-event-handler-both.html");
  forceFullCompositingUpdate();

  ASSERT_EQ(WebEventListenerProperties::BlockingAndPassive,
            webLayerTreeView()->eventListenerProperties(
                WebEventListenerClass::MouseWheel));
}

TEST_P(ScrollingCoordinatorTest, scrollEventHandler) {
  registerMockedHttpURLLoad("scroll-event-handler.html");
  navigateTo(m_baseURL + "scroll-event-handler.html");
  forceFullCompositingUpdate();

  ASSERT_TRUE(webLayerTreeView()->haveScrollEventHandlers());
}

TEST_P(ScrollingCoordinatorTest, updateEventHandlersDuringTeardown) {
  registerMockedHttpURLLoad("scroll-event-handler-window.html");
  navigateTo(m_baseURL + "scroll-event-handler-window.html");
  forceFullCompositingUpdate();

  // Simulate detaching the document from its DOM window. This should not
  // cause a crash when the WebViewImpl is closed by the test runner.
  frame()->document()->shutdown();
}

TEST_P(ScrollingCoordinatorTest, clippedBodyTest) {
  registerMockedHttpURLLoad("clipped-body.html");
  navigateTo(m_baseURL + "clipped-body.html");
  forceFullCompositingUpdate();

  WebLayer* rootScrollLayer = getRootScrollLayer();
  ASSERT_TRUE(rootScrollLayer);
  ASSERT_EQ(0u, rootScrollLayer->nonFastScrollableRegion().size());
}

TEST_P(ScrollingCoordinatorTest, overflowScrolling) {
  registerMockedHttpURLLoad("overflow-scrolling.html");
  navigateTo(m_baseURL + "overflow-scrolling.html");
  forceFullCompositingUpdate();

  // Verify the properties of the accelerated scrolling element starting from
  // the LayoutObject all the way to the WebLayer.
  Element* scrollableElement =
      frame()->document()->getElementById("scrollable");
  DCHECK(scrollableElement);

  LayoutObject* layoutObject = scrollableElement->layoutObject();
  ASSERT_TRUE(layoutObject->isBox());
  ASSERT_TRUE(layoutObject->hasLayer());

  LayoutBox* box = toLayoutBox(layoutObject);
  ASSERT_TRUE(box->usesCompositedScrolling());
  ASSERT_EQ(PaintsIntoOwnBacking, box->layer()->compositingState());

  CompositedLayerMapping* compositedLayerMapping =
      box->layer()->compositedLayerMapping();
  ASSERT_TRUE(compositedLayerMapping->hasScrollingLayer());
  DCHECK(compositedLayerMapping->scrollingContentsLayer());

  GraphicsLayer* graphicsLayer =
      compositedLayerMapping->scrollingContentsLayer();
  ASSERT_EQ(box->layer()->getScrollableArea(),
            graphicsLayer->getScrollableArea());

  WebLayer* webScrollLayer =
      compositedLayerMapping->scrollingContentsLayer()->platformLayer();
  ASSERT_TRUE(webScrollLayer->scrollable());
  ASSERT_TRUE(webScrollLayer->userScrollableHorizontal());
  ASSERT_TRUE(webScrollLayer->userScrollableVertical());

#if OS(ANDROID)
  // Now verify we've attached impl-side scrollbars onto the scrollbar layers
  ASSERT_TRUE(compositedLayerMapping->layerForHorizontalScrollbar());
  ASSERT_TRUE(compositedLayerMapping->layerForHorizontalScrollbar()
                  ->hasContentsLayer());
  ASSERT_TRUE(compositedLayerMapping->layerForVerticalScrollbar());
  ASSERT_TRUE(
      compositedLayerMapping->layerForVerticalScrollbar()->hasContentsLayer());
#endif
}

TEST_P(ScrollingCoordinatorTest, overflowHidden) {
  registerMockedHttpURLLoad("overflow-hidden.html");
  navigateTo(m_baseURL + "overflow-hidden.html");
  forceFullCompositingUpdate();

  // Verify the properties of the accelerated scrolling element starting from
  // the LayoutObject all the way to the WebLayer.
  Element* overflowElement =
      frame()->document()->getElementById("unscrollable-y");
  DCHECK(overflowElement);

  LayoutObject* layoutObject = overflowElement->layoutObject();
  ASSERT_TRUE(layoutObject->isBox());
  ASSERT_TRUE(layoutObject->hasLayer());

  LayoutBox* box = toLayoutBox(layoutObject);
  ASSERT_TRUE(box->usesCompositedScrolling());
  ASSERT_EQ(PaintsIntoOwnBacking, box->layer()->compositingState());

  CompositedLayerMapping* compositedLayerMapping =
      box->layer()->compositedLayerMapping();
  ASSERT_TRUE(compositedLayerMapping->hasScrollingLayer());
  DCHECK(compositedLayerMapping->scrollingContentsLayer());

  GraphicsLayer* graphicsLayer =
      compositedLayerMapping->scrollingContentsLayer();
  ASSERT_EQ(box->layer()->getScrollableArea(),
            graphicsLayer->getScrollableArea());

  WebLayer* webScrollLayer =
      compositedLayerMapping->scrollingContentsLayer()->platformLayer();
  ASSERT_TRUE(webScrollLayer->scrollable());
  ASSERT_TRUE(webScrollLayer->userScrollableHorizontal());
  ASSERT_FALSE(webScrollLayer->userScrollableVertical());

  overflowElement = frame()->document()->getElementById("unscrollable-x");
  DCHECK(overflowElement);

  layoutObject = overflowElement->layoutObject();
  ASSERT_TRUE(layoutObject->isBox());
  ASSERT_TRUE(layoutObject->hasLayer());

  box = toLayoutBox(layoutObject);
  ASSERT_TRUE(box->getScrollableArea()->usesCompositedScrolling());
  ASSERT_EQ(PaintsIntoOwnBacking, box->layer()->compositingState());

  compositedLayerMapping = box->layer()->compositedLayerMapping();
  ASSERT_TRUE(compositedLayerMapping->hasScrollingLayer());
  DCHECK(compositedLayerMapping->scrollingContentsLayer());

  graphicsLayer = compositedLayerMapping->scrollingContentsLayer();
  ASSERT_EQ(box->layer()->getScrollableArea(),
            graphicsLayer->getScrollableArea());

  webScrollLayer =
      compositedLayerMapping->scrollingContentsLayer()->platformLayer();
  ASSERT_TRUE(webScrollLayer->scrollable());
  ASSERT_FALSE(webScrollLayer->userScrollableHorizontal());
  ASSERT_TRUE(webScrollLayer->userScrollableVertical());
}

TEST_P(ScrollingCoordinatorTest, iframeScrolling) {
  registerMockedHttpURLLoad("iframe-scrolling.html");
  registerMockedHttpURLLoad("iframe-scrolling-inner.html");
  navigateTo(m_baseURL + "iframe-scrolling.html");
  forceFullCompositingUpdate();

  // Verify the properties of the accelerated scrolling element starting from
  // the LayoutObject all the way to the WebLayer.
  Element* scrollableFrame = frame()->document()->getElementById("scrollable");
  ASSERT_TRUE(scrollableFrame);

  LayoutObject* layoutObject = scrollableFrame->layoutObject();
  ASSERT_TRUE(layoutObject);
  ASSERT_TRUE(layoutObject->isLayoutPart());

  LayoutPart* layoutPart = toLayoutPart(layoutObject);
  ASSERT_TRUE(layoutPart);
  ASSERT_TRUE(layoutPart->frameViewBase());
  ASSERT_TRUE(layoutPart->frameViewBase()->isFrameView());

  FrameView* innerFrameView = toFrameView(layoutPart->frameViewBase());
  LayoutViewItem innerLayoutViewItem = innerFrameView->layoutViewItem();
  ASSERT_FALSE(innerLayoutViewItem.isNull());

  PaintLayerCompositor* innerCompositor = innerLayoutViewItem.compositor();
  ASSERT_TRUE(innerCompositor->inCompositingMode());

  GraphicsLayer* scrollLayer =
      innerFrameView->layoutViewportScrollableArea()->layerForScrolling();
  ASSERT_TRUE(scrollLayer);
  ASSERT_EQ(innerFrameView->layoutViewportScrollableArea(),
            scrollLayer->getScrollableArea());

  WebLayer* webScrollLayer = scrollLayer->platformLayer();
  ASSERT_TRUE(webScrollLayer->scrollable());

#if OS(ANDROID)
  // Now verify we've attached impl-side scrollbars onto the scrollbar layers
  GraphicsLayer* horizontalScrollbarLayer =
      innerFrameView->layoutViewportScrollableArea()
          ->layerForHorizontalScrollbar();
  ASSERT_TRUE(horizontalScrollbarLayer);
  ASSERT_TRUE(horizontalScrollbarLayer->hasContentsLayer());
  GraphicsLayer* verticalScrollbarLayer =
      innerFrameView->layoutViewportScrollableArea()
          ->layerForVerticalScrollbar();
  ASSERT_TRUE(verticalScrollbarLayer);
  ASSERT_TRUE(verticalScrollbarLayer->hasContentsLayer());
#endif
}

TEST_P(ScrollingCoordinatorTest, rtlIframe) {
  registerMockedHttpURLLoad("rtl-iframe.html");
  registerMockedHttpURLLoad("rtl-iframe-inner.html");
  navigateTo(m_baseURL + "rtl-iframe.html");
  forceFullCompositingUpdate();

  // Verify the properties of the accelerated scrolling element starting from
  // the LayoutObject all the way to the WebLayer.
  Element* scrollableFrame = frame()->document()->getElementById("scrollable");
  ASSERT_TRUE(scrollableFrame);

  LayoutObject* layoutObject = scrollableFrame->layoutObject();
  ASSERT_TRUE(layoutObject);
  ASSERT_TRUE(layoutObject->isLayoutPart());

  LayoutPart* layoutPart = toLayoutPart(layoutObject);
  ASSERT_TRUE(layoutPart);
  ASSERT_TRUE(layoutPart->frameViewBase());
  ASSERT_TRUE(layoutPart->frameViewBase()->isFrameView());

  FrameView* innerFrameView = toFrameView(layoutPart->frameViewBase());
  LayoutViewItem innerLayoutViewItem = innerFrameView->layoutViewItem();
  ASSERT_FALSE(innerLayoutViewItem.isNull());

  PaintLayerCompositor* innerCompositor = innerLayoutViewItem.compositor();
  ASSERT_TRUE(innerCompositor->inCompositingMode());

  GraphicsLayer* scrollLayer =
      innerFrameView->layoutViewportScrollableArea()->layerForScrolling();
  ASSERT_TRUE(scrollLayer);
  ASSERT_EQ(innerFrameView->layoutViewportScrollableArea(),
            scrollLayer->getScrollableArea());

  WebLayer* webScrollLayer = scrollLayer->platformLayer();
  ASSERT_TRUE(webScrollLayer->scrollable());

  int expectedScrollPosition =
      958 + (innerFrameView->layoutViewportScrollableArea()
                     ->verticalScrollbar()
                     ->isOverlayScrollbar()
                 ? 0
                 : 15);
  ASSERT_EQ(expectedScrollPosition, webScrollLayer->scrollPosition().x);
}

TEST_P(ScrollingCoordinatorTest, setupScrollbarLayerShouldNotCrash) {
  registerMockedHttpURLLoad("setup_scrollbar_layer_crash.html");
  navigateTo(m_baseURL + "setup_scrollbar_layer_crash.html");
  forceFullCompositingUpdate();
  // This test document setup an iframe with scrollbars, then switch to
  // an empty document by javascript.
}

TEST_P(ScrollingCoordinatorTest,
       scrollbarsForceMainThreadOrHaveWebScrollbarLayer) {
  registerMockedHttpURLLoad("trivial-scroller.html");
  navigateTo(m_baseURL + "trivial-scroller.html");
  forceFullCompositingUpdate();

  Document* document = frame()->document();
  Element* scrollableElement = document->getElementById("scroller");
  DCHECK(scrollableElement);

  LayoutObject* layoutObject = scrollableElement->layoutObject();
  ASSERT_TRUE(layoutObject->isBox());
  LayoutBox* box = toLayoutBox(layoutObject);
  ASSERT_TRUE(box->usesCompositedScrolling());
  CompositedLayerMapping* compositedLayerMapping =
      box->layer()->compositedLayerMapping();
  GraphicsLayer* scrollbarGraphicsLayer =
      compositedLayerMapping->layerForVerticalScrollbar();
  ASSERT_TRUE(scrollbarGraphicsLayer);

  bool hasWebScrollbarLayer = !scrollbarGraphicsLayer->drawsContent();
  ASSERT_TRUE(
      hasWebScrollbarLayer ||
      scrollbarGraphicsLayer->platformLayer()->shouldScrollOnMainThread());
}

#if OS(MACOSX) || OS(ANDROID)
TEST_P(ScrollingCoordinatorTest,
       DISABLED_setupScrollbarLayerShouldSetScrollLayerOpaque)
#else
TEST_P(ScrollingCoordinatorTest, setupScrollbarLayerShouldSetScrollLayerOpaque)
#endif
{
  registerMockedHttpURLLoad("wide_document.html");
  navigateTo(m_baseURL + "wide_document.html");
  forceFullCompositingUpdate();

  FrameView* frameView = frame()->view();
  ASSERT_TRUE(frameView);

  GraphicsLayer* scrollbarGraphicsLayer =
      frameView->layoutViewportScrollableArea()->layerForHorizontalScrollbar();
  ASSERT_TRUE(scrollbarGraphicsLayer);

  WebLayer* platformLayer = scrollbarGraphicsLayer->platformLayer();
  ASSERT_TRUE(platformLayer);

  WebLayer* contentsLayer = scrollbarGraphicsLayer->contentsLayer();
  ASSERT_TRUE(contentsLayer);

  // After scrollableAreaScrollbarLayerDidChange,
  // if the main frame's scrollbarLayer is opaque,
  // contentsLayer should be opaque too.
  ASSERT_EQ(platformLayer->opaque(), contentsLayer->opaque());
}

TEST_P(ScrollingCoordinatorTest,
       FixedPositionLosingBackingShouldTriggerMainThreadScroll) {
  webViewImpl()->settings()->setPreferCompositingToLCDTextEnabled(false);
  registerMockedHttpURLLoad("fixed-position-losing-backing.html");
  navigateTo(m_baseURL + "fixed-position-losing-backing.html");
  forceFullCompositingUpdate();

  WebLayer* scrollLayer = getRootScrollLayer();
  ASSERT_TRUE(scrollLayer);

  Document* document = frame()->document();
  Element* fixedPos = document->getElementById("fixed");

  EXPECT_TRUE(static_cast<LayoutBoxModelObject*>(fixedPos->layoutObject())
                  ->layer()
                  ->hasCompositedLayerMapping());
  EXPECT_FALSE(scrollLayer->shouldScrollOnMainThread());

  fixedPos->setInlineStyleProperty(CSSPropertyTransform, CSSValueNone);
  forceFullCompositingUpdate();

  EXPECT_FALSE(static_cast<LayoutBoxModelObject*>(fixedPos->layoutObject())
                   ->layer()
                   ->hasCompositedLayerMapping());
  EXPECT_TRUE(scrollLayer->shouldScrollOnMainThread());
}

TEST_P(ScrollingCoordinatorTest, CustomScrollbarShouldTriggerMainThreadScroll) {
  webViewImpl()->settings()->setPreferCompositingToLCDTextEnabled(true);
  webViewImpl()->setDeviceScaleFactor(2.f);
  registerMockedHttpURLLoad("custom_scrollbar.html");
  navigateTo(m_baseURL + "custom_scrollbar.html");
  forceFullCompositingUpdate();

  Document* document = frame()->document();
  Element* container = document->getElementById("container");
  Element* content = document->getElementById("content");
  DCHECK_EQ(container->getAttribute(HTMLNames::classAttr), "custom_scrollbar");
  DCHECK(container);
  DCHECK(content);

  LayoutObject* layoutObject = container->layoutObject();
  ASSERT_TRUE(layoutObject->isBox());
  LayoutBox* box = toLayoutBox(layoutObject);
  ASSERT_TRUE(box->usesCompositedScrolling());
  CompositedLayerMapping* compositedLayerMapping =
      box->layer()->compositedLayerMapping();
  GraphicsLayer* scrollbarGraphicsLayer =
      compositedLayerMapping->layerForVerticalScrollbar();
  ASSERT_TRUE(scrollbarGraphicsLayer);
  ASSERT_TRUE(
      scrollbarGraphicsLayer->platformLayer()->shouldScrollOnMainThread());
  ASSERT_TRUE(
      scrollbarGraphicsLayer->platformLayer()->mainThreadScrollingReasons() &
      MainThreadScrollingReason::kCustomScrollbarScrolling);

  // remove custom scrollbar class, the scrollbar is expected to scroll on
  // impl thread as it is an overlay scrollbar.
  container->removeAttribute("class");
  forceFullCompositingUpdate();
  scrollbarGraphicsLayer = compositedLayerMapping->layerForVerticalScrollbar();
  ASSERT_FALSE(
      scrollbarGraphicsLayer->platformLayer()->shouldScrollOnMainThread());
  ASSERT_FALSE(
      scrollbarGraphicsLayer->platformLayer()->mainThreadScrollingReasons() &
      MainThreadScrollingReason::kCustomScrollbarScrolling);
}

TEST_P(ScrollingCoordinatorTest,
       BackgroundAttachmentFixedShouldTriggerMainThreadScroll) {
  registerMockedHttpURLLoad("iframe-background-attachment-fixed.html");
  registerMockedHttpURLLoad("iframe-background-attachment-fixed-inner.html");
  registerMockedHttpURLLoad("white-1x1.png");
  navigateTo(m_baseURL + "iframe-background-attachment-fixed.html");
  forceFullCompositingUpdate();

  Element* iframe = frame()->document()->getElementById("iframe");
  ASSERT_TRUE(iframe);

  LayoutObject* layoutObject = iframe->layoutObject();
  ASSERT_TRUE(layoutObject);
  ASSERT_TRUE(layoutObject->isLayoutPart());

  LayoutPart* layoutPart = toLayoutPart(layoutObject);
  ASSERT_TRUE(layoutPart);
  ASSERT_TRUE(layoutPart->frameViewBase());
  ASSERT_TRUE(layoutPart->frameViewBase()->isFrameView());

  FrameView* innerFrameView = toFrameView(layoutPart->frameViewBase());
  LayoutViewItem innerLayoutViewItem = innerFrameView->layoutViewItem();
  ASSERT_FALSE(innerLayoutViewItem.isNull());

  PaintLayerCompositor* innerCompositor = innerLayoutViewItem.compositor();
  ASSERT_TRUE(innerCompositor->inCompositingMode());

  GraphicsLayer* scrollLayer =
      innerFrameView->layoutViewportScrollableArea()->layerForScrolling();
  ASSERT_TRUE(scrollLayer);
  ASSERT_EQ(innerFrameView->layoutViewportScrollableArea(),
            scrollLayer->getScrollableArea());

  WebLayer* webScrollLayer = scrollLayer->platformLayer();
  ASSERT_TRUE(webScrollLayer->scrollable());
  ASSERT_TRUE(webScrollLayer->mainThreadScrollingReasons() &
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);

  // Remove fixed background-attachment should make the iframe
  // scroll on cc.
  auto* iframeDoc = toHTMLIFrameElement(iframe)->contentDocument();
  iframe = iframeDoc->getElementById("scrollable");
  ASSERT_TRUE(iframe);

  iframe->removeAttribute("class");
  forceFullCompositingUpdate();

  layoutObject = iframe->layoutObject();
  ASSERT_TRUE(layoutObject);

  scrollLayer = layoutObject->frameView()
                    ->layoutViewportScrollableArea()
                    ->layerForScrolling();
  ASSERT_TRUE(scrollLayer);

  webScrollLayer = scrollLayer->platformLayer();
  ASSERT_TRUE(webScrollLayer->scrollable());
  ASSERT_FALSE(webScrollLayer->mainThreadScrollingReasons() &
               MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);

  // Force main frame to scroll on main thread. All its descendants
  // should scroll on main thread as well.
  Element* element = frame()->document()->getElementById("scrollable");
  element->setAttribute(
      "style",
      "background-image: url('white-1x1.png'); background-attachment: fixed;",
      ASSERT_NO_EXCEPTION);

  forceFullCompositingUpdate();

  layoutObject = iframe->layoutObject();
  ASSERT_TRUE(layoutObject);

  scrollLayer = layoutObject->frameView()
                    ->layoutViewportScrollableArea()
                    ->layerForScrolling();
  ASSERT_TRUE(scrollLayer);

  webScrollLayer = scrollLayer->platformLayer();
  ASSERT_TRUE(webScrollLayer->scrollable());
  ASSERT_TRUE(webScrollLayer->mainThreadScrollingReasons() &
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
}

// Upon resizing the content size, the main thread scrolling reason
// kHasNonLayerViewportConstrainedObject should be updated on all frames
TEST_P(ScrollingCoordinatorTest,
       RecalculateMainThreadScrollingReasonsUponResize) {
  webViewImpl()->settings()->setPreferCompositingToLCDTextEnabled(false);
  registerMockedHttpURLLoad("has-non-layer-viewport-constrained-objects.html");
  navigateTo(m_baseURL + "has-non-layer-viewport-constrained-objects.html");
  forceFullCompositingUpdate();

  LOG(ERROR) << frame()->view()->mainThreadScrollingReasons();
  Element* element = frame()->document()->getElementById("scrollable");
  ASSERT_TRUE(element);

  LayoutObject* layoutObject = element->layoutObject();
  ASSERT_TRUE(layoutObject);

  GraphicsLayer* scrollLayer = layoutObject->frameView()
                                   ->layoutViewportScrollableArea()
                                   ->layerForScrolling();
  WebLayer* webScrollLayer;

  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    // When RLS is enabled, the LayoutView won't have a scrolling contents
    // because it does not overflow.
    ASSERT_FALSE(scrollLayer);
  } else {
    ASSERT_TRUE(scrollLayer);
    webScrollLayer = scrollLayer->platformLayer();
    ASSERT_TRUE(webScrollLayer->scrollable());
    ASSERT_FALSE(
        webScrollLayer->mainThreadScrollingReasons() &
        MainThreadScrollingReason::kHasNonLayerViewportConstrainedObjects);
  }

  // When the div becomes to scrollable it should scroll on main thread
  element->setAttribute("style",
                        "overflow:scroll;height:2000px;will-change:transform;",
                        ASSERT_NO_EXCEPTION);
  forceFullCompositingUpdate();

  layoutObject = element->layoutObject();
  ASSERT_TRUE(layoutObject);

  scrollLayer = layoutObject->frameView()
                    ->layoutViewportScrollableArea()
                    ->layerForScrolling();
  ASSERT_TRUE(scrollLayer);

  webScrollLayer = scrollLayer->platformLayer();
  ASSERT_TRUE(webScrollLayer->scrollable());
  ASSERT_TRUE(
      webScrollLayer->mainThreadScrollingReasons() &
      MainThreadScrollingReason::kHasNonLayerViewportConstrainedObjects);

  // The main thread scrolling reason should be reset upon the following change
  element->setAttribute("style",
                        "overflow:scroll;height:200px;will-change:transform;",
                        ASSERT_NO_EXCEPTION);
  forceFullCompositingUpdate();

  layoutObject = element->layoutObject();
  ASSERT_TRUE(layoutObject);

  scrollLayer = layoutObject->frameView()
                    ->layoutViewportScrollableArea()
                    ->layerForScrolling();
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    // When RLS is enabled, the LayoutView won't have a scrolling contents
    // because it does not overflow.
    ASSERT_FALSE(scrollLayer);
  } else {
    ASSERT_TRUE(scrollLayer);
    webScrollLayer = scrollLayer->platformLayer();
    ASSERT_TRUE(webScrollLayer->scrollable());
    ASSERT_FALSE(
        webScrollLayer->mainThreadScrollingReasons() &
        MainThreadScrollingReason::kHasNonLayerViewportConstrainedObjects);
  }
}

class StyleRelatedMainThreadScrollingReasonTest
    : public ScrollingCoordinatorTest {
  static const uint32_t m_LCDTextRelatedReasons =
      MainThreadScrollingReason::kHasOpacityAndLCDText |
      MainThreadScrollingReason::kHasTransformAndLCDText |
      MainThreadScrollingReason::kBackgroundNotOpaqueInRectAndLCDText;

 protected:
  StyleRelatedMainThreadScrollingReasonTest() {
    registerMockedHttpURLLoad("two_scrollable_area.html");
    navigateTo(m_baseURL + "two_scrollable_area.html");
  }
  void testStyle(const std::string& target, const uint32_t reason) {
    webViewImpl()->settings()->setPreferCompositingToLCDTextEnabled(false);
    Document* document = frame()->document();
    Element* container = document->getElementById("scroller1");
    container->setAttribute("class", target.c_str(), ASSERT_NO_EXCEPTION);
    container = document->getElementById("scroller2");
    container->setAttribute("class", target.c_str(), ASSERT_NO_EXCEPTION);
    forceFullCompositingUpdate();

    FrameView* frameView = frame()->view();
    ASSERT_TRUE(frameView);
    ASSERT_TRUE(frameView->mainThreadScrollingReasons() & reason);

    // Remove the target attribute from one of the scrollers.
    // Still need to scroll on main thread.
    container = document->getElementById("scroller1");
    DCHECK(container);

    container->removeAttribute("class");
    forceFullCompositingUpdate();

    ASSERT_TRUE(frameView->mainThreadScrollingReasons() & reason);

    // Remove attribute from the other scroller would lead to
    // scroll on impl.
    container = document->getElementById("scroller2");
    DCHECK(container);

    container->removeAttribute("class");
    forceFullCompositingUpdate();

    ASSERT_FALSE(frameView->mainThreadScrollingReasons() & reason);

    // Add target attribute would again lead to scroll on main thread
    container->setAttribute("class", target.c_str(), ASSERT_NO_EXCEPTION);
    forceFullCompositingUpdate();

    ASSERT_TRUE(frameView->mainThreadScrollingReasons() & reason);

    if ((reason & m_LCDTextRelatedReasons) &&
        !(reason & ~m_LCDTextRelatedReasons)) {
      webViewImpl()->settings()->setPreferCompositingToLCDTextEnabled(true);
      forceFullCompositingUpdate();
      ASSERT_FALSE(frameView->mainThreadScrollingReasons());
    }
  }
};

INSTANTIATE_TEST_CASE_P(All,
                        StyleRelatedMainThreadScrollingReasonTest,
                        ::testing::Bool());

// TODO(yigu): This test and all other style realted main thread scrolling
// reason tests below have been disabled due to https://crbug.com/701355.
TEST_P(StyleRelatedMainThreadScrollingReasonTest, DISABLED_TransparentTest) {
  testStyle("transparent", MainThreadScrollingReason::kHasOpacityAndLCDText);
}

TEST_P(StyleRelatedMainThreadScrollingReasonTest, DISABLED_TransformTest) {
  testStyle("transform", MainThreadScrollingReason::kHasTransformAndLCDText);
}

TEST_P(StyleRelatedMainThreadScrollingReasonTest,
       DISABLED_BackgroundNotOpaqueTest) {
  testStyle("background-not-opaque",
            MainThreadScrollingReason::kBackgroundNotOpaqueInRectAndLCDText);
}

TEST_P(StyleRelatedMainThreadScrollingReasonTest, DISABLED_BorderRadiusTest) {
  testStyle("border-radius", MainThreadScrollingReason::kHasBorderRadius);
}

TEST_P(StyleRelatedMainThreadScrollingReasonTest, DISABLED_ClipTest) {
  testStyle("clip", MainThreadScrollingReason::kHasClipRelatedProperty);
}

TEST_P(StyleRelatedMainThreadScrollingReasonTest, DISABLED_ClipPathTest) {
  uint32_t reason = MainThreadScrollingReason::kHasClipRelatedProperty;
  webViewImpl()->settings()->setPreferCompositingToLCDTextEnabled(false);
  Document* document = frame()->document();
  // Test ancestor with ClipPath
  Element* element = document->body();
  DCHECK(element);
  element->setAttribute(HTMLNames::styleAttr,
                        "clip-path:circle(115px at 20px 20px);");
  forceFullCompositingUpdate();

  FrameView* frameView = frame()->view();
  ASSERT_TRUE(frameView);
  ASSERT_TRUE(frameView->mainThreadScrollingReasons() & reason);

  // Remove clip path from ancestor.
  element->removeAttribute(HTMLNames::styleAttr);
  forceFullCompositingUpdate();

  ASSERT_FALSE(frameView->mainThreadScrollingReasons() & reason);

  // Test descendant with ClipPath
  element = document->getElementById("content1");
  DCHECK(element);
  element->setAttribute(HTMLNames::styleAttr,
                        "clip-path:circle(115px at 20px 20px);");
  forceFullCompositingUpdate();
  ASSERT_TRUE(frameView->mainThreadScrollingReasons() & reason);

  // Remove clip path from descendant.
  element->removeAttribute(HTMLNames::styleAttr);
  forceFullCompositingUpdate();
  ASSERT_FALSE(frameView->mainThreadScrollingReasons() & reason);
}

TEST_P(StyleRelatedMainThreadScrollingReasonTest, DISABLED_LCDTextEnabledTest) {
  testStyle("transparent border-radius",
            MainThreadScrollingReason::kHasOpacityAndLCDText |
                MainThreadScrollingReason::kHasBorderRadius);
}

TEST_P(StyleRelatedMainThreadScrollingReasonTest, DISABLED_BoxShadowTest) {
  testStyle("box-shadow",
            MainThreadScrollingReason::kHasBoxShadowFromNonRootLayer);
}

}  // namespace blink
