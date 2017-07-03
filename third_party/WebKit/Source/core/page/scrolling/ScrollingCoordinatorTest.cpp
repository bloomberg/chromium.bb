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
#include "core/exported/WebViewBase.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/VisualViewport.h"
#include "core/frame/WebLocalFrameBase.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/layout/LayoutEmbeddedContent.h"
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

namespace blink {

class ScrollingCoordinatorTest : public ::testing::Test,
                                 public ::testing::WithParamInterface<bool>,
                                 private ScopedRootLayerScrollingForTest {
 public:
  ScrollingCoordinatorTest()
      : ScopedRootLayerScrollingForTest(GetParam()),
        base_url_("http://www.test.com/") {
    helper_.Initialize(nullptr, nullptr, nullptr, &ConfigureSettings);
    GetWebView()->Resize(IntSize(320, 240));

    // macOS attaches main frame scrollbars to the VisualViewport so the
    // VisualViewport layers need to be initialized.
    GetWebView()->UpdateAllLifecyclePhases();
    WebFrameWidgetBase* main_frame_widget =
        GetWebView()->MainFrameImpl()->FrameWidget();
    main_frame_widget->SetRootGraphicsLayer(GetWebView()
                                                ->MainFrameImpl()
                                                ->GetFrame()
                                                ->View()
                                                ->GetLayoutViewItem()
                                                .Compositor()
                                                ->RootGraphicsLayer());
  }

  ~ScrollingCoordinatorTest() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  void NavigateTo(const std::string& url) {
    FrameTestHelpers::LoadFrame(GetWebView()->MainFrameImpl(), url);
  }

  void LoadHTML(const std::string& html) {
    FrameTestHelpers::LoadHTMLString(GetWebView()->MainFrameImpl(), html,
                                     URLTestHelpers::ToKURL("about:blank"));
  }

  void ForceFullCompositingUpdate() {
    GetWebView()->UpdateAllLifecyclePhases();
  }

  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    URLTestHelpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), testing::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  WebLayer* GetRootScrollLayer() {
    GraphicsLayer* layer =
        GetFrame()->View()->LayoutViewportScrollableArea()->LayerForScrolling();
    return layer ? layer->PlatformLayer() : nullptr;
  }

  WebViewBase* GetWebView() const { return helper_.WebView(); }
  LocalFrame* GetFrame() const { return helper_.LocalMainFrame()->GetFrame(); }

  WebLayerTreeView* GetWebLayerTreeView() const {
    return GetWebView()->LayerTreeView();
  }

 protected:
  std::string base_url_;

 private:
  static void ConfigureSettings(WebSettings* settings) {
    settings->SetAcceleratedCompositingEnabled(true);
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }

  FrameTestHelpers::WebViewHelper helper_;
};

INSTANTIATE_TEST_CASE_P(All, ScrollingCoordinatorTest, ::testing::Bool());

TEST_P(ScrollingCoordinatorTest, fastScrollingByDefault) {
  GetWebView()->Resize(WebSize(800, 600));
  LoadHTML("<div id='spacer' style='height: 1000px'></div>");
  ForceFullCompositingUpdate();

  // Make sure the scrolling coordinator is active.
  LocalFrameView* frame_view = GetFrame()->View();
  Page* page = GetFrame()->GetPage();
  ASSERT_TRUE(page->GetScrollingCoordinator());
  ASSERT_TRUE(page->GetScrollingCoordinator()->CoordinatesScrollingForFrameView(
      frame_view));

  // Fast scrolling should be enabled by default.
  WebLayer* root_scroll_layer = GetRootScrollLayer();
  ASSERT_TRUE(root_scroll_layer);
  ASSERT_TRUE(root_scroll_layer->Scrollable());
  ASSERT_FALSE(root_scroll_layer->ShouldScrollOnMainThread());
  ASSERT_EQ(WebEventListenerProperties::kNothing,
            GetWebLayerTreeView()->EventListenerProperties(
                WebEventListenerClass::kTouchStartOrMove));
  ASSERT_EQ(WebEventListenerProperties::kNothing,
            GetWebLayerTreeView()->EventListenerProperties(
                WebEventListenerClass::kMouseWheel));

  WebLayer* inner_viewport_scroll_layer =
      page->GetVisualViewport().ScrollLayer()->PlatformLayer();
  ASSERT_TRUE(inner_viewport_scroll_layer->Scrollable());
  ASSERT_FALSE(inner_viewport_scroll_layer->ShouldScrollOnMainThread());
}

TEST_P(ScrollingCoordinatorTest, fastScrollingCanBeDisabledWithSetting) {
  GetWebView()->Resize(WebSize(800, 600));
  LoadHTML("<div id='spacer' style='height: 1000px'></div>");
  GetWebView()->GetSettings()->SetThreadedScrollingEnabled(false);
  ForceFullCompositingUpdate();

  // Make sure the scrolling coordinator is active.
  LocalFrameView* frame_view = GetFrame()->View();
  Page* page = GetFrame()->GetPage();
  ASSERT_TRUE(page->GetScrollingCoordinator());
  ASSERT_TRUE(page->GetScrollingCoordinator()->CoordinatesScrollingForFrameView(
      frame_view));

  // Main scrolling should be enabled with the setting override.
  WebLayer* root_scroll_layer = GetRootScrollLayer();
  ASSERT_TRUE(root_scroll_layer);
  ASSERT_TRUE(root_scroll_layer->Scrollable());
  ASSERT_TRUE(root_scroll_layer->ShouldScrollOnMainThread());

  // Main scrolling should also propagate to inner viewport layer.
  WebLayer* inner_viewport_scroll_layer =
      page->GetVisualViewport().ScrollLayer()->PlatformLayer();
  ASSERT_TRUE(inner_viewport_scroll_layer->Scrollable());
  ASSERT_TRUE(inner_viewport_scroll_layer->ShouldScrollOnMainThread());
}

TEST_P(ScrollingCoordinatorTest, fastFractionalScrollingDiv) {
  bool orig_fractional_offsets_enabled =
      RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled();
  RuntimeEnabledFeatures::SetFractionalScrollOffsetsEnabled(true);

  RegisterMockedHttpURLLoad("fractional-scroll-div.html");
  NavigateTo(base_url_ + "fractional-scroll-div.html");
  ForceFullCompositingUpdate();

  Document* document = GetFrame()->GetDocument();
  Element* scrollable_element = document->getElementById("scroller");
  DCHECK(scrollable_element);

  scrollable_element->setScrollTop(1.0);
  scrollable_element->setScrollLeft(1.0);
  ForceFullCompositingUpdate();

  // Make sure the fractional scroll offset change 1.0 -> 1.2 gets propagated
  // to compositor.
  scrollable_element->setScrollTop(1.2);
  scrollable_element->setScrollLeft(1.2);
  ForceFullCompositingUpdate();

  LayoutObject* layout_object = scrollable_element->GetLayoutObject();
  ASSERT_TRUE(layout_object->IsBox());
  LayoutBox* box = ToLayoutBox(layout_object);
  ASSERT_TRUE(box->UsesCompositedScrolling());
  CompositedLayerMapping* composited_layer_mapping =
      box->Layer()->GetCompositedLayerMapping();
  ASSERT_TRUE(composited_layer_mapping->HasScrollingLayer());
  DCHECK(composited_layer_mapping->ScrollingContentsLayer());
  WebLayer* web_scroll_layer =
      composited_layer_mapping->ScrollingContentsLayer()->PlatformLayer();
  ASSERT_TRUE(web_scroll_layer);
  ASSERT_NEAR(1.2f, web_scroll_layer->ScrollPosition().x, 0.01f);
  ASSERT_NEAR(1.2f, web_scroll_layer->ScrollPosition().y, 0.01f);

  RuntimeEnabledFeatures::SetFractionalScrollOffsetsEnabled(
      orig_fractional_offsets_enabled);
}

static WebLayer* WebLayerFromElement(Element* element) {
  if (!element)
    return 0;
  LayoutObject* layout_object = element->GetLayoutObject();
  if (!layout_object || !layout_object->IsBoxModelObject())
    return 0;
  PaintLayer* layer = ToLayoutBoxModelObject(layout_object)->Layer();
  if (!layer)
    return 0;
  if (!layer->HasCompositedLayerMapping())
    return 0;
  CompositedLayerMapping* composited_layer_mapping =
      layer->GetCompositedLayerMapping();
  GraphicsLayer* graphics_layer = composited_layer_mapping->MainGraphicsLayer();
  if (!graphics_layer)
    return 0;
  return graphics_layer->PlatformLayer();
}

TEST_P(ScrollingCoordinatorTest, fastScrollingForFixedPosition) {
  RegisterMockedHttpURLLoad("fixed-position.html");
  NavigateTo(base_url_ + "fixed-position.html");
  ForceFullCompositingUpdate();

  // Fixed position should not fall back to main thread scrolling.
  WebLayer* root_scroll_layer = GetRootScrollLayer();
  ASSERT_TRUE(root_scroll_layer);
  ASSERT_FALSE(root_scroll_layer->ShouldScrollOnMainThread());

  Document* document = GetFrame()->GetDocument();
  {
    Element* element = document->getElementById("div-tl");
    ASSERT_TRUE(element);
    WebLayer* layer = WebLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->PositionConstraint();
    ASSERT_TRUE(constraint.is_fixed_position);
    ASSERT_TRUE(!constraint.is_fixed_to_right_edge &&
                !constraint.is_fixed_to_bottom_edge);
  }
  {
    Element* element = document->getElementById("div-tr");
    ASSERT_TRUE(element);
    WebLayer* layer = WebLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->PositionConstraint();
    ASSERT_TRUE(constraint.is_fixed_position);
    ASSERT_TRUE(constraint.is_fixed_to_right_edge &&
                !constraint.is_fixed_to_bottom_edge);
  }
  {
    Element* element = document->getElementById("div-bl");
    ASSERT_TRUE(element);
    WebLayer* layer = WebLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->PositionConstraint();
    ASSERT_TRUE(constraint.is_fixed_position);
    ASSERT_TRUE(!constraint.is_fixed_to_right_edge &&
                constraint.is_fixed_to_bottom_edge);
  }
  {
    Element* element = document->getElementById("div-br");
    ASSERT_TRUE(element);
    WebLayer* layer = WebLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->PositionConstraint();
    ASSERT_TRUE(constraint.is_fixed_position);
    ASSERT_TRUE(constraint.is_fixed_to_right_edge &&
                constraint.is_fixed_to_bottom_edge);
  }
  {
    Element* element = document->getElementById("span-tl");
    ASSERT_TRUE(element);
    WebLayer* layer = WebLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->PositionConstraint();
    ASSERT_TRUE(constraint.is_fixed_position);
    ASSERT_TRUE(!constraint.is_fixed_to_right_edge &&
                !constraint.is_fixed_to_bottom_edge);
  }
  {
    Element* element = document->getElementById("span-tr");
    ASSERT_TRUE(element);
    WebLayer* layer = WebLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->PositionConstraint();
    ASSERT_TRUE(constraint.is_fixed_position);
    ASSERT_TRUE(constraint.is_fixed_to_right_edge &&
                !constraint.is_fixed_to_bottom_edge);
  }
  {
    Element* element = document->getElementById("span-bl");
    ASSERT_TRUE(element);
    WebLayer* layer = WebLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->PositionConstraint();
    ASSERT_TRUE(constraint.is_fixed_position);
    ASSERT_TRUE(!constraint.is_fixed_to_right_edge &&
                constraint.is_fixed_to_bottom_edge);
  }
  {
    Element* element = document->getElementById("span-br");
    ASSERT_TRUE(element);
    WebLayer* layer = WebLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerPositionConstraint constraint = layer->PositionConstraint();
    ASSERT_TRUE(constraint.is_fixed_position);
    ASSERT_TRUE(constraint.is_fixed_to_right_edge &&
                constraint.is_fixed_to_bottom_edge);
  }
}

TEST_P(ScrollingCoordinatorTest, fastScrollingForStickyPosition) {
  RegisterMockedHttpURLLoad("sticky-position.html");
  NavigateTo(base_url_ + "sticky-position.html");
  ForceFullCompositingUpdate();

  // Sticky position should not fall back to main thread scrolling.
  WebLayer* root_scroll_layer = GetRootScrollLayer();
  ASSERT_TRUE(root_scroll_layer);
  EXPECT_FALSE(root_scroll_layer->ShouldScrollOnMainThread());

  Document* document = GetFrame()->GetDocument();
  {
    Element* element = document->getElementById("div-tl");
    ASSERT_TRUE(element);
    WebLayer* layer = WebLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerStickyPositionConstraint constraint =
        layer->StickyPositionConstraint();
    ASSERT_TRUE(constraint.is_sticky);
    EXPECT_TRUE(constraint.is_anchored_top && constraint.is_anchored_left &&
                !constraint.is_anchored_right &&
                !constraint.is_anchored_bottom);
    EXPECT_EQ(1.f, constraint.top_offset);
    EXPECT_EQ(1.f, constraint.left_offset);
    EXPECT_EQ(IntRect(100, 100, 10, 10),
              IntRect(constraint.scroll_container_relative_sticky_box_rect));
    EXPECT_EQ(
        IntRect(100, 100, 200, 200),
        IntRect(constraint.scroll_container_relative_containing_block_rect));
  }
  {
    Element* element = document->getElementById("div-tr");
    ASSERT_TRUE(element);
    WebLayer* layer = WebLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerStickyPositionConstraint constraint =
        layer->StickyPositionConstraint();
    ASSERT_TRUE(constraint.is_sticky);
    EXPECT_TRUE(constraint.is_anchored_top && !constraint.is_anchored_left &&
                constraint.is_anchored_right && !constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById("div-bl");
    ASSERT_TRUE(element);
    WebLayer* layer = WebLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerStickyPositionConstraint constraint =
        layer->StickyPositionConstraint();
    ASSERT_TRUE(constraint.is_sticky);
    EXPECT_TRUE(!constraint.is_anchored_top && constraint.is_anchored_left &&
                !constraint.is_anchored_right && constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById("div-br");
    ASSERT_TRUE(element);
    WebLayer* layer = WebLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerStickyPositionConstraint constraint =
        layer->StickyPositionConstraint();
    ASSERT_TRUE(constraint.is_sticky);
    EXPECT_TRUE(!constraint.is_anchored_top && !constraint.is_anchored_left &&
                constraint.is_anchored_right && constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById("span-tl");
    ASSERT_TRUE(element);
    WebLayer* layer = WebLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerStickyPositionConstraint constraint =
        layer->StickyPositionConstraint();
    ASSERT_TRUE(constraint.is_sticky);
    EXPECT_TRUE(constraint.is_anchored_top && constraint.is_anchored_left &&
                !constraint.is_anchored_right &&
                !constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById("span-tlbr");
    ASSERT_TRUE(element);
    WebLayer* layer = WebLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerStickyPositionConstraint constraint =
        layer->StickyPositionConstraint();
    ASSERT_TRUE(constraint.is_sticky);
    EXPECT_TRUE(constraint.is_anchored_top && constraint.is_anchored_left &&
                constraint.is_anchored_right && constraint.is_anchored_bottom);
    EXPECT_EQ(1.f, constraint.top_offset);
    EXPECT_EQ(1.f, constraint.left_offset);
    EXPECT_EQ(1.f, constraint.right_offset);
    EXPECT_EQ(1.f, constraint.bottom_offset);
  }
  {
    Element* element = document->getElementById("composited-top");
    ASSERT_TRUE(element);
    WebLayer* layer = WebLayerFromElement(element);
    ASSERT_TRUE(layer);
    WebLayerStickyPositionConstraint constraint =
        layer->StickyPositionConstraint();
    ASSERT_TRUE(constraint.is_sticky);
    EXPECT_TRUE(constraint.is_anchored_top);
    EXPECT_EQ(IntRect(100, 110, 10, 10),
              IntRect(constraint.scroll_container_relative_sticky_box_rect));
    EXPECT_EQ(
        IntRect(100, 100, 200, 200),
        IntRect(constraint.scroll_container_relative_containing_block_rect));
  }
}

TEST_P(ScrollingCoordinatorTest, touchEventHandler) {
  RegisterMockedHttpURLLoad("touch-event-handler.html");
  NavigateTo(base_url_ + "touch-event-handler.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(WebEventListenerProperties::kBlocking,
            GetWebLayerTreeView()->EventListenerProperties(
                WebEventListenerClass::kTouchStartOrMove));
}

TEST_P(ScrollingCoordinatorTest, touchEventHandlerPassive) {
  RegisterMockedHttpURLLoad("touch-event-handler-passive.html");
  NavigateTo(base_url_ + "touch-event-handler-passive.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(WebEventListenerProperties::kPassive,
            GetWebLayerTreeView()->EventListenerProperties(
                WebEventListenerClass::kTouchStartOrMove));
}

TEST_P(ScrollingCoordinatorTest, touchEventHandlerBoth) {
  RegisterMockedHttpURLLoad("touch-event-handler-both.html");
  NavigateTo(base_url_ + "touch-event-handler-both.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(WebEventListenerProperties::kBlockingAndPassive,
            GetWebLayerTreeView()->EventListenerProperties(
                WebEventListenerClass::kTouchStartOrMove));
}

TEST_P(ScrollingCoordinatorTest, wheelEventHandler) {
  RegisterMockedHttpURLLoad("wheel-event-handler.html");
  NavigateTo(base_url_ + "wheel-event-handler.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(WebEventListenerProperties::kBlocking,
            GetWebLayerTreeView()->EventListenerProperties(
                WebEventListenerClass::kMouseWheel));
}

TEST_P(ScrollingCoordinatorTest, wheelEventHandlerPassive) {
  RegisterMockedHttpURLLoad("wheel-event-handler-passive.html");
  NavigateTo(base_url_ + "wheel-event-handler-passive.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(WebEventListenerProperties::kPassive,
            GetWebLayerTreeView()->EventListenerProperties(
                WebEventListenerClass::kMouseWheel));
}

TEST_P(ScrollingCoordinatorTest, wheelEventHandlerBoth) {
  RegisterMockedHttpURLLoad("wheel-event-handler-both.html");
  NavigateTo(base_url_ + "wheel-event-handler-both.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(WebEventListenerProperties::kBlockingAndPassive,
            GetWebLayerTreeView()->EventListenerProperties(
                WebEventListenerClass::kMouseWheel));
}

TEST_P(ScrollingCoordinatorTest, scrollEventHandler) {
  RegisterMockedHttpURLLoad("scroll-event-handler.html");
  NavigateTo(base_url_ + "scroll-event-handler.html");
  ForceFullCompositingUpdate();

  ASSERT_TRUE(GetWebLayerTreeView()->HaveScrollEventHandlers());
}

TEST_P(ScrollingCoordinatorTest, updateEventHandlersDuringTeardown) {
  RegisterMockedHttpURLLoad("scroll-event-handler-window.html");
  NavigateTo(base_url_ + "scroll-event-handler-window.html");
  ForceFullCompositingUpdate();

  // Simulate detaching the document from its DOM window. This should not
  // cause a crash when the WebViewImpl is closed by the test runner.
  GetFrame()->GetDocument()->Shutdown();
}

TEST_P(ScrollingCoordinatorTest, clippedBodyTest) {
  RegisterMockedHttpURLLoad("clipped-body.html");
  NavigateTo(base_url_ + "clipped-body.html");
  ForceFullCompositingUpdate();

  WebLayer* root_scroll_layer = GetRootScrollLayer();
  ASSERT_TRUE(root_scroll_layer);
  ASSERT_EQ(0u, root_scroll_layer->NonFastScrollableRegion().size());
}

TEST_P(ScrollingCoordinatorTest, overflowScrolling) {
  RegisterMockedHttpURLLoad("overflow-scrolling.html");
  NavigateTo(base_url_ + "overflow-scrolling.html");
  ForceFullCompositingUpdate();

  // Verify the properties of the accelerated scrolling element starting from
  // the LayoutObject all the way to the WebLayer.
  Element* scrollable_element =
      GetFrame()->GetDocument()->getElementById("scrollable");
  DCHECK(scrollable_element);

  LayoutObject* layout_object = scrollable_element->GetLayoutObject();
  ASSERT_TRUE(layout_object->IsBox());
  ASSERT_TRUE(layout_object->HasLayer());

  LayoutBox* box = ToLayoutBox(layout_object);
  ASSERT_TRUE(box->UsesCompositedScrolling());
  ASSERT_EQ(kPaintsIntoOwnBacking, box->Layer()->GetCompositingState());

  CompositedLayerMapping* composited_layer_mapping =
      box->Layer()->GetCompositedLayerMapping();
  ASSERT_TRUE(composited_layer_mapping->HasScrollingLayer());
  DCHECK(composited_layer_mapping->ScrollingContentsLayer());

  GraphicsLayer* graphics_layer =
      composited_layer_mapping->ScrollingContentsLayer();
  ASSERT_EQ(box->Layer()->GetScrollableArea(),
            graphics_layer->GetScrollableArea());

  WebLayer* web_scroll_layer =
      composited_layer_mapping->ScrollingContentsLayer()->PlatformLayer();
  ASSERT_TRUE(web_scroll_layer->Scrollable());
  ASSERT_TRUE(web_scroll_layer->UserScrollableHorizontal());
  ASSERT_TRUE(web_scroll_layer->UserScrollableVertical());

#if OS(ANDROID)
  // Now verify we've attached impl-side scrollbars onto the scrollbar layers
  ASSERT_TRUE(composited_layer_mapping->LayerForHorizontalScrollbar());
  ASSERT_TRUE(composited_layer_mapping->LayerForHorizontalScrollbar()
                  ->HasContentsLayer());
  ASSERT_TRUE(composited_layer_mapping->LayerForVerticalScrollbar());
  ASSERT_TRUE(composited_layer_mapping->LayerForVerticalScrollbar()
                  ->HasContentsLayer());
#endif
}

TEST_P(ScrollingCoordinatorTest, overflowHidden) {
  RegisterMockedHttpURLLoad("overflow-hidden.html");
  NavigateTo(base_url_ + "overflow-hidden.html");
  ForceFullCompositingUpdate();

  // Verify the properties of the accelerated scrolling element starting from
  // the LayoutObject all the way to the WebLayer.
  Element* overflow_element =
      GetFrame()->GetDocument()->getElementById("unscrollable-y");
  DCHECK(overflow_element);

  LayoutObject* layout_object = overflow_element->GetLayoutObject();
  ASSERT_TRUE(layout_object->IsBox());
  ASSERT_TRUE(layout_object->HasLayer());

  LayoutBox* box = ToLayoutBox(layout_object);
  ASSERT_TRUE(box->UsesCompositedScrolling());
  ASSERT_EQ(kPaintsIntoOwnBacking, box->Layer()->GetCompositingState());

  CompositedLayerMapping* composited_layer_mapping =
      box->Layer()->GetCompositedLayerMapping();
  ASSERT_TRUE(composited_layer_mapping->HasScrollingLayer());
  DCHECK(composited_layer_mapping->ScrollingContentsLayer());

  GraphicsLayer* graphics_layer =
      composited_layer_mapping->ScrollingContentsLayer();
  ASSERT_EQ(box->Layer()->GetScrollableArea(),
            graphics_layer->GetScrollableArea());

  WebLayer* web_scroll_layer =
      composited_layer_mapping->ScrollingContentsLayer()->PlatformLayer();
  ASSERT_TRUE(web_scroll_layer->Scrollable());
  ASSERT_TRUE(web_scroll_layer->UserScrollableHorizontal());
  ASSERT_FALSE(web_scroll_layer->UserScrollableVertical());

  overflow_element =
      GetFrame()->GetDocument()->getElementById("unscrollable-x");
  DCHECK(overflow_element);

  layout_object = overflow_element->GetLayoutObject();
  ASSERT_TRUE(layout_object->IsBox());
  ASSERT_TRUE(layout_object->HasLayer());

  box = ToLayoutBox(layout_object);
  ASSERT_TRUE(box->GetScrollableArea()->UsesCompositedScrolling());
  ASSERT_EQ(kPaintsIntoOwnBacking, box->Layer()->GetCompositingState());

  composited_layer_mapping = box->Layer()->GetCompositedLayerMapping();
  ASSERT_TRUE(composited_layer_mapping->HasScrollingLayer());
  DCHECK(composited_layer_mapping->ScrollingContentsLayer());

  graphics_layer = composited_layer_mapping->ScrollingContentsLayer();
  ASSERT_EQ(box->Layer()->GetScrollableArea(),
            graphics_layer->GetScrollableArea());

  web_scroll_layer =
      composited_layer_mapping->ScrollingContentsLayer()->PlatformLayer();
  ASSERT_TRUE(web_scroll_layer->Scrollable());
  ASSERT_FALSE(web_scroll_layer->UserScrollableHorizontal());
  ASSERT_TRUE(web_scroll_layer->UserScrollableVertical());
}

TEST_P(ScrollingCoordinatorTest, iframeScrolling) {
  RegisterMockedHttpURLLoad("iframe-scrolling.html");
  RegisterMockedHttpURLLoad("iframe-scrolling-inner.html");
  NavigateTo(base_url_ + "iframe-scrolling.html");
  ForceFullCompositingUpdate();

  // Verify the properties of the accelerated scrolling element starting from
  // the LayoutObject all the way to the WebLayer.
  Element* scrollable_frame =
      GetFrame()->GetDocument()->getElementById("scrollable");
  ASSERT_TRUE(scrollable_frame);

  LayoutObject* layout_object = scrollable_frame->GetLayoutObject();
  ASSERT_TRUE(layout_object);
  ASSERT_TRUE(layout_object->IsLayoutEmbeddedContent());

  LayoutEmbeddedContent* layout_embedded_content =
      ToLayoutEmbeddedContent(layout_object);
  ASSERT_TRUE(layout_embedded_content);

  LocalFrameView* inner_frame_view = layout_embedded_content->ChildFrameView();
  ASSERT_TRUE(inner_frame_view);

  LayoutViewItem inner_layout_view_item = inner_frame_view->GetLayoutViewItem();
  ASSERT_FALSE(inner_layout_view_item.IsNull());

  PaintLayerCompositor* inner_compositor = inner_layout_view_item.Compositor();
  ASSERT_TRUE(inner_compositor->InCompositingMode());

  GraphicsLayer* scroll_layer =
      inner_frame_view->LayoutViewportScrollableArea()->LayerForScrolling();
  ASSERT_TRUE(scroll_layer);
  ASSERT_EQ(inner_frame_view->LayoutViewportScrollableArea(),
            scroll_layer->GetScrollableArea());

  WebLayer* web_scroll_layer = scroll_layer->PlatformLayer();
  ASSERT_TRUE(web_scroll_layer->Scrollable());

#if OS(ANDROID)
  // Now verify we've attached impl-side scrollbars onto the scrollbar layers
  GraphicsLayer* horizontal_scrollbar_layer =
      inner_frame_view->LayoutViewportScrollableArea()
          ->LayerForHorizontalScrollbar();
  ASSERT_TRUE(horizontal_scrollbar_layer);
  ASSERT_TRUE(horizontal_scrollbar_layer->HasContentsLayer());
  GraphicsLayer* vertical_scrollbar_layer =
      inner_frame_view->LayoutViewportScrollableArea()
          ->LayerForVerticalScrollbar();
  ASSERT_TRUE(vertical_scrollbar_layer);
  ASSERT_TRUE(vertical_scrollbar_layer->HasContentsLayer());
#endif
}

TEST_P(ScrollingCoordinatorTest, rtlIframe) {
  RegisterMockedHttpURLLoad("rtl-iframe.html");
  RegisterMockedHttpURLLoad("rtl-iframe-inner.html");
  NavigateTo(base_url_ + "rtl-iframe.html");
  ForceFullCompositingUpdate();

  // Verify the properties of the accelerated scrolling element starting from
  // the LayoutObject all the way to the WebLayer.
  Element* scrollable_frame =
      GetFrame()->GetDocument()->getElementById("scrollable");
  ASSERT_TRUE(scrollable_frame);

  LayoutObject* layout_object = scrollable_frame->GetLayoutObject();
  ASSERT_TRUE(layout_object);
  ASSERT_TRUE(layout_object->IsLayoutEmbeddedContent());

  LayoutEmbeddedContent* layout_embedded_content =
      ToLayoutEmbeddedContent(layout_object);
  ASSERT_TRUE(layout_embedded_content);

  LocalFrameView* inner_frame_view = layout_embedded_content->ChildFrameView();
  ASSERT_TRUE(inner_frame_view);

  LayoutViewItem inner_layout_view_item = inner_frame_view->GetLayoutViewItem();
  ASSERT_FALSE(inner_layout_view_item.IsNull());

  PaintLayerCompositor* inner_compositor = inner_layout_view_item.Compositor();
  ASSERT_TRUE(inner_compositor->InCompositingMode());

  GraphicsLayer* scroll_layer =
      inner_frame_view->LayoutViewportScrollableArea()->LayerForScrolling();
  ASSERT_TRUE(scroll_layer);
  ASSERT_EQ(inner_frame_view->LayoutViewportScrollableArea(),
            scroll_layer->GetScrollableArea());

  WebLayer* web_scroll_layer = scroll_layer->PlatformLayer();
  ASSERT_TRUE(web_scroll_layer->Scrollable());

  int expected_scroll_position =
      958 + (inner_frame_view->LayoutViewportScrollableArea()
                     ->VerticalScrollbar()
                     ->IsOverlayScrollbar()
                 ? 0
                 : 15);
  ASSERT_EQ(expected_scroll_position, web_scroll_layer->ScrollPosition().x);
}

TEST_P(ScrollingCoordinatorTest, setupScrollbarLayerShouldNotCrash) {
  RegisterMockedHttpURLLoad("setup_scrollbar_layer_crash.html");
  NavigateTo(base_url_ + "setup_scrollbar_layer_crash.html");
  ForceFullCompositingUpdate();
  // This test document setup an iframe with scrollbars, then switch to
  // an empty document by javascript.
}

TEST_P(ScrollingCoordinatorTest,
       scrollbarsForceMainThreadOrHaveWebScrollbarLayer) {
  RegisterMockedHttpURLLoad("trivial-scroller.html");
  NavigateTo(base_url_ + "trivial-scroller.html");
  ForceFullCompositingUpdate();

  Document* document = GetFrame()->GetDocument();
  Element* scrollable_element = document->getElementById("scroller");
  DCHECK(scrollable_element);

  LayoutObject* layout_object = scrollable_element->GetLayoutObject();
  ASSERT_TRUE(layout_object->IsBox());
  LayoutBox* box = ToLayoutBox(layout_object);
  ASSERT_TRUE(box->UsesCompositedScrolling());
  CompositedLayerMapping* composited_layer_mapping =
      box->Layer()->GetCompositedLayerMapping();
  GraphicsLayer* scrollbar_graphics_layer =
      composited_layer_mapping->LayerForVerticalScrollbar();
  ASSERT_TRUE(scrollbar_graphics_layer);

  bool has_web_scrollbar_layer = !scrollbar_graphics_layer->DrawsContent();
  ASSERT_TRUE(
      has_web_scrollbar_layer ||
      scrollbar_graphics_layer->PlatformLayer()->ShouldScrollOnMainThread());
}

#if OS(MACOSX) || OS(ANDROID)
TEST_P(ScrollingCoordinatorTest,
       DISABLED_setupScrollbarLayerShouldSetScrollLayerOpaque)
#else
TEST_P(ScrollingCoordinatorTest, setupScrollbarLayerShouldSetScrollLayerOpaque)
#endif
{
  RegisterMockedHttpURLLoad("wide_document.html");
  NavigateTo(base_url_ + "wide_document.html");
  ForceFullCompositingUpdate();

  LocalFrameView* frame_view = GetFrame()->View();
  ASSERT_TRUE(frame_view);

  GraphicsLayer* scrollbar_graphics_layer =
      frame_view->LayoutViewportScrollableArea()->LayerForHorizontalScrollbar();
  ASSERT_TRUE(scrollbar_graphics_layer);

  WebLayer* platform_layer = scrollbar_graphics_layer->PlatformLayer();
  ASSERT_TRUE(platform_layer);

  WebLayer* contents_layer = scrollbar_graphics_layer->ContentsLayer();
  ASSERT_TRUE(contents_layer);

  // After scrollableAreaScrollbarLayerDidChange,
  // if the main frame's scrollbarLayer is opaque,
  // contentsLayer should be opaque too.
  ASSERT_EQ(platform_layer->Opaque(), contents_layer->Opaque());
}

TEST_P(ScrollingCoordinatorTest,
       FixedPositionLosingBackingShouldTriggerMainThreadScroll) {
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  RegisterMockedHttpURLLoad("fixed-position-losing-backing.html");
  NavigateTo(base_url_ + "fixed-position-losing-backing.html");
  ForceFullCompositingUpdate();

  WebLayer* scroll_layer = GetRootScrollLayer();
  ASSERT_TRUE(scroll_layer);

  Document* document = GetFrame()->GetDocument();
  Element* fixed_pos = document->getElementById("fixed");

  EXPECT_TRUE(static_cast<LayoutBoxModelObject*>(fixed_pos->GetLayoutObject())
                  ->Layer()
                  ->HasCompositedLayerMapping());
  EXPECT_FALSE(scroll_layer->ShouldScrollOnMainThread());

  fixed_pos->SetInlineStyleProperty(CSSPropertyTransform, CSSValueNone);
  ForceFullCompositingUpdate();

  EXPECT_FALSE(static_cast<LayoutBoxModelObject*>(fixed_pos->GetLayoutObject())
                   ->Layer()
                   ->HasCompositedLayerMapping());
  EXPECT_TRUE(scroll_layer->ShouldScrollOnMainThread());
}

TEST_P(ScrollingCoordinatorTest, CustomScrollbarShouldTriggerMainThreadScroll) {
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  GetWebView()->SetDeviceScaleFactor(2.f);
  RegisterMockedHttpURLLoad("custom_scrollbar.html");
  NavigateTo(base_url_ + "custom_scrollbar.html");
  ForceFullCompositingUpdate();

  Document* document = GetFrame()->GetDocument();
  Element* container = document->getElementById("container");
  Element* content = document->getElementById("content");
  DCHECK_EQ(container->getAttribute(HTMLNames::classAttr), "custom_scrollbar");
  DCHECK(container);
  DCHECK(content);

  LayoutObject* layout_object = container->GetLayoutObject();
  ASSERT_TRUE(layout_object->IsBox());
  LayoutBox* box = ToLayoutBox(layout_object);
  ASSERT_TRUE(box->UsesCompositedScrolling());
  CompositedLayerMapping* composited_layer_mapping =
      box->Layer()->GetCompositedLayerMapping();
  GraphicsLayer* scrollbar_graphics_layer =
      composited_layer_mapping->LayerForVerticalScrollbar();
  ASSERT_TRUE(scrollbar_graphics_layer);
  ASSERT_TRUE(
      scrollbar_graphics_layer->PlatformLayer()->ShouldScrollOnMainThread());
  ASSERT_TRUE(
      scrollbar_graphics_layer->PlatformLayer()->MainThreadScrollingReasons() &
      MainThreadScrollingReason::kCustomScrollbarScrolling);

  // remove custom scrollbar class, the scrollbar is expected to scroll on
  // impl thread as it is an overlay scrollbar.
  container->removeAttribute("class");
  ForceFullCompositingUpdate();
  scrollbar_graphics_layer =
      composited_layer_mapping->LayerForVerticalScrollbar();
  ASSERT_FALSE(
      scrollbar_graphics_layer->PlatformLayer()->ShouldScrollOnMainThread());
  ASSERT_FALSE(
      scrollbar_graphics_layer->PlatformLayer()->MainThreadScrollingReasons() &
      MainThreadScrollingReason::kCustomScrollbarScrolling);
}

TEST_P(ScrollingCoordinatorTest,
       BackgroundAttachmentFixedShouldTriggerMainThreadScroll) {
  RegisterMockedHttpURLLoad("iframe-background-attachment-fixed.html");
  RegisterMockedHttpURLLoad("iframe-background-attachment-fixed-inner.html");
  RegisterMockedHttpURLLoad("white-1x1.png");
  NavigateTo(base_url_ + "iframe-background-attachment-fixed.html");
  ForceFullCompositingUpdate();

  Element* iframe = GetFrame()->GetDocument()->getElementById("iframe");
  ASSERT_TRUE(iframe);

  LayoutObject* layout_object = iframe->GetLayoutObject();
  ASSERT_TRUE(layout_object);
  ASSERT_TRUE(layout_object->IsLayoutEmbeddedContent());

  LayoutEmbeddedContent* layout_embedded_content =
      ToLayoutEmbeddedContent(layout_object);
  ASSERT_TRUE(layout_embedded_content);

  LocalFrameView* inner_frame_view = layout_embedded_content->ChildFrameView();
  ASSERT_TRUE(inner_frame_view);

  LayoutViewItem inner_layout_view_item = inner_frame_view->GetLayoutViewItem();
  ASSERT_FALSE(inner_layout_view_item.IsNull());

  PaintLayerCompositor* inner_compositor = inner_layout_view_item.Compositor();
  ASSERT_TRUE(inner_compositor->InCompositingMode());

  GraphicsLayer* scroll_layer =
      inner_frame_view->LayoutViewportScrollableArea()->LayerForScrolling();
  ASSERT_TRUE(scroll_layer);
  ASSERT_EQ(inner_frame_view->LayoutViewportScrollableArea(),
            scroll_layer->GetScrollableArea());

  WebLayer* web_scroll_layer = scroll_layer->PlatformLayer();
  ASSERT_TRUE(web_scroll_layer->Scrollable());
  ASSERT_TRUE(web_scroll_layer->MainThreadScrollingReasons() &
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);

  // Remove fixed background-attachment should make the iframe
  // scroll on cc.
  auto* iframe_doc = toHTMLIFrameElement(iframe)->contentDocument();
  iframe = iframe_doc->getElementById("scrollable");
  ASSERT_TRUE(iframe);

  iframe->removeAttribute("class");
  ForceFullCompositingUpdate();

  layout_object = iframe->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  scroll_layer = layout_object->GetFrameView()
                     ->LayoutViewportScrollableArea()
                     ->LayerForScrolling();
  ASSERT_TRUE(scroll_layer);

  web_scroll_layer = scroll_layer->PlatformLayer();
  ASSERT_TRUE(web_scroll_layer->Scrollable());
  ASSERT_FALSE(web_scroll_layer->MainThreadScrollingReasons() &
               MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);

  // Force main frame to scroll on main thread. All its descendants
  // should scroll on main thread as well.
  Element* element = GetFrame()->GetDocument()->getElementById("scrollable");
  element->setAttribute(
      "style",
      "background-image: url('white-1x1.png'); background-attachment: fixed;",
      ASSERT_NO_EXCEPTION);

  ForceFullCompositingUpdate();

  layout_object = iframe->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  scroll_layer = layout_object->GetFrameView()
                     ->LayoutViewportScrollableArea()
                     ->LayerForScrolling();
  ASSERT_TRUE(scroll_layer);

  web_scroll_layer = scroll_layer->PlatformLayer();
  ASSERT_TRUE(web_scroll_layer->Scrollable());
  ASSERT_TRUE(web_scroll_layer->MainThreadScrollingReasons() &
              MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects);
}

// Upon resizing the content size, the main thread scrolling reason
// kHasNonLayerViewportConstrainedObject should be updated on all frames
TEST_P(ScrollingCoordinatorTest,
       RecalculateMainThreadScrollingReasonsUponResize) {
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  RegisterMockedHttpURLLoad("has-non-layer-viewport-constrained-objects.html");
  NavigateTo(base_url_ + "has-non-layer-viewport-constrained-objects.html");
  ForceFullCompositingUpdate();

  Element* element = GetFrame()->GetDocument()->getElementById("scrollable");
  ASSERT_TRUE(element);

  LayoutObject* layout_object = element->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  GraphicsLayer* scroll_layer = layout_object->GetFrameView()
                                    ->LayoutViewportScrollableArea()
                                    ->LayerForScrolling();
  WebLayer* web_scroll_layer;

  if (!RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    ASSERT_TRUE(scroll_layer);
    web_scroll_layer = scroll_layer->PlatformLayer();
    ASSERT_TRUE(web_scroll_layer->Scrollable());
    ASSERT_FALSE(
        web_scroll_layer->MainThreadScrollingReasons() &
        MainThreadScrollingReason::kHasNonLayerViewportConstrainedObjects);
  }

  // When the div becomes to scrollable it should scroll on main thread
  element->setAttribute("style",
                        "overflow:scroll;height:2000px;will-change:transform;",
                        ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();

  layout_object = element->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  scroll_layer = layout_object->GetFrameView()
                     ->LayoutViewportScrollableArea()
                     ->LayerForScrolling();
  ASSERT_TRUE(scroll_layer);

  web_scroll_layer = scroll_layer->PlatformLayer();
  ASSERT_TRUE(web_scroll_layer->Scrollable());
  ASSERT_TRUE(
      web_scroll_layer->MainThreadScrollingReasons() &
      MainThreadScrollingReason::kHasNonLayerViewportConstrainedObjects);

  // The main thread scrolling reason should be reset upon the following change
  element->setAttribute("style",
                        "overflow:scroll;height:200px;will-change:transform;",
                        ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();

  layout_object = element->GetLayoutObject();
  ASSERT_TRUE(layout_object);

  scroll_layer = layout_object->GetFrameView()
                     ->LayoutViewportScrollableArea()
                     ->LayerForScrolling();
  if (!RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    ASSERT_TRUE(scroll_layer);
    web_scroll_layer = scroll_layer->PlatformLayer();
    ASSERT_TRUE(web_scroll_layer->Scrollable());
    ASSERT_FALSE(
        web_scroll_layer->MainThreadScrollingReasons() &
        MainThreadScrollingReason::kHasNonLayerViewportConstrainedObjects);
  }
}

class NonCompositedMainThreadScrollingReasonTest
    : public ScrollingCoordinatorTest {
  static const uint32_t kLCDTextRelatedReasons =
      MainThreadScrollingReason::kHasOpacityAndLCDText |
      MainThreadScrollingReason::kHasTransformAndLCDText |
      MainThreadScrollingReason::kBackgroundNotOpaqueInRectAndLCDText |
      MainThreadScrollingReason::kIsNotStackingContextAndLCDText;

 protected:
  NonCompositedMainThreadScrollingReasonTest() {
    RegisterMockedHttpURLLoad("two_scrollable_area.html");
    NavigateTo(base_url_ + "two_scrollable_area.html");
  }
  void TestNonCompositedReasons(const std::string& target,
                                const uint32_t reason) {
    GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
    Document* document = GetFrame()->GetDocument();
    Element* container = document->getElementById("scroller1");
    container->setAttribute("class", target.c_str(), ASSERT_NO_EXCEPTION);
    ForceFullCompositingUpdate();

    PaintLayerScrollableArea* scrollable_area =
        ToLayoutBoxModelObject(container->GetLayoutObject())
            ->GetScrollableArea();
    ASSERT_TRUE(scrollable_area);
    EXPECT_TRUE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
                reason);

    Element* container2 = document->getElementById("scroller2");
    PaintLayerScrollableArea* scrollable_area2 =
        ToLayoutBoxModelObject(container2->GetLayoutObject())
            ->GetScrollableArea();
    ASSERT_TRUE(scrollable_area2);
    // Different scrollable area should remain unaffected.
    EXPECT_FALSE(
        scrollable_area2->GetNonCompositedMainThreadScrollingReasons() &
        reason);

    LocalFrameView* frame_view = GetFrame()->View();
    ASSERT_TRUE(frame_view);
    EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons() & reason);

    // Remove attribute from the scroller 1 would lead to scroll on impl.
    container->removeAttribute("class");
    ForceFullCompositingUpdate();

    EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
                 reason);
    EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons() & reason);

    // Add target attribute would again lead to scroll on main thread
    container->setAttribute("class", target.c_str(), ASSERT_NO_EXCEPTION);
    ForceFullCompositingUpdate();

    EXPECT_TRUE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
                reason);
    EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons() & reason);

    if ((reason & kLCDTextRelatedReasons) &&
        !(reason & ~kLCDTextRelatedReasons)) {
      GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
      ForceFullCompositingUpdate();
      EXPECT_FALSE(
          scrollable_area->GetNonCompositedMainThreadScrollingReasons());
      EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons());
    }
  }
};

INSTANTIATE_TEST_CASE_P(All,
                        NonCompositedMainThreadScrollingReasonTest,
                        ::testing::Bool());

TEST_P(NonCompositedMainThreadScrollingReasonTest, TransparentTest) {
  TestNonCompositedReasons("transparent",
                           MainThreadScrollingReason::kHasOpacityAndLCDText);
}

TEST_P(NonCompositedMainThreadScrollingReasonTest, TransformTest) {
  TestNonCompositedReasons("transform",
                           MainThreadScrollingReason::kHasTransformAndLCDText);
}

TEST_P(NonCompositedMainThreadScrollingReasonTest, BackgroundNotOpaqueTest) {
  TestNonCompositedReasons(
      "background-not-opaque",
      MainThreadScrollingReason::kBackgroundNotOpaqueInRectAndLCDText);
}

TEST_P(NonCompositedMainThreadScrollingReasonTest, BorderRadiusTest) {
  TestNonCompositedReasons("border-radius",
                           MainThreadScrollingReason::kHasBorderRadius);
}

TEST_P(NonCompositedMainThreadScrollingReasonTest, ClipTest) {
  TestNonCompositedReasons("clip",
                           MainThreadScrollingReason::kHasClipRelatedProperty);
}

TEST_P(NonCompositedMainThreadScrollingReasonTest, ClipPathTest) {
  uint32_t clip_reason = MainThreadScrollingReason::kHasClipRelatedProperty;
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  Document* document = GetFrame()->GetDocument();
  // Test ancestor with ClipPath
  Element* element = document->body();
  ASSERT_TRUE(element);
  element->setAttribute(HTMLNames::styleAttr,
                        "clip-path:circle(115px at 20px 20px);");
  Element* container = document->getElementById("scroller1");
  ASSERT_TRUE(container);
  ForceFullCompositingUpdate();

  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(container->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_TRUE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
              clip_reason);

  LocalFrameView* frame_view = GetFrame()->View();
  ASSERT_TRUE(frame_view);
  EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons() & clip_reason);

  // Remove clip path from ancestor.
  element->removeAttribute(HTMLNames::styleAttr);
  ForceFullCompositingUpdate();

  EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
               clip_reason);
  EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons() & clip_reason);

  // Test descendant with ClipPath
  element = document->getElementById("content1");
  ASSERT_TRUE(element);
  element->setAttribute(HTMLNames::styleAttr,
                        "clip-path:circle(115px at 20px 20px);");
  ForceFullCompositingUpdate();
  EXPECT_TRUE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
              clip_reason);
  EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons() & clip_reason);

  // Remove clip path from descendant.
  element->removeAttribute(HTMLNames::styleAttr);
  ForceFullCompositingUpdate();
  EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
               clip_reason);
  EXPECT_FALSE(frame_view->GetMainThreadScrollingReasons() & clip_reason);
}

TEST_P(NonCompositedMainThreadScrollingReasonTest, LCDTextEnabledTest) {
  TestNonCompositedReasons("transparent border-radius",
                           MainThreadScrollingReason::kHasOpacityAndLCDText |
                               MainThreadScrollingReason::kHasBorderRadius);
}

TEST_P(NonCompositedMainThreadScrollingReasonTest, BoxShadowTest) {
  TestNonCompositedReasons(
      "box-shadow", MainThreadScrollingReason::kHasBoxShadowFromNonRootLayer);
}

TEST_P(NonCompositedMainThreadScrollingReasonTest, StackingContextTest) {
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);

  Document* document = GetFrame()->GetDocument();
  Element* container = document->getElementById("scroller1");
  ASSERT_TRUE(container);

  ForceFullCompositingUpdate();

  // If a scroller contains all its children, it's not a stacking context.
  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(container->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_TRUE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
              MainThreadScrollingReason::kIsNotStackingContextAndLCDText);

  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  ForceFullCompositingUpdate();
  EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons() &
               MainThreadScrollingReason::kIsNotStackingContextAndLCDText);
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);

  // Adding "contain: paint" to force a stacking context leads to promotion.
  container->setAttribute("style", "contain: paint", ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();

  EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons());
}

TEST_P(NonCompositedMainThreadScrollingReasonTest,
       CompositedWithLCDTextRelatedReasonsTest) {
  // With "will-change:transform" we composite elements with
  // LCDTextRelatedReasons only. For elements with other
  // NonCompositedReasons, we don't create scrollingLayer for their
  // CompositedLayerMapping therefore they don't get composited.
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  Document* document = GetFrame()->GetDocument();
  Element* container = document->getElementById("scroller1");
  ASSERT_TRUE(container);
  container->setAttribute("class", "composited transparent",
                          ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();

  PaintLayerScrollableArea* scrollable_area =
      ToLayoutBoxModelObject(container->GetLayoutObject())->GetScrollableArea();
  ASSERT_TRUE(scrollable_area);
  EXPECT_FALSE(scrollable_area->GetNonCompositedMainThreadScrollingReasons());

  Element* container2 = document->getElementById("scroller2");
  ASSERT_TRUE(container2);
  container2->setAttribute("class", "composited border-radius",
                           ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();
  PaintLayerScrollableArea* scrollable_area2 =
      ToLayoutBoxModelObject(container2->GetLayoutObject())
          ->GetScrollableArea();
  ASSERT_TRUE(scrollable_area2);
  EXPECT_TRUE(scrollable_area2->GetNonCompositedMainThreadScrollingReasons() &
              MainThreadScrollingReason::kHasBorderRadius);
}

}  // namespace blink
