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
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/core/paint/link_highlight_impl.h"

#include <memory>

#include "cc/layers/picture_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/events/web_input_event_conversion.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/link_highlights.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/animation/compositor_animation_timeline.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

class LinkHighlightImplTest : public testing::Test,
                              public PaintTestConfigurations {
 protected:
  GestureEventWithHitTestResults GetTargetedEvent(
      WebGestureEvent& touch_event) {
    WebGestureEvent scaled_event = TransformWebGestureEvent(
        web_view_helper_.GetWebView()->MainFrameImpl()->GetFrameView(),
        touch_event);
    return web_view_helper_.GetWebView()
        ->GetPage()
        ->DeprecatedLocalMainFrame()
        ->GetEventHandler()
        .TargetGestureEvent(scaled_event, true);
  }

  void SetUp() override {
    WebURL url = url_test_helpers::RegisterMockedURLLoadFromBase(
        WebString::FromUTF8("http://www.test.com/"), test::CoreTestDataPath(),
        WebString::FromUTF8("test_touch_link_highlight.html"));
    web_view_helper_.InitializeAndLoad(url.GetString().Utf8());
  }

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();

    // Ensure we fully clean up while scoped settings are enabled. Without this,
    // garbage collection would occur after ScopedBlinkGenPropertyTreesForTest
    // is out of scope, so the settings would not apply in some destructors.
    web_view_helper_.Reset();
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

  size_t ContentLayerCount() {
    // paint_artifact_compositor()->EnableExtraDataForTesting() should be called
    // before using this function.
    DCHECK(paint_artifact_compositor()->GetExtraDataForTesting());
    return paint_artifact_compositor()
        ->GetExtraDataForTesting()
        ->content_layers.size();
  }

  PaintArtifactCompositor* paint_artifact_compositor() {
    auto* local_frame_view = web_view_helper_.LocalMainFrame()->GetFrameView();
    return local_frame_view->GetPaintArtifactCompositorForTesting();
  }

  void UpdateAllLifecyclePhases() {
    web_view_helper_.GetWebView()->MainFrameWidget()->UpdateAllLifecyclePhases(
        WebWidget::LifecycleUpdateReason::kTest);
  }

  frame_test_helpers::WebViewHelper web_view_helper_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(LinkHighlightImplTest);

TEST_P(LinkHighlightImplTest, verifyWebViewImplIntegration) {
  WebViewImpl* web_view_impl = web_view_helper_.GetWebView();
  int page_width = 640;
  int page_height = 480;
  web_view_impl->MainFrameWidget()->Resize(WebSize(page_width, page_height));
  UpdateAllLifecyclePhases();

  WebGestureEvent touch_event(WebInputEvent::kGestureShowPress,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests(),
                              WebGestureDevice::kTouchscreen);

  // The coordinates below are linked to absolute positions in the referenced
  // .html file.
  touch_event.SetPositionInWidget(WebFloatPoint(20, 20));

  ASSERT_TRUE(web_view_impl->BestTapNode(GetTargetedEvent(touch_event)));

  touch_event.SetPositionInWidget(WebFloatPoint(20, 40));
  EXPECT_FALSE(web_view_impl->BestTapNode(GetTargetedEvent(touch_event)));

  touch_event.SetPositionInWidget(WebFloatPoint(20, 20));
  // Shouldn't crash.
  web_view_impl->EnableTapHighlightAtPoint(GetTargetedEvent(touch_event));

  const auto& highlights =
      web_view_impl->GetPage()->GetLinkHighlights().link_highlights_;
  EXPECT_TRUE(highlights.at(0));
  EXPECT_EQ(1u, highlights.at(0)->FragmentCountForTesting());
  EXPECT_TRUE(highlights.at(0)->LayerForTesting(0));

  // Find a target inside a scrollable div
  touch_event.SetPositionInWidget(WebFloatPoint(20, 100));
  web_view_impl->EnableTapHighlightAtPoint(GetTargetedEvent(touch_event));
  ASSERT_TRUE(highlights.at(0));

  // Enesure the timeline was added to a host.
  EXPECT_TRUE(!!web_view_impl->GetPage()
                    ->GetLinkHighlights()
                    .timeline_->GetAnimationTimeline()
                    ->animation_host());

  // Don't highlight if no "hand cursor"
  touch_event.SetPositionInWidget(
      WebFloatPoint(20, 220));  // An A-link with cross-hair cursor.
  web_view_impl->EnableTapHighlightAtPoint(GetTargetedEvent(touch_event));
  ASSERT_EQ(0U, highlights.size());

  touch_event.SetPositionInWidget(WebFloatPoint(20, 260));  // A text input box.
  web_view_impl->EnableTapHighlightAtPoint(GetTargetedEvent(touch_event));
  ASSERT_EQ(0U, highlights.size());
}

TEST_P(LinkHighlightImplTest, resetDuringNodeRemoval) {
  WebViewImpl* web_view_impl = web_view_helper_.GetWebView();

  int page_width = 640;
  int page_height = 480;
  web_view_impl->MainFrameWidget()->Resize(WebSize(page_width, page_height));
  UpdateAllLifecyclePhases();

  WebGestureEvent touch_event(WebInputEvent::kGestureShowPress,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests(),
                              WebGestureDevice::kTouchscreen);
  touch_event.SetPositionInWidget(WebFloatPoint(20, 20));

  GestureEventWithHitTestResults targeted_event = GetTargetedEvent(touch_event);
  Node* touch_node = web_view_impl->BestTapNode(targeted_event);
  ASSERT_TRUE(touch_node);

  web_view_impl->EnableTapHighlightAtPoint(targeted_event);
  const auto& highlights = web_view_impl->GetPage()->GetLinkHighlights();
  ASSERT_EQ(1u, highlights.link_highlights_.size());
  ASSERT_TRUE(highlights.link_highlights_.at(0));
  EXPECT_EQ(touch_node, highlights.link_highlights_.at(0)->GetNode());

  GraphicsLayer* highlight_layer;
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    highlight_layer =
        highlights.link_highlights_.at(0)->CurrentGraphicsLayerForTesting();
    ASSERT_TRUE(highlight_layer);
    EXPECT_TRUE(highlight_layer->GetLinkHighlights().at(0));
  }

  touch_node->remove(IGNORE_EXCEPTION_FOR_TESTING);
  UpdateAllLifecyclePhases();

  ASSERT_EQ(1u, highlights.link_highlights_.size());
  ASSERT_TRUE(highlights.link_highlights_.at(0));
  EXPECT_FALSE(highlights.link_highlights_.at(0)->GetNode());

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(0U, highlight_layer->GetLinkHighlights().size());
    EXPECT_FALSE(
        highlights.link_highlights_.at(0)->CurrentGraphicsLayerForTesting());
  }
}

// A lifetime test: delete LayerTreeView while running LinkHighlights.
TEST_P(LinkHighlightImplTest, resetLayerTreeView) {
  WebViewImpl* web_view_impl = web_view_helper_.GetWebView();

  int page_width = 640;
  int page_height = 480;
  web_view_impl->MainFrameWidget()->Resize(WebSize(page_width, page_height));
  UpdateAllLifecyclePhases();

  WebGestureEvent touch_event(WebInputEvent::kGestureShowPress,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests(),
                              WebGestureDevice::kTouchscreen);
  touch_event.SetPositionInWidget(WebFloatPoint(20, 20));

  GestureEventWithHitTestResults targeted_event = GetTargetedEvent(touch_event);
  Node* touch_node = web_view_impl->BestTapNode(targeted_event);
  ASSERT_TRUE(touch_node);

  web_view_impl->EnableTapHighlightAtPoint(targeted_event);
  const auto& highlights =
      web_view_impl->GetPage()->GetLinkHighlights().link_highlights_;
  ASSERT_TRUE(highlights.at(0));

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    GraphicsLayer* highlight_layer =
        highlights.at(0)->CurrentGraphicsLayerForTesting();
    ASSERT_TRUE(highlight_layer);
    EXPECT_TRUE(highlight_layer->GetLinkHighlights().at(0));
  }
}

TEST_P(LinkHighlightImplTest, HighlightInvalidation) {
  // This test requires GraphicsLayers which are not used in
  // CompositeAfterPaint.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  WebViewImpl* web_view_impl = web_view_helper_.GetWebView();
  web_view_impl->MainFrameWidget()->Resize(WebSize(640, 480));
  UpdateAllLifecyclePhases();

  WebGestureEvent touch_event(WebInputEvent::kGestureShowPress,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests(),
                              WebGestureDevice::kTouchscreen);
  touch_event.SetPositionInWidget(WebFloatPoint(20, 20));
  GestureEventWithHitTestResults targeted_event = GetTargetedEvent(touch_event);
  auto* touch_element = To<Element>(web_view_impl->BestTapNode(targeted_event));
  web_view_impl->EnableTapHighlightAtPoint(targeted_event);

  web_view_helper_.LocalMainFrame()
      ->GetFrameView()
      ->SetTracksPaintInvalidations(true);

  // Change the touched element's height to 12px.
  auto& style = touch_element->getAttribute(html_names::kStyleAttr);
  StringBuilder new_style;
  new_style.Append(style.GetString());
  new_style.Append("height: 12px;");
  touch_element->setAttribute(html_names::kStyleAttr,
                              new_style.ToAtomicString());
  UpdateAllLifecyclePhases();

  const auto& highlights =
      web_view_impl->GetPage()->GetLinkHighlights().link_highlights_;
  auto* highlight_layer = highlights.at(0)->CurrentGraphicsLayerForTesting();
  const auto* tracking = highlight_layer->GetRasterInvalidationTracking();
  // The invalidation rect should fully cover the layer.
  EXPECT_EQ(tracking->Invalidations().back().rect, IntRect(0, 0, 200, 12));
}

TEST_P(LinkHighlightImplTest, HighlightLayerEffectNode) {
  // This is testing the blink->cc layer integration.
  if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled() &&
      !RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  bool was_running_web_test = WebTestSupport::IsRunningWebTest();
  WebTestSupport::SetIsRunningWebTest(false);
  int page_width = 640;
  int page_height = 480;
  WebViewImpl* web_view_impl = web_view_helper_.GetWebView();
  web_view_impl->MainFrameWidget()->Resize(WebSize(page_width, page_height));

  paint_artifact_compositor()->EnableExtraDataForTesting();
  UpdateAllLifecyclePhases();
  size_t layer_count_before_highlight = ContentLayerCount();

  WebGestureEvent touch_event(WebInputEvent::kGestureShowPress,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests(),
                              WebGestureDevice::kTouchscreen);
  touch_event.SetPositionInWidget(WebFloatPoint(20, 20));

  GestureEventWithHitTestResults targeted_event = GetTargetedEvent(touch_event);
  Node* touch_node = web_view_impl->BestTapNode(targeted_event);
  ASSERT_TRUE(touch_node);

  web_view_impl->EnableTapHighlightAtPoint(targeted_event);
  // The highlight should create one additional layer.
  EXPECT_EQ(layer_count_before_highlight + 1, ContentLayerCount());

  auto& highlights = web_view_impl->GetPage()->GetLinkHighlights();
  auto* highlight = highlights.link_highlights_.at(0).get();
  ASSERT_TRUE(highlight);

  // Check that the link highlight cc layer has a cc effect property tree node.
  EXPECT_EQ(1u, highlight->FragmentCountForTesting());
  auto* layer = highlight->LayerForTesting(0);
  // We don't set layer's element id.
  EXPECT_EQ(cc::ElementId(), layer->element_id());
  auto effect_tree_index = layer->effect_tree_index();
  auto* property_trees = layer->layer_tree_host()->property_trees();
  EXPECT_EQ(
      effect_tree_index,
      property_trees
          ->element_id_to_effect_node_index[highlight->ElementIdForTesting()]);
  // The link highlight cc effect node should correspond to the blink effect
  // node.
  EXPECT_EQ(highlight->Effect().GetCompositorElementId(),
            highlight->ElementIdForTesting());

  // Initially the highlight node has full opacity as it is expected to remain
  // visible until the user completes a tap. See https://crbug.com/974631
  EXPECT_EQ(1.f, highlight->Effect().Opacity());
  EXPECT_TRUE(highlight->Effect().HasActiveOpacityAnimation());

  // After starting the highlight animation the effect node's opacity should
  // be 0.f as it will be overridden bt the animation but may become visible
  // before the animation is destructed. See https://crbug.com/974160
  highlights.StartHighlightAnimationIfNeeded();
  EXPECT_EQ(0.f, highlight->Effect().Opacity());
  EXPECT_TRUE(highlight->Effect().HasActiveOpacityAnimation());

  touch_node->remove(IGNORE_EXCEPTION_FOR_TESTING);
  UpdateAllLifecyclePhases();
  // Removing the highlight layer should drop the cc layer count by one.
  EXPECT_EQ(layer_count_before_highlight, ContentLayerCount());

  WebTestSupport::SetIsRunningWebTest(was_running_web_test);
}

TEST_P(LinkHighlightImplTest, MultiColumn) {
  // This is testing the blink->cc layer integration.
  if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled() &&
      !RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  int page_width = 640;
  int page_height = 480;
  WebViewImpl* web_view_impl = web_view_helper_.GetWebView();
  web_view_impl->MainFrameWidget()->Resize(WebSize(page_width, page_height));
  UpdateAllLifecyclePhases();

  paint_artifact_compositor()->EnableExtraDataForTesting();
  UpdateAllLifecyclePhases();
  size_t layer_count_before_highlight = ContentLayerCount();

  WebGestureEvent touch_event(WebInputEvent::kGestureShowPress,
                              WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests(),
                              WebGestureDevice::kTouchscreen);
  // This will touch the link under multicol.
  touch_event.SetPositionInWidget(WebFloatPoint(20, 300));

  GestureEventWithHitTestResults targeted_event = GetTargetedEvent(touch_event);
  Node* touch_node = web_view_impl->BestTapNode(targeted_event);
  ASSERT_TRUE(touch_node);

  web_view_impl->EnableTapHighlightAtPoint(targeted_event);

  const auto& highlights =
      web_view_impl->GetPage()->GetLinkHighlights().link_highlights_;
  EXPECT_EQ(1u, highlights.size());
  const auto* highlight = highlights.at(0).get();
  ASSERT_TRUE(highlight);

  // The link highlight cc effect node should correspond to the blink effect
  // node.
  const auto& effect = highlight->Effect();
  EXPECT_EQ(effect.GetCompositorElementId(), highlight->ElementIdForTesting());
  EXPECT_TRUE(effect.HasActiveOpacityAnimation());

  const auto& first_fragment = touch_node->GetLayoutObject()->FirstFragment();
  const auto* second_fragment = first_fragment.NextFragment();
  ASSERT_TRUE(second_fragment);
  EXPECT_FALSE(second_fragment->NextFragment());

  auto check_layer = [&](const cc::PictureLayer* layer) {
    ASSERT_TRUE(layer);
    // We don't set layer's element id.
    EXPECT_EQ(cc::ElementId(), layer->element_id());
    auto effect_tree_index = layer->effect_tree_index();
    auto* property_trees = layer->layer_tree_host()->property_trees();
    EXPECT_EQ(effect_tree_index, property_trees->element_id_to_effect_node_index
                                     [highlight->ElementIdForTesting()]);
  };

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // The highlight should create 2 additional layer, each for each fragment.
    EXPECT_EQ(layer_count_before_highlight + 2, ContentLayerCount());
    EXPECT_EQ(2u, highlight->FragmentCountForTesting());
    check_layer(highlight->LayerForTesting(0));
    check_layer(highlight->LayerForTesting(1));
  } else {
    // The highlight should create 1 additional layer covering both fragments.
    EXPECT_EQ(layer_count_before_highlight + 1, ContentLayerCount());
    EXPECT_EQ(1u, highlight->FragmentCountForTesting());
    check_layer(highlight->LayerForTesting(0));
  }

  touch_node->remove(IGNORE_EXCEPTION_FOR_TESTING);
  UpdateAllLifecyclePhases();
  // Removing the highlight layer should drop the cc layers for highlights.
  EXPECT_EQ(layer_count_before_highlight, ContentLayerCount());
}

}  // namespace blink
