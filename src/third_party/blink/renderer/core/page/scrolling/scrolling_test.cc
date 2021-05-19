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

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/layers/scrollbar_layer_base.h"
#include "cc/trees/compositor_commit_data.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/sticky_position_constraint.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_sheet_list.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_resolved.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator_context.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/testing/find_cc_layer.h"
#include "third_party/blink/renderer/platform/testing/histogram_tester.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "ui/base/ui_base_features.h"

namespace blink {

class ScrollingTest : public testing::Test, public PaintTestConfigurations {
 public:
  ScrollingTest() : base_url_("http://www.test.com/") {
    helper_.Initialize(nullptr, nullptr, &ConfigureSettings);
    GetWebView()->MainFrameViewWidget()->Resize(gfx::Size(320, 240));
    GetWebView()->MainFrameViewWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  ~ScrollingTest() override {
    url_test_helpers::UnregisterAllURLsAndClearMemoryCache();
  }

  void NavigateTo(const std::string& url) {
    frame_test_helpers::LoadFrame(GetWebView()->MainFrameImpl(), url);
  }

  void LoadHTML(const std::string& html) {
    frame_test_helpers::LoadHTMLString(GetWebView()->MainFrameImpl(), html,
                                       url_test_helpers::ToKURL("about:blank"));
  }

  void ForceFullCompositingUpdate() {
    GetWebView()->MainFrameViewWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  void RegisterMockedHttpURLLoad(const std::string& file_name) {
    // TODO(crbug.com/751425): We should use the mock functionality
    // via |helper_|.
    url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8(base_url_), test::CoreTestDataPath(),
        WebString::FromUTF8(file_name));
  }

  WebViewImpl* GetWebView() const { return helper_.GetWebView(); }
  LocalFrame* GetFrame() const { return helper_.LocalMainFrame()->GetFrame(); }

  frame_test_helpers::TestWebFrameWidget* GetMainFrameWidget() const {
    return helper_.GetMainFrameWidget();
  }

  PaintLayerScrollableArea* ScrollableAreaByDOMElementId(
      const char* id_value) const {
    return GetFrame()
        ->GetDocument()
        ->getElementById(id_value)
        ->GetLayoutBoxForScrolling()
        ->GetScrollableArea();
  }

  void LoadAhem() { helper_.LoadAhem(); }

  const cc::ScrollNode* ScrollNodeForScrollableArea(
      const ScrollableArea* scrollable_area) const {
    if (!scrollable_area)
      return nullptr;
    const auto* property_trees =
        RootCcLayer()->layer_tree_host()->property_trees();
    return property_trees->scroll_tree.Node(
        property_trees->element_id_to_scroll_node_index.at(
            scrollable_area->GetScrollElementId()));
  }

  const cc::ScrollNode* ScrollNodeByDOMElementId(const char* dom_id) const {
    return ScrollNodeForScrollableArea(ScrollableAreaByDOMElementId(dom_id));
  }

  gfx::ScrollOffset CurrentScrollOffset(cc::ElementId element_id) const {
    return RootCcLayer()
        ->layer_tree_host()
        ->property_trees()
        ->scroll_tree.current_scroll_offset(element_id);
  }

  gfx::ScrollOffset CurrentScrollOffset(
      const cc::ScrollNode* scroll_node) const {
    return CurrentScrollOffset(scroll_node->element_id);
  }

  const cc::ScrollbarLayerBase* ScrollbarLayerForScrollNode(
      const cc::ScrollNode* scroll_node,
      cc::ScrollbarOrientation orientation) const {
    return blink::ScrollbarLayerForScrollNode(RootCcLayer(), scroll_node,
                                              orientation);
  }

  const cc::Layer* RootCcLayer() const {
    return GetFrame()->View()->RootCcLayer();
  }

  cc::LayerTreeHost* LayerTreeHost() const {
    return helper_.GetLayerTreeHost();
  }

  const cc::Layer* FrameScrollingContentsLayer(const LocalFrame& frame) const {
    return ScrollingContentsCcLayerByScrollElementId(
        RootCcLayer(), frame.View()->LayoutViewport()->GetScrollElementId());
  }

  const cc::Layer* MainFrameScrollingContentsLayer() const {
    return FrameScrollingContentsLayer(*GetFrame());
  }

  const cc::Layer* LayerByDOMElementId(const char* dom_id) const {
    return CcLayersByDOMElementId(RootCcLayer(), dom_id)[0];
  }

  const cc::Layer* ScrollingContentsLayerByDOMElementId(
      const char* element_id) const {
    const auto* scrollable_area = ScrollableAreaByDOMElementId(element_id);
    return ScrollingContentsCcLayerByScrollElementId(
        RootCcLayer(), scrollable_area->GetScrollElementId());
  }

 protected:
  std::string base_url_;

 private:
  static void ConfigureSettings(WebSettings* settings) {
    settings->SetPreferCompositingToLCDTextEnabled(true);
  }

  frame_test_helpers::WebViewHelper helper_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(ScrollingTest);

TEST_P(ScrollingTest, fastScrollingByDefault) {
  GetWebView()->MainFrameViewWidget()->Resize(gfx::Size(800, 600));
  LoadHTML("<div id='spacer' style='height: 1000px'></div>");
  ForceFullCompositingUpdate();

  // Make sure the scrolling coordinator is active.
  LocalFrameView* frame_view = GetFrame()->View();
  Page* page = GetFrame()->GetPage();
  ASSERT_TRUE(page->GetScrollingCoordinator());

  // Fast scrolling should be enabled by default.
  const auto* outer_scroll_node =
      ScrollNodeForScrollableArea(frame_view->LayoutViewport());
  ASSERT_TRUE(outer_scroll_node);
  EXPECT_FALSE(outer_scroll_node->main_thread_scrolling_reasons);

  ASSERT_EQ(cc::EventListenerProperties::kNone,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kTouchStartOrMove));
  ASSERT_EQ(cc::EventListenerProperties::kNone,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kMouseWheel));

  const auto* inner_scroll_node =
      ScrollNodeForScrollableArea(&page->GetVisualViewport());
  ASSERT_TRUE(inner_scroll_node);
  EXPECT_FALSE(inner_scroll_node->main_thread_scrolling_reasons);
}

TEST_P(ScrollingTest, fastFractionalScrollingDiv) {
  ScopedFractionalScrollOffsetsForTest fractional_scroll_offsets(true);

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

  const auto* scroll_node = ScrollNodeByDOMElementId("scroller");
  ASSERT_TRUE(scroll_node);
  ASSERT_NEAR(1.2f, CurrentScrollOffset(scroll_node).x(), 0.01f);
  ASSERT_NEAR(1.2f, CurrentScrollOffset(scroll_node).y(), 0.01f);
}

TEST_P(ScrollingTest, fastScrollingForFixedPosition) {
  RegisterMockedHttpURLLoad("fixed-position.html");
  NavigateTo(base_url_ + "fixed-position.html");
  ForceFullCompositingUpdate();

  const auto* scroll_node =
      ScrollNodeForScrollableArea(GetFrame()->View()->LayoutViewport());
  ASSERT_TRUE(scroll_node);
  EXPECT_FALSE(scroll_node->main_thread_scrolling_reasons);
}

// Sticky constraints are stored on transform property tree nodes.
static cc::StickyPositionConstraint GetStickyConstraint(Element* element) {
  const auto* properties =
      element->GetLayoutObject()->FirstFragment().PaintProperties();
  DCHECK(properties);
  return *properties->StickyTranslation()->GetStickyConstraint();
}

TEST_P(ScrollingTest, fastScrollingForStickyPosition) {
  RegisterMockedHttpURLLoad("sticky-position.html");
  NavigateTo(base_url_ + "sticky-position.html");
  ForceFullCompositingUpdate();

  // Sticky position should not fall back to main thread scrolling.
  const auto* scroll_node =
      ScrollNodeForScrollableArea(GetFrame()->View()->LayoutViewport());
  ASSERT_TRUE(scroll_node);
  EXPECT_FALSE(scroll_node->main_thread_scrolling_reasons);

  Document* document = GetFrame()->GetDocument();
  {
    Element* element = document->getElementById("div-tl");
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(constraint.is_anchored_top && constraint.is_anchored_left &&
                !constraint.is_anchored_right &&
                !constraint.is_anchored_bottom);
    EXPECT_EQ(1.f, constraint.top_offset);
    EXPECT_EQ(1.f, constraint.left_offset);
    EXPECT_EQ(gfx::RectF(100, 100, 10, 10),
              constraint.scroll_container_relative_sticky_box_rect);
    EXPECT_EQ(gfx::RectF(100, 100, 200, 200),
              constraint.scroll_container_relative_containing_block_rect);
  }
  {
    Element* element = document->getElementById("div-tr");
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(constraint.is_anchored_top && !constraint.is_anchored_left &&
                constraint.is_anchored_right && !constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById("div-bl");
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(!constraint.is_anchored_top && constraint.is_anchored_left &&
                !constraint.is_anchored_right && constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById("div-br");
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(!constraint.is_anchored_top && !constraint.is_anchored_left &&
                constraint.is_anchored_right && constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById("span-tl");
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(constraint.is_anchored_top && constraint.is_anchored_left &&
                !constraint.is_anchored_right &&
                !constraint.is_anchored_bottom);
  }
  {
    Element* element = document->getElementById("span-tlbr");
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(constraint.is_anchored_top && constraint.is_anchored_left &&
                constraint.is_anchored_right && constraint.is_anchored_bottom);
    EXPECT_EQ(1.f, constraint.top_offset);
    EXPECT_EQ(1.f, constraint.left_offset);
    EXPECT_EQ(1.f, constraint.right_offset);
    EXPECT_EQ(1.f, constraint.bottom_offset);
  }
  {
    Element* element = document->getElementById("composited-top");
    auto constraint = GetStickyConstraint(element);
    EXPECT_TRUE(constraint.is_anchored_top);
    EXPECT_EQ(gfx::RectF(100, 110, 10, 10),
              constraint.scroll_container_relative_sticky_box_rect);
    EXPECT_EQ(gfx::RectF(100, 100, 200, 200),
              constraint.scroll_container_relative_containing_block_rect);
  }
}

TEST_P(ScrollingTest, elementPointerEventHandler) {
  LoadHTML(R"HTML(
    <div id="pointer" style="width: 100px; height: 100px;"></div>
    <script>
      pointer.addEventListener('pointerdown', function(event) {
      }, {blocking: false} );
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  // Pointer event handlers should not generate blocking touch action regions.
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_TRUE(region.IsEmpty());
}

TEST_P(ScrollingTest, touchEventHandler) {
  RegisterMockedHttpURLLoad("touch-event-handler.html");
  NavigateTo(base_url_ + "touch-event-handler.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(cc::EventListenerProperties::kBlocking,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kTouchStartOrMove));
}

TEST_P(ScrollingTest, elementBlockingTouchEventHandler) {
  LoadHTML(R"HTML(
    <div id="blocking" style="width: 100px; height: 100px;"></div>
    <script>
      blocking.addEventListener('touchstart', function(event) {
      }, {passive: false} );
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 100, 100));
}

TEST_P(ScrollingTest, touchEventHandlerPassive) {
  RegisterMockedHttpURLLoad("touch-event-handler-passive.html");
  NavigateTo(base_url_ + "touch-event-handler-passive.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(cc::EventListenerProperties::kPassive,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kTouchStartOrMove));
}

TEST_P(ScrollingTest, elementTouchEventHandlerPassive) {
  LoadHTML(R"HTML(
    <div id="passive" style="width: 100px; height: 100px;"></div>
    <script>
      passive.addEventListener('touchstart', function(event) {
      }, {passive: true} );
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  // Passive event handlers should not generate blocking touch action regions.
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_TRUE(region.IsEmpty());
}

TEST_P(ScrollingTest, TouchActionRectsOnImage) {
  LoadHTML(R"HTML(
    <img id="image" style="width: 100px; height: 100px; touch-action: none;">
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 100, 100));
}

TEST_P(ScrollingTest, touchEventHandlerBoth) {
  RegisterMockedHttpURLLoad("touch-event-handler-both.html");
  NavigateTo(base_url_ + "touch-event-handler-both.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(cc::EventListenerProperties::kBlockingAndPassive,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kTouchStartOrMove));
}

TEST_P(ScrollingTest, wheelEventHandler) {
  RegisterMockedHttpURLLoad("wheel-event-handler.html");
  NavigateTo(base_url_ + "wheel-event-handler.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(cc::EventListenerProperties::kBlocking,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kMouseWheel));
}

TEST_P(ScrollingTest, wheelEventHandlerPassive) {
  RegisterMockedHttpURLLoad("wheel-event-handler-passive.html");
  NavigateTo(base_url_ + "wheel-event-handler-passive.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(cc::EventListenerProperties::kPassive,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kMouseWheel));
}

TEST_P(ScrollingTest, wheelEventHandlerBoth) {
  RegisterMockedHttpURLLoad("wheel-event-handler-both.html");
  NavigateTo(base_url_ + "wheel-event-handler-both.html");
  ForceFullCompositingUpdate();

  ASSERT_EQ(cc::EventListenerProperties::kBlockingAndPassive,
            LayerTreeHost()->event_listener_properties(
                cc::EventListenerClass::kMouseWheel));
}

TEST_P(ScrollingTest, scrollEventHandler) {
  RegisterMockedHttpURLLoad("scroll-event-handler.html");
  NavigateTo(base_url_ + "scroll-event-handler.html");
  ForceFullCompositingUpdate();

  ASSERT_TRUE(GetMainFrameWidget()->HaveScrollEventHandlers());
}

TEST_P(ScrollingTest, updateEventHandlersDuringTeardown) {
  RegisterMockedHttpURLLoad("scroll-event-handler-window.html");
  NavigateTo(base_url_ + "scroll-event-handler-window.html");
  ForceFullCompositingUpdate();

  // Simulate detaching the document from its DOM window. This should not
  // cause a crash when the WebViewImpl is closed by the test runner.
  GetFrame()->GetDocument()->Shutdown();
}

TEST_P(ScrollingTest, clippedBodyTest) {
  RegisterMockedHttpURLLoad("clipped-body.html");
  NavigateTo(base_url_ + "clipped-body.html");
  ForceFullCompositingUpdate();

  const auto* root_scroll_layer = MainFrameScrollingContentsLayer();
  EXPECT_TRUE(root_scroll_layer->non_fast_scrollable_region().IsEmpty());
}

TEST_P(ScrollingTest, touchAction) {
  RegisterMockedHttpURLLoad("touch-action.html");
  NavigateTo(base_url_ + "touch-action.html");
  ForceFullCompositingUpdate();

  const auto* cc_layer = ScrollingContentsLayerByDOMElementId("scrollable");
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanX | TouchAction::kPanDown |
      TouchAction::kInternalPanXScrolls);
  EXPECT_EQ(region.GetRegionComplexity(), 1);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 1000, 1000));
}

TEST_P(ScrollingTest, touchActionRegions) {
  RegisterMockedHttpURLLoad("touch-action-regions.html");
  NavigateTo(base_url_ + "touch-action-regions.html");
  ForceFullCompositingUpdate();

  const auto* cc_layer = ScrollingContentsLayerByDOMElementId("scrollable");

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanDown | TouchAction::kPanX |
      TouchAction::kInternalPanXScrolls);
  EXPECT_EQ(region.GetRegionComplexity(), 1);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 100, 100));

  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanDown | TouchAction::kPanRight |
      TouchAction::kInternalPanXScrolls);
  EXPECT_EQ(region.GetRegionComplexity(), 1);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 50, 50));

  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanDown);
  EXPECT_EQ(region.GetRegionComplexity(), 1);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 100, 100, 100));
}

TEST_P(ScrollingTest, touchActionNesting) {
  LoadHTML(R"HTML(
    <style>
      #scrollable {
        width: 200px;
        height: 200px;
        background: blue;
        overflow: scroll;
      }
      #touchaction {
        touch-action: pan-x;
        width: 100px;
        height: 100px;
        margin: 5px;
      }
      #child {
        width: 150px;
        height: 50px;
      }
    </style>
    <div id="scrollable">
      <div id="touchaction">
        <div id="child"></div>
      </div>
      <div id="forcescroll" style="width: 1000px; height: 1000px;"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = ScrollingContentsLayerByDOMElementId("scrollable");

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanX | TouchAction::kInternalPanXScrolls);
  EXPECT_EQ(region.GetRegionComplexity(), 2);
  EXPECT_EQ(region.bounds(), gfx::Rect(5, 5, 150, 100));
}

TEST_P(ScrollingTest, nestedTouchActionInvalidation) {
  LoadHTML(R"HTML(
    <style>
      #scrollable {
        width: 200px;
        height: 200px;
        background: blue;
        overflow: scroll;
      }
      #touchaction {
        touch-action: pan-x;
        width: 100px;
        height: 100px;
        margin: 5px;
      }
      #child {
        width: 150px;
        height: 50px;
      }
    </style>
    <div id="scrollable">
      <div id="touchaction">
        <div id="child"></div>
      </div>
      <div id="forcescroll" style="width: 1000px; height: 1000px;"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = ScrollingContentsLayerByDOMElementId("scrollable");

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanX | TouchAction::kInternalPanXScrolls);
  EXPECT_EQ(region.GetRegionComplexity(), 2);
  EXPECT_EQ(region.bounds(), gfx::Rect(5, 5, 150, 100));

  auto* scrollable = GetFrame()->GetDocument()->getElementById("scrollable");
  scrollable->setAttribute("style", "touch-action: none", ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanX | TouchAction::kInternalPanXScrolls);
  EXPECT_TRUE(region.IsEmpty());
}

// Similar to nestedTouchActionInvalidation but tests that an ancestor with
// touch-action: pan-x and a descendant with touch-action: pan-y results in a
// touch-action rect of none for the descendant.
TEST_P(ScrollingTest, nestedTouchActionChangesUnion) {
  LoadHTML(R"HTML(
    <style>
      #ancestor {
        width: 100px;
        height: 100px;
      }
      #child {
        touch-action: pan-x;
        width: 150px;
        height: 50px;
      }
    </style>
    <div id="ancestor">
      <div id="child"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanX | TouchAction::kInternalPanXScrolls);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 150, 50));
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_TRUE(region.IsEmpty());

  Element* ancestor = GetFrame()->GetDocument()->getElementById("ancestor");
  ancestor->setAttribute(html_names::kStyleAttr, "touch-action: pan-y");
  ForceFullCompositingUpdate();

  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanY);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 100, 100));
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanX | TouchAction::kInternalPanXScrolls);
  EXPECT_TRUE(region.IsEmpty());
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 150, 50));
}

TEST_P(ScrollingTest, touchActionEditableElement) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({::features::kSwipeToMoveCursor}, {});
  if (!::features::IsSwipeToMoveCursorEnabled())
    return;
  // Long text that will overflow in y-direction.
  LoadHTML(R"HTML(
    <style>
      #touchaction {
        touch-action: manipulation;
        width: 100px;
        height: 50px;
        overflow: scroll;
      }
    </style>
    <div id="touchaction" contenteditable>
      <div id="child"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();
  const auto* cc_layer = MainFrameScrollingContentsLayer();
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kManipulation);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 100, 50));
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_TRUE(region.IsEmpty());

  // Make touchaction scrollable by making child overflow.
  Element* child = GetFrame()->GetDocument()->getElementById("child");
  child->setAttribute("style", "width: 1000px; height: 100px;",
                      ASSERT_NO_EXCEPTION);
  ForceFullCompositingUpdate();

  cc_layer = ScrollingContentsLayerByDOMElementId("touchaction");
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kManipulation | TouchAction::kInternalPanXScrolls);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 1000, 100));
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_TRUE(region.IsEmpty());
}

// Box shadow is not hit testable and should not be included in touch action.
TEST_P(ScrollingTest, touchActionExcludesBoxShadow) {
  LoadHTML(R"HTML(
    <style>
      #shadow {
        width: 100px;
        height: 100px;
        touch-action: none;
        box-shadow: 10px 5px 5px red;
      }
    </style>
    <div id="shadow"></div>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 100, 100));
}

TEST_P(ScrollingTest, touchActionOnInline) {
  RegisterMockedHttpURLLoad("touch-action-on-inline.html");
  NavigateTo(base_url_ + "touch-action-on-inline.html");
  LoadAhem();
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 120, 50));
}

TEST_P(ScrollingTest, touchActionOnText) {
  RegisterMockedHttpURLLoad("touch-action-on-text.html");
  NavigateTo(base_url_ + "touch-action-on-text.html");
  LoadAhem();
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 160, 30));
}

TEST_P(ScrollingTest, touchActionWithVerticalRLWritingMode) {
  RegisterMockedHttpURLLoad("touch-action-with-vertical-rl-writing-mode.html");
  NavigateTo(base_url_ + "touch-action-with-vertical-rl-writing-mode.html");
  LoadAhem();
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(292, 8, 20, 80));
}

TEST_P(ScrollingTest, touchActionBlockingHandler) {
  RegisterMockedHttpURLLoad("touch-action-blocking-handler.html");
  NavigateTo(base_url_ + "touch-action-blocking-handler.html");
  ForceFullCompositingUpdate();

  const auto* cc_layer = ScrollingContentsLayerByDOMElementId("scrollable");

  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(region.GetRegionComplexity(), 1);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 100, 100));

  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanY);
  EXPECT_EQ(region.GetRegionComplexity(), 2);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 1000, 1000));
}

TEST_P(ScrollingTest, touchActionOnScrollingElement) {
  LoadHTML(R"HTML(
    <style>
      #scrollable {
        width: 100px;
        height: 100px;
        overflow: scroll;
        touch-action: pan-y;
      }
      #child {
        width: 50px;
        height: 150px;
      }
    </style>
    <div id="scrollable">
      <div id="child"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  // The outer layer (not scrollable) will be fully marked as pan-y (100x100)
  // and the scrollable layer will only have the contents marked as pan-y
  // (50x150).
  const auto* scrolling_contents_layer =
      ScrollingContentsLayerByDOMElementId("scrollable");
  cc::Region region =
      scrolling_contents_layer->touch_action_region().GetRegionForTouchAction(
          TouchAction::kPanY);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 50, 150));

  const auto* container_layer =
      RuntimeEnabledFeatures::CompositeAfterPaintEnabled()
          ? MainFrameScrollingContentsLayer()
          : LayerByDOMElementId("scrollable");
  region = container_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kPanY);
  EXPECT_EQ(region.bounds(),
            RuntimeEnabledFeatures::CompositeAfterPaintEnabled()
                ? gfx::Rect(8, 8, 100, 100)
                : gfx::Rect(0, 0, 100, 100));
}

TEST_P(ScrollingTest, IframeWindowTouchHandler) {
  LoadHTML(R"HTML(
    <iframe style="width: 275px; height: 250px; will-change: transform">
    </iframe>
  )HTML");
  auto* child_frame =
      To<WebLocalFrameImpl>(GetWebView()->MainFrameImpl()->FirstChild());
  frame_test_helpers::LoadHTMLString(child_frame, R"HTML(
      <p style="margin: 1000px"> Hello </p>
      <script>
        window.addEventListener('touchstart', (e) => {
          e.preventDefault();
        }, {passive: false});
      </script>
    )HTML",
                                     url_test_helpers::ToKURL("about:blank"));
  ForceFullCompositingUpdate();

  const auto* child_cc_layer =
      FrameScrollingContentsLayer(*child_frame->GetFrame());
  cc::Region region_child_frame =
      child_cc_layer->touch_action_region().GetRegionForTouchAction(
          TouchAction::kNone);
  cc::Region region_main_frame =
      MainFrameScrollingContentsLayer()
          ->touch_action_region()
          .GetRegionForTouchAction(TouchAction::kNone);
  EXPECT_TRUE(region_main_frame.bounds().IsEmpty());
  EXPECT_FALSE(region_child_frame.bounds().IsEmpty());
  // We only check for the content size for verification as the offset is 0x0
  // due to child frame having its own composited layer.

  // Because touch action rects are painted on the scrolling contents layer,
  // the size of the rect should be equal to the entire scrolling contents area.
  EXPECT_EQ(gfx::Rect(child_cc_layer->bounds()), region_child_frame.bounds());
}

TEST_P(ScrollingTest, WindowTouchEventHandler) {
  LoadHTML(R"HTML(
    <style>
      html { width: 200px; height: 200px; }
      body { width: 100px; height: 100px; }
    </style>
    <script>
      window.addEventListener('touchstart', function(event) {
        event.preventDefault();
      }, {passive: false} );
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  auto* cc_layer = MainFrameScrollingContentsLayer();

  // The touch action region should include the entire frame, even though the
  // document is smaller than the frame.
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 320, 240));
}

namespace {
class ScrollingTestMockEventListener final : public NativeEventListener {
 public:
  void Invoke(ExecutionContext*, Event*) override {}
};
}  // namespace

TEST_P(ScrollingTest, WindowTouchEventHandlerInvalidation) {
  LoadHTML("");
  ForceFullCompositingUpdate();

  auto* cc_layer = MainFrameScrollingContentsLayer();

  // Initially there are no touch action regions.
  auto region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_TRUE(region.IsEmpty());

  // Adding a blocking window event handler should create a touch action region.
  auto* listener = MakeGarbageCollected<ScrollingTestMockEventListener>();
  auto* resolved_options =
      MakeGarbageCollected<AddEventListenerOptionsResolved>();
  resolved_options->setPassive(false);
  GetFrame()->DomWindow()->addEventListener(event_type_names::kTouchstart,
                                            listener, resolved_options);
  ForceFullCompositingUpdate();
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_FALSE(region.IsEmpty());

  // Removing the window event handler also removes the blocking touch action
  // region.
  GetFrame()->DomWindow()->RemoveAllEventListeners();
  ForceFullCompositingUpdate();
  region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_TRUE(region.IsEmpty());
}

TEST_P(ScrollingTest, TouchActionChangeWithoutContent) {
  LoadHTML(R"HTML(
    <div id="blocking"
        style="will-change: transform; width: 100px; height: 100px;"></div>
  )HTML");
  ForceFullCompositingUpdate();

  // Adding a blocking window event handler should create a touch action region.
  auto* listener = MakeGarbageCollected<ScrollingTestMockEventListener>();
  auto* resolved_options =
      MakeGarbageCollected<AddEventListenerOptionsResolved>();
  resolved_options->setPassive(false);
  auto* target_element = GetFrame()->GetDocument()->getElementById("blocking");
  target_element->addEventListener(event_type_names::kTouchstart, listener,
                                   resolved_options);
  ForceFullCompositingUpdate();

  const auto* cc_layer = LayerByDOMElementId("blocking");
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 100, 100));
}

TEST_P(ScrollingTest, WheelEventRegion) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kWheelEventRegions);
  LoadHTML(R"HTML(
    <style>
      #scrollable {
        width: 200px;
        height: 200px;
        will-change: transform;
        overflow: scroll;
      }
      #content {
        width: 1000px;
        height: 1000px;
      }
    </style>
    <div id="scrollable">
      <div id="content"></div>
    </div>
    <script>
      document.getElementById("scrollable").addEventListener('wheel', (e) => {
        e.preventDefault();
      });
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  auto* cc_layer = MainFrameScrollingContentsLayer();
  cc::Region region = cc_layer->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());

  cc_layer = LayerByDOMElementId("scrollable");
  region = cc_layer->wheel_event_region();
  EXPECT_EQ(region.GetRegionComplexity(), 1);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 200, 200));

  cc_layer = ScrollingContentsLayerByDOMElementId("scrollable");
  region = cc_layer->wheel_event_region();
  EXPECT_EQ(region.GetRegionComplexity(), 1);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 1000, 1000));
}

TEST_P(ScrollingTest, WheelEventHandlerInvalidation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kWheelEventRegions);
  LoadHTML(R"HTML(
    <style>
      #scrollable {
        width: 200px;
        height: 200px;
        will-change: transform;
        overflow: scroll;
      }
      #content {
        width: 1000px;
        height: 1000px;
      }
    </style>
    <div id="scrollable">
      <div id="content"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  // Initially there are no wheel event regions.
  const auto* cc_layer = LayerByDOMElementId("scrollable");
  auto region = cc_layer->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());

  const auto* cc_layer_content =
      ScrollingContentsLayerByDOMElementId("scrollable");
  region = cc_layer->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());

  // Adding a blocking window event handler should create a wheel event region.
  auto* listener = MakeGarbageCollected<ScrollingTestMockEventListener>();
  auto* resolved_options =
      MakeGarbageCollected<AddEventListenerOptionsResolved>();
  resolved_options->setPassive(false);
  GetFrame()
      ->GetDocument()
      ->getElementById("scrollable")
      ->addEventListener(event_type_names::kWheel, listener, resolved_options);
  ForceFullCompositingUpdate();
  region = cc_layer->wheel_event_region();
  EXPECT_FALSE(region.IsEmpty());
  region = cc_layer_content->wheel_event_region();
  EXPECT_FALSE(region.IsEmpty());

  // Removing the window event handler also removes the wheel event region.
  GetFrame()
      ->GetDocument()
      ->getElementById("scrollable")
      ->RemoveAllEventListeners();
  ForceFullCompositingUpdate();
  region = cc_layer->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());
  region = cc_layer_content->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());
}

TEST_P(ScrollingTest, WheelEventRegions) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kWheelEventRegions);
  LoadHTML(R"HTML(
    <style>
      #scrollable {
        width: 200px;
        height: 200px;
        will-change: transform;
        overflow: scroll;
      }
      #content {
        width: 1000px;
        height: 1000px;
      }
      .region {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id="scrollable">
      <div id="region1" class="region"></div>
      <div id="content"></div>
      <div id="region2" class="region"></div>
    </div>
    <script>
      document.getElementById("region1").addEventListener('wheel', (e) => {
        e.preventDefault();
      });
      document.getElementById("region2").addEventListener('wheel', (e) => {
        e.preventDefault();
      });
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  auto* cc_layer = LayerByDOMElementId("scrollable");
  cc::Region region = cc_layer->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());

  cc_layer = ScrollingContentsLayerByDOMElementId("scrollable");
  region = cc_layer->wheel_event_region();

  cc::Region expected_region;
  expected_region.Union(gfx::Rect(0, 0, 100, 100));
  expected_region.Union(gfx::Rect(0, 1100, 100, 100));

  EXPECT_EQ(region, expected_region);
}

TEST_P(ScrollingTest, WheelEventRegionUpdatedOnSubscrollerScrollChange) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kWheelEventRegions);
  GetWebView()->GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  LoadHTML(R"HTML(
    <style>
      #noncomposited {
        width: 200px;
        height: 200px;
        overflow: auto;
        position: absolute;
        top: 50px;
        background: white;
        box-shadow: 10px 10px black inset;
      }
      #content {
        width: 100%;
        height: 1000px;
      }
      .region {
        width: 100px;
        height: 100px;
      }
    </style>
    <div id="noncomposited">
      <div id="region" class="region"></div>
      <div id="content"></div>
    </div>
    <script>
      document.getElementById("region").addEventListener('wheel', (e) => {
        e.preventDefault();
      }, {passive: false});
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();
  cc::Region region = cc_layer->wheel_event_region();
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 50, 100, 100));

  Element* scrollable_element =
      GetFrame()->GetDocument()->getElementById("noncomposited");
  DCHECK(scrollable_element);

  // Change scroll position and verify that blocking wheel handler region is
  // updated accordingly.
  scrollable_element->setScrollTop(10.0);
  ForceFullCompositingUpdate();
  region = cc_layer->wheel_event_region();
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 50, 100, 90));
}

// Box shadow is not hit testable and should not be included in wheel region.
TEST_P(ScrollingTest, WheelEventRegionExcludesBoxShadow) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kWheelEventRegions);
  LoadHTML(R"HTML(
    <style>
      #shadow {
        width: 100px;
        height: 100px;
        box-shadow: 10px 5px 5px red;
      }
    </style>
    <div id="shadow"></div>
    <script>
      document.getElementById("shadow").addEventListener('wheel', (e) => {
        e.preventDefault();
      });
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = MainFrameScrollingContentsLayer();

  cc::Region region = cc_layer->wheel_event_region();
  EXPECT_EQ(region.bounds(), gfx::Rect(8, 8, 100, 100));
}

TEST_P(ScrollingTest, IframeWindowWheelEventHandler) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kWheelEventRegions);
  LoadHTML(R"HTML(
    <iframe style="width: 275px; height: 250px; will-change: transform">
    </iframe>
  )HTML");
  auto* child_frame =
      To<WebLocalFrameImpl>(GetWebView()->MainFrameImpl()->FirstChild());
  frame_test_helpers::LoadHTMLString(child_frame, R"HTML(
      <p style="margin: 1000px"> Hello </p>
      <script>
        window.addEventListener('wheel', (e) => {
          e.preventDefault();
        }, {passive: false});
      </script>
    )HTML",
                                     url_test_helpers::ToKURL("about:blank"));
  ForceFullCompositingUpdate();

  const auto* child_cc_layer =
      FrameScrollingContentsLayer(*child_frame->GetFrame());
  cc::Region region_child_frame = child_cc_layer->wheel_event_region();
  cc::Region region_main_frame =
      MainFrameScrollingContentsLayer()->wheel_event_region();
  EXPECT_TRUE(region_main_frame.bounds().IsEmpty());
  EXPECT_FALSE(region_child_frame.bounds().IsEmpty());
  // We only check for the content size for verification as the offset is 0x0
  // due to child frame having its own composited layer.

  // Because blocking wheel rects are painted on the scrolling contents layer,
  // the size of the rect should be equal to the entire scrolling contents area.
  EXPECT_EQ(gfx::Rect(child_cc_layer->bounds()), region_child_frame.bounds());
}

TEST_P(ScrollingTest, WindowWheelEventHandler) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kWheelEventRegions);
  LoadHTML(R"HTML(
    <style>
      html { width: 200px; height: 200px; }
      body { width: 100px; height: 100px; }
    </style>
    <script>
      window.addEventListener('wheel', function(event) {
        event.preventDefault();
      }, {passive: false} );
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  auto* cc_layer = MainFrameScrollingContentsLayer();

  // The wheel region should include the entire frame, even though the
  // document is smaller than the frame.
  cc::Region region = cc_layer->wheel_event_region();
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 320, 240));
}

TEST_P(ScrollingTest, WindowWheelEventHandlerInvalidation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kWheelEventRegions);
  LoadHTML("");
  ForceFullCompositingUpdate();

  auto* cc_layer = MainFrameScrollingContentsLayer();

  // Initially there are no wheel event regions.
  auto region = cc_layer->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());

  // Adding a blocking window event handler should create a wheel event region.
  auto* listener = MakeGarbageCollected<ScrollingTestMockEventListener>();
  auto* resolved_options =
      MakeGarbageCollected<AddEventListenerOptionsResolved>();
  resolved_options->setPassive(false);
  GetFrame()->DomWindow()->addEventListener(event_type_names::kWheel, listener,
                                            resolved_options);
  ForceFullCompositingUpdate();
  region = cc_layer->wheel_event_region();
  EXPECT_FALSE(region.IsEmpty());

  // Removing the window event handler also removes the wheel event region.
  GetFrame()->DomWindow()->RemoveAllEventListeners();
  ForceFullCompositingUpdate();
  region = cc_layer->wheel_event_region();
  EXPECT_TRUE(region.IsEmpty());
}

TEST_P(ScrollingTest, WheelEventHandlerChangeWithoutContent) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(::features::kWheelEventRegions);
  LoadHTML(R"HTML(
    <div id="blocking"
        style="will-change: transform; width: 100px; height: 100px;"></div>
  )HTML");
  ForceFullCompositingUpdate();

  // Adding a blocking window event handler should create a touch action region.
  auto* listener = MakeGarbageCollected<ScrollingTestMockEventListener>();
  auto* resolved_options =
      MakeGarbageCollected<AddEventListenerOptionsResolved>();
  resolved_options->setPassive(false);
  auto* target_element = GetFrame()->GetDocument()->getElementById("blocking");
  target_element->addEventListener(event_type_names::kWheel, listener,
                                   resolved_options);
  ForceFullCompositingUpdate();

  const auto* cc_layer = LayerByDOMElementId("blocking");
  cc::Region region = cc_layer->wheel_event_region();
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 100, 100));
}

// Ensure we don't crash when a plugin becomes a LayoutInline
TEST_P(ScrollingTest, PluginBecomesLayoutInline) {
  LoadHTML(R"HTML(
    <style>
      body {
        margin: 0;
        height: 3000px;
      }
    </style>
    <object id="plugin" type="application/x-webkit-test-plugin"></object>
    <script>
      document.getElementById("plugin")
              .appendChild(document.createElement("label"))
    </script>
  )HTML");

  // This test passes if it doesn't crash. We're trying to make sure
  // ScrollingCoordinator can deal with LayoutInline plugins when generating
  // NonFastScrollableRegions.
  auto* plugin = To<HTMLObjectElement>(
      GetFrame()->GetDocument()->getElementById("plugin"));
  ASSERT_TRUE(plugin->GetLayoutObject()->IsLayoutInline());
  ForceFullCompositingUpdate();
}

// Ensure NonFastScrollableRegions are correctly generated for both fixed and
// in-flow plugins that need them.
TEST_P(ScrollingTest, NonFastScrollableRegionsForPlugins) {
  LoadHTML(R"HTML(
    <style>
      body {
        margin: 0;
        height: 3000px;
      }
      #plugin {
        width: 300px;
        height: 300px;
      }
      #pluginfixed {
        width: 200px;
        height: 200px;
      }
      #fixed {
        position: fixed;
        left: 300px;
      }
    </style>
    <div id="fixed">
      <object id="pluginfixed" type="application/x-webkit-test-plugin"></object>
    </div>
    <object id="plugin" type="application/x-webkit-test-plugin"></object>
  )HTML");

  auto* plugin = To<HTMLObjectElement>(
      GetFrame()->GetDocument()->getElementById("plugin"));
  auto* plugin_fixed = To<HTMLObjectElement>(
      GetFrame()->GetDocument()->getElementById("pluginfixed"));
  // NonFastScrollableRegions are generated for plugins that require wheel
  // events.
  plugin->OwnedPlugin()->SetWantsWheelEvents(true);
  plugin_fixed->OwnedPlugin()->SetWantsWheelEvents(true);

  ForceFullCompositingUpdate();

  // The non-fixed plugin should create a non-fast scrollable region in the
  // scrolling contents layer of the LayoutView.
  auto* viewport_non_fast_layer = MainFrameScrollingContentsLayer();
  EXPECT_EQ(viewport_non_fast_layer->non_fast_scrollable_region().bounds(),
            gfx::Rect(0, 0, 300, 300));

  // The fixed plugin should create a non-fast scrollable region in a fixed
  // cc::Layer.
  auto* fixed_layer = LayerByDOMElementId("fixed");
  EXPECT_EQ(fixed_layer->non_fast_scrollable_region().bounds(),
            gfx::Rect(0, 0, 200, 200));
}

TEST_P(ScrollingTest, NonFastScrollableRegionWithBorder) {
  GetWebView()->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      false);
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            body { margin: 0; }
            #scroller {
              height: 100px;
              width: 100px;
              overflow-y: scroll;
              border: 10px solid black;
            }
          </style>
          <div id="scroller">
            <div id="forcescroll" style="height: 1000px;"></div>
          </div>
      )HTML");
  ForceFullCompositingUpdate();

  auto* non_fast_layer = MainFrameScrollingContentsLayer();
  EXPECT_EQ(non_fast_layer->non_fast_scrollable_region().bounds(),
            gfx::Rect(0, 0, 120, 120));
}

TEST_P(ScrollingTest, overflowScrolling) {
  RegisterMockedHttpURLLoad("overflow-scrolling.html");
  NavigateTo(base_url_ + "overflow-scrolling.html");
  ForceFullCompositingUpdate();

  // Verify the scroll node of the accelerated scrolling element.
  const auto* scroll_node = ScrollNodeByDOMElementId("scrollable");
  ASSERT_TRUE(scroll_node);
  EXPECT_TRUE(scroll_node->user_scrollable_horizontal);
  EXPECT_TRUE(scroll_node->user_scrollable_vertical);

  EXPECT_TRUE(ScrollbarLayerForScrollNode(
      scroll_node, cc::ScrollbarOrientation::HORIZONTAL));
  EXPECT_TRUE(ScrollbarLayerForScrollNode(scroll_node,
                                          cc::ScrollbarOrientation::VERTICAL));
}

TEST_P(ScrollingTest, overflowHidden) {
  RegisterMockedHttpURLLoad("overflow-hidden.html");
  NavigateTo(base_url_ + "overflow-hidden.html");
  ForceFullCompositingUpdate();

  // Verify the scroll node of the accelerated scrolling element.
  const auto* scroll_node = ScrollNodeByDOMElementId("unscrollable-y");
  ASSERT_TRUE(scroll_node);
  EXPECT_TRUE(scroll_node->user_scrollable_horizontal);
  EXPECT_FALSE(scroll_node->user_scrollable_vertical);

  scroll_node = ScrollNodeByDOMElementId("unscrollable-x");
  ASSERT_TRUE(scroll_node);
  EXPECT_FALSE(scroll_node->user_scrollable_horizontal);
  EXPECT_TRUE(scroll_node->user_scrollable_vertical);
}

TEST_P(ScrollingTest, iframeScrolling) {
  RegisterMockedHttpURLLoad("iframe-scrolling.html");
  RegisterMockedHttpURLLoad("iframe-scrolling-inner.html");
  NavigateTo(base_url_ + "iframe-scrolling.html");
  ForceFullCompositingUpdate();

  Element* scrollable_frame =
      GetFrame()->GetDocument()->getElementById("scrollable");
  ASSERT_TRUE(scrollable_frame);

  LayoutObject* layout_object = scrollable_frame->GetLayoutObject();
  ASSERT_TRUE(layout_object);
  ASSERT_TRUE(layout_object->IsLayoutEmbeddedContent());

  auto* layout_embedded_content = To<LayoutEmbeddedContent>(layout_object);
  ASSERT_TRUE(layout_embedded_content);

  LocalFrameView* inner_frame_view =
      To<LocalFrameView>(layout_embedded_content->ChildFrameView());
  ASSERT_TRUE(inner_frame_view);

  // Verify the scroll node of the accelerated scrolling iframe.
  const auto* scroll_node =
      ScrollNodeForScrollableArea(inner_frame_view->LayoutViewport());
  ASSERT_TRUE(scroll_node);
  EXPECT_TRUE(ScrollbarLayerForScrollNode(
      scroll_node, cc::ScrollbarOrientation::HORIZONTAL));
  EXPECT_TRUE(ScrollbarLayerForScrollNode(scroll_node,
                                          cc::ScrollbarOrientation::VERTICAL));
}

TEST_P(ScrollingTest, rtlIframe) {
  RegisterMockedHttpURLLoad("rtl-iframe.html");
  RegisterMockedHttpURLLoad("rtl-iframe-inner.html");
  NavigateTo(base_url_ + "rtl-iframe.html");
  ForceFullCompositingUpdate();

  Element* scrollable_frame =
      GetFrame()->GetDocument()->getElementById("scrollable");
  ASSERT_TRUE(scrollable_frame);

  LayoutObject* layout_object = scrollable_frame->GetLayoutObject();
  ASSERT_TRUE(layout_object);
  ASSERT_TRUE(layout_object->IsLayoutEmbeddedContent());

  auto* layout_embedded_content = To<LayoutEmbeddedContent>(layout_object);
  ASSERT_TRUE(layout_embedded_content);

  LocalFrameView* inner_frame_view =
      To<LocalFrameView>(layout_embedded_content->ChildFrameView());
  ASSERT_TRUE(inner_frame_view);

  // Verify the scroll node of the accelerated scrolling iframe.
  const auto* scroll_node =
      ScrollNodeForScrollableArea(inner_frame_view->LayoutViewport());
  ASSERT_TRUE(scroll_node);

  int expected_scroll_position = 958 + (inner_frame_view->LayoutViewport()
                                                ->VerticalScrollbar()
                                                ->IsOverlayScrollbar()
                                            ? 0
                                            : 15);
  ASSERT_EQ(expected_scroll_position, CurrentScrollOffset(scroll_node).x());
}

TEST_P(ScrollingTest, setupScrollbarLayerShouldNotCrash) {
  RegisterMockedHttpURLLoad("setup_scrollbar_layer_crash.html");
  NavigateTo(base_url_ + "setup_scrollbar_layer_crash.html");
  ForceFullCompositingUpdate();
  // This test document setup an iframe with scrollbars, then switch to
  // an empty document by javascript.
}

#if defined(OS_MAC) || defined(OS_ANDROID)
TEST_P(ScrollingTest, DISABLED_setupScrollbarLayerShouldSetScrollLayerOpaque)
#else
TEST_P(ScrollingTest, setupScrollbarLayerShouldSetScrollLayerOpaque)
#endif
{
  ScopedMockOverlayScrollbars mock_overlay_scrollbar(false);

  RegisterMockedHttpURLLoad("wide_document.html");
  NavigateTo(base_url_ + "wide_document.html");
  ForceFullCompositingUpdate();

  LocalFrameView* frame_view = GetFrame()->View();
  ASSERT_TRUE(frame_view);

  const auto* scroll_node =
      ScrollNodeForScrollableArea(frame_view->LayoutViewport());
  ASSERT_TRUE(scroll_node);

  const auto* horizontal_scrollbar_layer = ScrollbarLayerForScrollNode(
      scroll_node, cc::ScrollbarOrientation::HORIZONTAL);
  ASSERT_TRUE(horizontal_scrollbar_layer);
  // TODO(crbug.com/1029620): CAP needs more accurate contents_opaque.
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(!frame_view->LayoutViewport()
                   ->HorizontalScrollbar()
                   ->IsOverlayScrollbar(),
              horizontal_scrollbar_layer->contents_opaque());
  }

  EXPECT_FALSE(ScrollbarLayerForScrollNode(scroll_node,
                                           cc::ScrollbarOrientation::VERTICAL));
}

TEST_P(ScrollingTest, NestedIFramesMainThreadScrollingRegion) {
  // This page has an absolute IFRAME. It contains a scrollable child DIV
  // that's nested within an intermediate IFRAME.
  GetWebView()->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      false);
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            #spacer {
              height: 10000px;
            }
            iframe {
              position: absolute;
              top: 1200px;
              left: 0px;
              width: 200px;
              height: 200px;
              border: 0;
            }

          </style>
          <div id="spacer"></div>
          <iframe srcdoc="
              <!DOCTYPE html>
              <style>
                body { margin: 0; }
                iframe { width: 100px; height: 100px; border: 0; }
              </style>
              <iframe srcdoc='<!DOCTYPE html>
                              <style>
                                body { margin: 0; }
                                div {
                                  width: 65px;
                                  height: 65px;
                                  overflow: auto;
                                }
                                p {
                                  width: 300px;
                                  height: 300px;
                                }
                              </style>
                              <div>
                                <p></p>
                              </div>'>
              </iframe>">
          </iframe>
      )HTML");

  ForceFullCompositingUpdate();

  // Scroll the frame to ensure the rect is in the correct coordinate space.
  GetFrame()->GetDocument()->View()->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 1000), mojom::blink::ScrollType::kProgrammatic);

  ForceFullCompositingUpdate();

  auto* non_fast_layer = MainFrameScrollingContentsLayer();
  EXPECT_EQ(non_fast_layer->non_fast_scrollable_region().bounds(),
            gfx::Rect(0, 1200, 65, 65));
}

// Same as above but test that the rect is correctly calculated into the fixed
// region when the containing iframe is position: fixed.
TEST_P(ScrollingTest, NestedFixedIFramesMainThreadScrollingRegion) {
  // This page has a fixed IFRAME. It contains a scrollable child DIV that's
  // nested within an intermediate IFRAME.
  GetWebView()->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      false);
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            #spacer {
              height: 10000px;
            }
            #iframe {
              position: fixed;
              top: 20px;
              left: 0px;
              width: 200px;
              height: 200px;
              border: 20px solid blue;
            }

          </style>
          <div id="spacer"></div>
          <iframe id="iframe" srcdoc="
              <!DOCTYPE html>
              <style>
                body { margin: 0; }
                iframe { width: 100px; height: 100px; border: 0; }
              </style>
              <iframe srcdoc='<!DOCTYPE html>
                              <style>
                                body { margin: 0; }
                                div {
                                  width: 75px;
                                  height: 75px;
                                  overflow: auto;
                                }
                                p {
                                  width: 300px;
                                  height: 300px;
                                }
                              </style>
                              <div>
                                <p></p>
                              </div>'>
              </iframe>">
          </iframe>
      )HTML");

  ForceFullCompositingUpdate();

  // Scroll the frame to ensure the rect is in the correct coordinate space.
  GetFrame()->GetDocument()->View()->GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 1000), mojom::blink::ScrollType::kProgrammatic);

  ForceFullCompositingUpdate();
  auto* non_fast_layer = LayerByDOMElementId("iframe");
  EXPECT_EQ(non_fast_layer->non_fast_scrollable_region().bounds(),
            gfx::Rect(20, 20, 75, 75));
}

TEST_P(ScrollingTest, IframeCompositedScrollingHideAndShow) {
  GetWebView()->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      false);
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            body {
              margin: 0;
            }
            iframe {
              height: 100px;
              width: 100px;
            }
          </style>
          <iframe id="iframe" srcdoc="
              <!DOCTYPE html>
              <style>
                body {height: 1000px;}
              </style>"></iframe>
      )HTML");

  ForceFullCompositingUpdate();

  const auto* non_fast_layer = MainFrameScrollingContentsLayer();

  // Should have a NFSR initially.
  EXPECT_EQ(non_fast_layer->non_fast_scrollable_region().bounds(),
            gfx::Rect(2, 2, 100, 100));

  // Hiding the iframe should clear the NFSR.
  Element* iframe = GetFrame()->GetDocument()->getElementById("iframe");
  iframe->setAttribute(html_names::kStyleAttr, "display: none");
  ForceFullCompositingUpdate();
  EXPECT_TRUE(non_fast_layer->non_fast_scrollable_region().bounds().IsEmpty());

  // Showing it again should compute the NFSR.
  iframe->setAttribute(html_names::kStyleAttr, "");
  ForceFullCompositingUpdate();
  EXPECT_EQ(non_fast_layer->non_fast_scrollable_region().bounds(),
            gfx::Rect(2, 2, 100, 100));
}

// Same as above but the main frame is scrollable. This should cause the non
// fast scrollable regions to go on the outer viewport's scroll layer.
TEST_P(ScrollingTest, IframeCompositedScrollingHideAndShowScrollable) {
  GetWebView()->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      false);
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            body {
              height: 1000px;
              margin: 0;
            }
            iframe {
              height: 100px;
              width: 100px;
            }
          </style>
          <iframe id="iframe" srcdoc="
              <!DOCTYPE html>
              <style>
                body {height: 1000px;}
              </style>"></iframe>
      )HTML");

  ForceFullCompositingUpdate();

  Page* page = GetFrame()->GetPage();
  const auto* inner_viewport_scroll_layer =
      page->GetVisualViewport().LayerForScrolling();
  Element* iframe = GetFrame()->GetDocument()->getElementById("iframe");

  const auto* outer_viewport_scroll_layer = MainFrameScrollingContentsLayer();

  // Should have a NFSR initially.
  ForceFullCompositingUpdate();
  EXPECT_FALSE(outer_viewport_scroll_layer->non_fast_scrollable_region()
                   .bounds()
                   .IsEmpty());

  // Ensure the visual viewport's scrolling layer didn't get an NFSR.
  EXPECT_TRUE(inner_viewport_scroll_layer->non_fast_scrollable_region()
                  .bounds()
                  .IsEmpty());

  // Hiding the iframe should clear the NFSR.
  iframe->setAttribute(html_names::kStyleAttr, "display: none");
  ForceFullCompositingUpdate();
  EXPECT_TRUE(outer_viewport_scroll_layer->non_fast_scrollable_region()
                  .bounds()
                  .IsEmpty());

  // Showing it again should compute the NFSR.
  iframe->setAttribute(html_names::kStyleAttr, "");
  ForceFullCompositingUpdate();
  EXPECT_FALSE(outer_viewport_scroll_layer->non_fast_scrollable_region()
                   .bounds()
                   .IsEmpty());
}

TEST_P(ScrollingTest, ScrollOffsetClobberedBeforeCompositingUpdate) {
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            #container {
              width: 300px;
              height: 300px;
              overflow: auto;
              will-change: transform;
            }
            #spacer {
              height: 1000px;
            }
          </style>
          <div id="container">
            <div id="spacer"></div>
          </div>
      )HTML");
  ForceFullCompositingUpdate();

  auto* scrollable_area = ScrollableAreaByDOMElementId("container");
  ASSERT_EQ(0, scrollable_area->GetScrollOffset().Height());
  const auto* scroll_node = ScrollNodeForScrollableArea(scrollable_area);

  // Simulate 100px of scroll coming from the compositor thread during a commit.
  gfx::ScrollOffset compositor_delta(0, 100.f);
  cc::CompositorCommitData commit_data;
  commit_data.scrolls.push_back(
      {scrollable_area->GetScrollElementId(), compositor_delta, base::nullopt});
  RootCcLayer()->layer_tree_host()->ApplyCompositorChanges(&commit_data);
  // The compositor offset is reflected in blink and cc scroll tree.
  EXPECT_EQ(compositor_delta,
            gfx::ScrollOffset(scrollable_area->ScrollPosition()));
  EXPECT_EQ(compositor_delta, CurrentScrollOffset(scroll_node));

  // Before updating the lifecycle, set the scroll offset back to what it was
  // before the commit from the main thread.
  scrollable_area->SetScrollOffset(ScrollOffset(0, 0),
                                   mojom::blink::ScrollType::kProgrammatic);

  // Ensure the offset is up-to-date on the cc::Layer even though, as far as
  // the main thread is concerned, it was unchanged since the last time we
  // pushed the scroll offset.
  ForceFullCompositingUpdate();
  EXPECT_EQ(gfx::ScrollOffset(), CurrentScrollOffset(scroll_node));
}

TEST_P(ScrollingTest, UpdateVisualViewportScrollLayer) {
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            #box {
              width: 300px;
              height: 1000px;
              background-color: red;
            }
          </style>
          <div id="box">
          </div>
      )HTML");
  ForceFullCompositingUpdate();

  Page* page = GetFrame()->GetPage();
  const auto* inner_viewport_scroll_node =
      ScrollNodeForScrollableArea(&page->GetVisualViewport());

  page->GetVisualViewport().SetScale(2);
  ForceFullCompositingUpdate();
  EXPECT_EQ(gfx::ScrollOffset(0, 0),
            CurrentScrollOffset(inner_viewport_scroll_node));

  page->GetVisualViewport().SetLocation(FloatPoint(10, 20));
  ForceFullCompositingUpdate();
  EXPECT_EQ(gfx::ScrollOffset(10, 20),
            CurrentScrollOffset(inner_viewport_scroll_node));
}

TEST_P(ScrollingTest, NonCompositedNonFastScrollableRegion) {
  GetWebView()->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      false);
  LoadHTML(R"HTML(
          <!DOCTYPE html>
          <style>
            body { margin: 0; }
            #composited_container {
              will-change: transform;
              border: 20px solid blue;
            }
            #scroller {
              height: 200px;
              width: 200px;
              overflow-y: scroll;
            }
          </style>
          <div id="composited_container">
            <div id="scroller">
              <div id="forcescroll" style="height: 1000px;"></div>
            </div>
          </div>
      )HTML");
  ForceFullCompositingUpdate();

  // The non-scrolling graphics layer should have a non-scrolling region for the
  // non-composited scroller.
  const auto* cc_layer = LayerByDOMElementId("composited_container");
  auto region = cc_layer->non_fast_scrollable_region();
  EXPECT_EQ(region.bounds(), gfx::Rect(20, 20, 200, 200));
}

TEST_P(ScrollingTest, NonCompositedResizerNonFastScrollableRegion) {
  GetWebView()->GetPage()->GetSettings().SetPreferCompositingToLCDTextEnabled(
      false);
  LoadHTML(R"HTML(
    <style>
      #container {
        will-change: transform;
        border: 20px solid blue;
      }
      #scroller {
        width: 80px;
        height: 80px;
        resize: both;
        overflow-y: scroll;
      }
    </style>
    <div id="container">
      <div id="offset" style="height: 35px;"></div>
      <div id="scroller"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  auto* container_cc_layer = LayerByDOMElementId("container");
  // The non-fast scrollable region should be on the container's graphics layer
  // and not one of the viewport scroll layers because the region should move
  // when the container moves and not when the viewport scrolls.
  auto region = container_cc_layer->non_fast_scrollable_region();
  EXPECT_EQ(region.bounds(), gfx::Rect(86, 121, 14, 14));
}

TEST_P(ScrollingTest, CompositedResizerNonFastScrollableRegion) {
  LoadHTML(R"HTML(
    <style>
      #container { will-change: transform; }
      #scroller {
        will-change: transform;
        width: 80px;
        height: 80px;
        resize: both;
        overflow-y: scroll;
      }
    </style>
    <div id="container">
      <div id="offset" style="height: 35px;"></div>
      <div id="scroller"></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  auto region = LayerByDOMElementId("scroller")->non_fast_scrollable_region();
  EXPECT_EQ(region.bounds(), gfx::Rect(66, 66, 14, 14));
}

TEST_P(ScrollingTest, TouchActionUpdatesOutsideInterestRect) {
  LoadHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #scroller {
        will-change: transform;
        width: 200px;
        height: 200px;
        background: blue;
        overflow-y: scroll;
      }
      .spacer {
        height: 1000px;
      }
      #touchaction {
        height: 100px;
        background: yellow;
      }
    </style>
    <div id="scroller">
      <div class="spacer"></div>
      <div class="spacer"></div>
      <div class="spacer"></div>
      <div class="spacer"></div>
      <div class="spacer"></div>
      <div id="touchaction">This should not scroll via touch.</div>
    </div>
  )HTML");

  ForceFullCompositingUpdate();

  auto* touch_action = GetFrame()->GetDocument()->getElementById("touchaction");
  touch_action->setAttribute(html_names::kStyleAttr, "touch-action: none;");

  ForceFullCompositingUpdate();

  ScrollableAreaByDOMElementId("scroller")
      ->SetScrollOffset(ScrollOffset(0, 5100),
                        mojom::blink::ScrollType::kProgrammatic);

  ForceFullCompositingUpdate();

  auto* cc_layer = ScrollingContentsLayerByDOMElementId("scroller");
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 5000, 200, 100));
}

TEST_P(ScrollingTest, MainThreadScrollAndDeltaFromImplSide) {
  LoadHTML(R"HTML(
    <div id='scroller' style='overflow: scroll; width: 100px; height: 100px'>
      <div style='height: 1000px'></div>
    </div>
  )HTML");
  ForceFullCompositingUpdate();

  auto* scroller = GetFrame()->GetDocument()->getElementById("scroller");
  auto* scrollable_area = scroller->GetLayoutBox()->GetScrollableArea();
  auto element_id = scrollable_area->GetScrollElementId();

  EXPECT_EQ(gfx::ScrollOffset(), CurrentScrollOffset(element_id));

  // Simulate a direct scroll update out of document lifecycle update.
  scroller->scrollTo(0, 200);
  EXPECT_EQ(FloatPoint(0, 200), scrollable_area->ScrollPosition());
  EXPECT_EQ(gfx::ScrollOffset(0, 200), CurrentScrollOffset(element_id));

  // Simulate the scroll update with scroll delta from impl-side at the
  // beginning of BeginMainFrame.
  cc::CompositorCommitData commit_data;
  commit_data.scrolls.push_back(cc::CompositorCommitData::ScrollUpdateInfo(
      element_id, gfx::ScrollOffset(0, 10), base::nullopt));
  RootCcLayer()->layer_tree_host()->ApplyCompositorChanges(&commit_data);
  EXPECT_EQ(FloatPoint(0, 210), scrollable_area->ScrollPosition());
  EXPECT_EQ(gfx::ScrollOffset(0, 210), CurrentScrollOffset(element_id));
}

class UnifiedScrollingSimTest : public SimTest, public PaintTestConfigurations {
 public:
  UnifiedScrollingSimTest() : scroll_unification_enabled_(true) {}

  void SetUp() override {
    SimTest::SetUp();
    WebView().GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
    WebView().MainFrameViewWidget()->Resize(gfx::Size(1000, 1000));
    WebView().MainFrameViewWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  void RunIdleTasks() {
    auto* scheduler =
        ThreadScheduler::Current()->GetWebMainThreadSchedulerForTest();
    blink::scheduler::RunIdleTasksForTesting(scheduler,
                                             base::BindOnce([]() {}));
    test::RunPendingTasks();
  }

  const cc::Layer* RootCcLayer() { return GetDocument().View()->RootCcLayer(); }

  const cc::ScrollNode* ScrollNodeForScrollableArea(
      const ScrollableArea* scrollable_area) {
    if (!scrollable_area)
      return nullptr;
    const auto* property_trees =
        RootCcLayer()->layer_tree_host()->property_trees();
    return property_trees->scroll_tree.Node(
        property_trees->element_id_to_scroll_node_index.at(
            scrollable_area->GetScrollElementId()));
  }

  PaintLayerScrollableArea* ScrollableAreaByDOMElementId(const char* id_value) {
    auto* box = MainFrame()
                    .GetFrame()
                    ->GetDocument()
                    ->getElementById(id_value)
                    ->GetLayoutBoxForScrolling();
    return box ? box->GetScrollableArea() : nullptr;
  }

 protected:
  RuntimeEnabledFeaturesTestHelpers::ScopedScrollUnification
      scroll_unification_enabled_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(UnifiedScrollingSimTest);

// Tests that the compositor gets a scroll node for noncomposited scrollers by
// loading a page with a scroller that has an inset box-shadow, and ensuring
// that scroller generates a compositor scroll node with the proper
// noncomposited reasons set. It then removes the box-shadow property and
// ensures the compositor node updates accordingly.
TEST_P(UnifiedScrollingSimTest, ScrollNodeForNonCompositedScroller) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #noncomposited {
      width: 200px;
      height: 200px;
      overflow: auto;
      position: absolute;
      top: 300px;
      background: white;
      box-shadow: 10px 10px black inset;
    }
    #spacer {
      width: 100%;
      height: 10000px;
    }
    </style>
    <div id="noncomposited">
      <div id="spacer"></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Element* noncomposited_element =
      MainFrame().GetFrame()->GetDocument()->getElementById("noncomposited");
  auto* scrollable_area =
      noncomposited_element->GetLayoutBoxForScrolling()->GetScrollableArea();
  ASSERT_EQ(
      cc::MainThreadScrollingReason::kCantPaintScrollingBackgroundAndLCDText,
      scrollable_area->GetNonCompositedMainThreadScrollingReasons());

  const auto* scroll_node = ScrollNodeForScrollableArea(scrollable_area);
  ASSERT_TRUE(scroll_node);

  EXPECT_EQ(scroll_node->element_id, scrollable_area->GetScrollElementId());
  EXPECT_FALSE(RootCcLayer()
                   ->layer_tree_host()
                   ->property_trees()
                   ->scroll_tree.IsComposited(*scroll_node));

  // Now remove the box-shadow property and ensure the compositor scroll node
  // changes.
  noncomposited_element->setAttribute(html_names::kStyleAttr,
                                      "box-shadow: none");
  Compositor().BeginFrame();

  EXPECT_EQ(0u, scrollable_area->GetNonCompositedMainThreadScrollingReasons());
  EXPECT_EQ(scroll_node->element_id, scrollable_area->GetScrollElementId());
  EXPECT_TRUE(RootCcLayer()
                  ->layer_tree_host()
                  ->property_trees()
                  ->scroll_tree.IsComposited(*scroll_node));
}

// Tests that the compositor retains the scroll node for a composited scroller
// when it becomes noncomposited, and ensures the scroll node has its
// IsComposited state updated accordingly.
TEST_P(UnifiedScrollingSimTest,
       ScrollNodeForCompositedToNonCompositedScroller) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #composited {
      width: 200px;
      height: 200px;
      overflow: auto;
      position: absolute;
      top: 300px;
      background: white;
    }
    #spacer {
      width: 100%;
      height: 10000px;
    }
    </style>
    <div id="composited">
      <div id="spacer"></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  Element* composited_element =
      MainFrame().GetFrame()->GetDocument()->getElementById("composited");
  auto* scrollable_area =
      composited_element->GetLayoutBoxForScrolling()->GetScrollableArea();
  EXPECT_EQ(0u, scrollable_area->GetNonCompositedMainThreadScrollingReasons());

  const auto* scroll_node = ScrollNodeForScrollableArea(scrollable_area);
  ASSERT_TRUE(scroll_node);

  EXPECT_EQ(scroll_node->element_id, scrollable_area->GetScrollElementId());
  EXPECT_TRUE(RootCcLayer()
                  ->layer_tree_host()
                  ->property_trees()
                  ->scroll_tree.IsComposited(*scroll_node));

  // Now add an inset box-shadow property to make the node noncomposited and
  // ensure the compositor scroll node updates accordingly.
  composited_element->setAttribute(html_names::kStyleAttr,
                                   "box-shadow: 10px 10px black inset");
  Compositor().BeginFrame();

  ASSERT_EQ(
      cc::MainThreadScrollingReason::kCantPaintScrollingBackgroundAndLCDText,
      scrollable_area->GetNonCompositedMainThreadScrollingReasons());
  EXPECT_EQ(scroll_node->element_id, scrollable_area->GetScrollElementId());
  EXPECT_FALSE(RootCcLayer()
                   ->layer_tree_host()
                   ->property_trees()
                   ->scroll_tree.IsComposited(*scroll_node));
}

// Tests that the compositor gets a scroll node for noncomposited scrollers
// embedded in an iframe, by loading a document with an iframe that has a
// scroller with an inset box shadow, and ensuring that scroller generates a
// compositor scroll node with the proper noncomposited reasons set.
TEST_P(UnifiedScrollingSimTest, ScrollNodeForEmbeddedScrollers) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    #iframe {
      width: 300px;
      height: 300px;
      overflow: auto;
    }
    </style>
    <iframe id="iframe" srcdoc="
        <!DOCTYPE html>
        <style>
          body {
            background: white;
          }
          #scroller {
            width: 200px;
            height: 200px;
            overflow: auto;
            position: absolute;
            top: 50px;
            background: white;
            box-shadow: 10px 10px black inset;
          }
          #spacer {
            width: 100%;
            height: 10000px;
          }
        </style>
        <div id='scroller'>
          <div id='spacer'></div>
        </div>
        <div id='spacer'></div>">
    </iframe>
  )HTML");

  // RunIdleTasks to load the srcdoc iframe.
  RunIdleTasks();
  Compositor().BeginFrame();

  HTMLFrameOwnerElement* iframe =
      To<HTMLFrameOwnerElement>(GetDocument().getElementById("iframe"));
  auto* iframe_scrollable_area =
      iframe->contentDocument()->View()->LayoutViewport();
  const auto* iframe_scroll_node =
      ScrollNodeForScrollableArea(iframe_scrollable_area);
  ASSERT_TRUE(iframe_scroll_node);

  // The iframe itself is a composited scroller.
  EXPECT_EQ(
      0u, iframe_scrollable_area->GetNonCompositedMainThreadScrollingReasons());
  EXPECT_EQ(iframe_scroll_node->element_id,
            iframe_scrollable_area->GetScrollElementId());
  EXPECT_TRUE(RootCcLayer()
                  ->layer_tree_host()
                  ->property_trees()
                  ->scroll_tree.IsComposited(*iframe_scroll_node));

  // Ensure we have a compositor scroll node for the noncomposited subscroller.
  auto* child_scrollable_area = iframe->contentDocument()
                                    ->getElementById("scroller")
                                    ->GetLayoutBoxForScrolling()
                                    ->GetScrollableArea();
  const auto* child_scroll_node =
      ScrollNodeForScrollableArea(child_scrollable_area);
  ASSERT_TRUE(child_scroll_node);

  EXPECT_EQ(
      cc::MainThreadScrollingReason::kCantPaintScrollingBackgroundAndLCDText,
      child_scrollable_area->GetNonCompositedMainThreadScrollingReasons());
  EXPECT_EQ(child_scroll_node->element_id,
            child_scrollable_area->GetScrollElementId());

  EXPECT_FALSE(RootCcLayer()
                   ->layer_tree_host()
                   ->property_trees()
                   ->scroll_tree.IsComposited(*child_scroll_node));
}

// Similar to the above test, but for deeper nesting iframes to ensure we
// generate scroll nodes that are deeper than the main frame's children.
TEST_P(UnifiedScrollingSimTest, ScrollNodeForNestedEmbeddedScrollers) {
  SimRequest request("https://example.com/test.html", "text/html");
  SimRequest child_request_1("https://example.com/child1.html", "text/html");
  SimRequest child_request_2("https://example.com/child2.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    iframe {
      width: 300px;
      height: 300px;
      overflow: auto;
    }
    </style>
    <iframe id="child1" src="child1.html">
  )HTML");

  child_request_1.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    iframe {
      width: 300px;
      height: 300px;
      overflow: auto;
    }
    </style>
    <iframe id="child2" src="child2.html">
  )HTML");

  child_request_2.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
      #scroller {
        width: 200px;
        height: 200px;
        overflow: auto;
        position: absolute;
        top: 50px;
        background: white;
        box-shadow: 10px 10px black inset;
      }
      #spacer {
        width: 100%;
        height: 10000px;
      }
    </style>
    <div id='scroller'>
      <div id='spacer'></div>
    </div>
    <div id='spacer'></div>
  )HTML");

  RunIdleTasks();
  Compositor().BeginFrame();

  HTMLFrameOwnerElement* child_iframe_1 =
      To<HTMLFrameOwnerElement>(GetDocument().getElementById("child1"));

  HTMLFrameOwnerElement* child_iframe_2 = To<HTMLFrameOwnerElement>(
      child_iframe_1->contentDocument()->getElementById("child2"));

  // Ensure we have a compositor scroll node for the noncomposited subscroller
  // nested in the second iframe.
  auto* child_scrollable_area = child_iframe_2->contentDocument()
                                    ->getElementById("scroller")
                                    ->GetLayoutBoxForScrolling()
                                    ->GetScrollableArea();
  const auto* child_scroll_node =
      ScrollNodeForScrollableArea(child_scrollable_area);
  ASSERT_TRUE(child_scroll_node);

  EXPECT_EQ(
      cc::MainThreadScrollingReason::kCantPaintScrollingBackgroundAndLCDText,
      child_scrollable_area->GetNonCompositedMainThreadScrollingReasons());
  EXPECT_EQ(child_scroll_node->element_id,
            child_scrollable_area->GetScrollElementId());

  EXPECT_FALSE(RootCcLayer()
                   ->layer_tree_host()
                   ->property_trees()
                   ->scroll_tree.IsComposited(*child_scroll_node));
}

// Tests that the compositor gets a scroll node for opacity 0 noncomposited
// scrollers by loading a page with an opacity 0 scroller that has an inset
// box-shadow, and ensuring that scroller generates a compositor scroll node
// with the proper noncomposited reasons set. The test also ensures that there
// is no scroll node for a display:none scroller, as there is no scrollable
// area.
TEST_P(UnifiedScrollingSimTest, ScrollNodeForInvisibleNonCompositedScroller) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
    <!DOCTYPE html>
    <style>
    .noncomposited {
      width: 200px;
      height: 200px;
      overflow: auto;
      position: absolute;
      top: 300px;
      background: white;
      box-shadow: 10px 10px black inset;
    }
    #invisible {
      opacity: 0;
    }
    #displaynone {
      display: none;
    }
    #spacer {
      width: 100%;
      height: 10000px;
    }
    </style>
    <div id="invisible" class="noncomposited">
      <div id="spacer"></div>
    </div>
    <div id="displaynone" class="noncomposited">
      <div id="spacer"></div>
    </div>
  )HTML");
  Compositor().BeginFrame();

  // Ensure the opacity 0 noncomposited scrollable area generates a scroll node
  auto* invisible_scrollable_area = ScrollableAreaByDOMElementId("invisible");
  ASSERT_EQ(
      cc::MainThreadScrollingReason::kCantPaintScrollingBackgroundAndLCDText,
      invisible_scrollable_area->GetNonCompositedMainThreadScrollingReasons());

  const auto* invisible_scroll_node =
      ScrollNodeForScrollableArea(invisible_scrollable_area);
  ASSERT_TRUE(invisible_scroll_node);

  EXPECT_EQ(invisible_scroll_node->element_id,
            invisible_scrollable_area->GetScrollElementId());

  EXPECT_FALSE(RootCcLayer()
                   ->layer_tree_host()
                   ->property_trees()
                   ->scroll_tree.IsComposited(*invisible_scroll_node));

  // Ensure there's no scrollable area (and therefore no scroll node) for a
  // display none scroller.
  EXPECT_EQ(nullptr, ScrollableAreaByDOMElementId("displaynone"));
}

// Tests that the compositor gets a scroll node for scrollable input boxes,
// which are unique as they are not a composited scroller but also do not have
// NonCompositedMainThreadScrollingReasons.
TEST_P(UnifiedScrollingSimTest, ScrollNodeForInputBox) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
        input {
          width: 50px;
        }
      </style>
      <input id="textinput" type="text" value="some overflowing text"/>
  )HTML");
  Compositor().BeginFrame();

  auto* scrollable_area = ScrollableAreaByDOMElementId("textinput");
  ASSERT_EQ(0u, scrollable_area->GetNonCompositedMainThreadScrollingReasons());

  const auto* scroll_node = ScrollNodeForScrollableArea(scrollable_area);
  ASSERT_TRUE(scroll_node);

  EXPECT_EQ(scroll_node->element_id, scrollable_area->GetScrollElementId());
  EXPECT_FALSE(RootCcLayer()
                   ->layer_tree_host()
                   ->property_trees()
                   ->scroll_tree.IsComposited(*scroll_node));
}

class ScrollingSimTest : public SimTest,
                         public testing::WithParamInterface<bool> {
 public:
  ScrollingSimTest() : scroll_unification_enabled_(GetParam()) {}

  void SetUp() override {
    SimTest::SetUp();
    WebView().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
    WebView().MainFrameViewWidget()->Resize(gfx::Size(1000, 1000));
    WebView().MainFrameViewWidget()->UpdateAllLifecyclePhases(
        DocumentUpdateReason::kTest);
  }

  WebCoalescedInputEvent GenerateGestureEvent(WebInputEvent::Type type,
                                              int delta_x = 0,
                                              int delta_y = 0) {
    WebGestureEvent event(type, WebInputEvent::kNoModifiers,
                          WebInputEvent::GetStaticTimeStampForTests(),
                          WebGestureDevice::kTouchscreen);
    event.SetPositionInWidget(gfx::PointF(100, 100));
    if (type == WebInputEvent::Type::kGestureScrollUpdate) {
      event.data.scroll_update.delta_x = delta_x;
      event.data.scroll_update.delta_y = delta_y;
    } else if (type == WebInputEvent::Type::kGestureScrollBegin) {
      event.data.scroll_begin.delta_x_hint = delta_x;
      event.data.scroll_begin.delta_y_hint = delta_y;
    }
    return WebCoalescedInputEvent(event, ui::LatencyInfo());
  }

  unsigned NumObjectsNeedingLayout() {
    bool is_partial = false;
    unsigned num_objects_need_layout = 0;
    unsigned total_objects = 0;
    GetDocument().View()->CountObjectsNeedingLayout(num_objects_need_layout,
                                                    total_objects, is_partial);
    return num_objects_need_layout;
  }

 protected:
 protected:
  RuntimeEnabledFeaturesTestHelpers::ScopedScrollUnification
      scroll_unification_enabled_;
};

INSTANTIATE_TEST_SUITE_P(All, ScrollingSimTest, testing::Bool());

// Pre-scroll-unification, ensures that ScrollBegin and ScrollUpdate cause
// layout and ScrollEnd does not. Post unification, Blink will not handle these
// events but ensure that a unification main-thread-hit-test does cause layout.
TEST_P(ScrollingSimTest, ScrollLayoutTriggers) {
  SimRequest request("https://example.com/test.html", "text/html");
  LoadURL("https://example.com/test.html");
  request.Complete(R"HTML(
      <!DOCTYPE html>
      <style>
      #box {
        position: absolute;
      }
      body {
        height: 5000px;
      }
      </style>
      <div id='box'></div>
  )HTML");
  Compositor().BeginFrame();
  ASSERT_EQ(0u, NumObjectsNeedingLayout());

  Element* box = GetDocument().getElementById("box");
  if (RuntimeEnabledFeatures::ScrollUnificationEnabled()) {
    // Dirty the layout
    box->setAttribute(html_names::kStyleAttr, "height: 10px");
    GetDocument().UpdateStyleAndLayoutTree();
    ASSERT_NE(NumObjectsNeedingLayout(), 0u);

    // The hit test (which may be performed by a scroll begin) should cause a
    // layout to occur.
    WebView().MainFrameWidget()->HitTestResultAt(gfx::PointF(10, 10));
    EXPECT_EQ(NumObjectsNeedingLayout(), 0u);

  } else {
    // ScrollBegin should trigger a layout.
    {
      // Dirty the layout
      box->setAttribute(html_names::kStyleAttr, "height: 10px");
      GetDocument().UpdateStyleAndLayoutTree();
      ASSERT_NE(NumObjectsNeedingLayout(), 0u);

      WebView().MainFrameWidget()->HandleInputEvent(GenerateGestureEvent(
          WebInputEvent::Type::kGestureScrollBegin, 0, 10));
      EXPECT_EQ(NumObjectsNeedingLayout(), 0u);
    }

    // ScrollUpdate should trigger a layout.
    {
      // Dirty the layout
      box->setAttribute(html_names::kStyleAttr, "height: 11px");
      GetDocument().UpdateStyleAndLayoutTree();
      ASSERT_NE(NumObjectsNeedingLayout(), 0u);

      WebView().MainFrameWidget()->HandleInputEvent(GenerateGestureEvent(
          WebInputEvent::Type::kGestureScrollUpdate, 0, 10));
      EXPECT_EQ(NumObjectsNeedingLayout(), 0u);
    }

    // ScrollEnd shouldn't trigger a layout.
    {
      // Dirty the layout
      box->setAttribute(html_names::kStyleAttr, "height: 12px");
      GetDocument().UpdateStyleAndLayoutTree();
      ASSERT_NE(NumObjectsNeedingLayout(), 0u);

      WebView().MainFrameWidget()->HandleInputEvent(
          GenerateGestureEvent(WebInputEvent::Type::kGestureScrollEnd, 0, 0));
      EXPECT_NE(NumObjectsNeedingLayout(), 0u);
    }
  }
}

class ScrollingTestWithAcceleratedContext : public ScrollingTest {
 protected:
  void SetUp() override {
    auto factory = [](FakeGLES2Interface* gl, bool* gpu_compositing_disabled)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      *gpu_compositing_disabled = false;
      gl->SetIsContextLost(false);
      return std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        WTF::BindRepeating(factory, WTF::Unretained(&gl_)));
    ScrollingTest::SetUp();
  }

  void TearDown() override {
    SharedGpuContext::ResetForTesting();
    ScrollingTest::TearDown();
  }

 private:
  FakeGLES2Interface gl_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(ScrollingTestWithAcceleratedContext);

TEST_P(ScrollingTestWithAcceleratedContext, CanvasTouchActionRects) {
  LoadHTML(R"HTML(
    <canvas id="canvas" style="touch-action: none; will-change: transform;">
    <script>
      var canvas = document.getElementById("canvas");
      var ctx = canvas.getContext("2d");
      canvas.width = 400;
      canvas.height = 400;
      ctx.fillStyle = 'lightgrey';
      ctx.fillRect(0, 0, 400, 400);
    </script>
  )HTML");
  ForceFullCompositingUpdate();

  const auto* cc_layer = LayerByDOMElementId("canvas");
  cc::Region region = cc_layer->touch_action_region().GetRegionForTouchAction(
      TouchAction::kNone);
  EXPECT_EQ(region.bounds(), gfx::Rect(0, 0, 400, 400));
}

}  // namespace blink
