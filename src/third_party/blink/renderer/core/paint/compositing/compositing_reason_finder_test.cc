// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/compositing/compositing_reason_finder.h"

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/graphics_layer.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

class CompositingReasonFinderTest : public RenderingTest,
                                    public PaintTestConfigurations {
 public:
  CompositingReasonFinderTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

 protected:
  void SetUp() override {
    EnableCompositing();
    RenderingTest::SetUp();
  }

  void CheckCompositingReasonsForAnimation(bool supports_transform_animation);

  static CompositingReasons DirectReasonsForPaintProperties(
      const LayoutObject& object) {
    // We expect that the scrollable area's composited scrolling status has been
    // updated.
    return CompositingReasonFinder::DirectReasonsForPaintProperties(
        object,
        CompositingReasonFinder::DirectReasonsForPaintPropertiesExceptScrolling(
            object));
  }
};

#define EXPECT_REASONS(expect, actual)                        \
  EXPECT_EQ(expect, actual)                                   \
      << " expected: " << CompositingReason::ToString(expect) \
      << " actual: " << CompositingReason::ToString(actual)

INSTANTIATE_PAINT_TEST_SUITE_P(CompositingReasonFinderTest);

TEST_P(CompositingReasonFinderTest, CompositingReasonDependencies) {
  EXPECT_FALSE(CompositingReason::kComboAllDirectNonStyleDeterminedReasons &
               (~CompositingReason::kComboAllDirectReasons));
  EXPECT_REASONS(
      CompositingReason::kComboAllDirectReasons,
      CompositingReason::kComboAllDirectStyleDeterminedReasons |
          CompositingReason::kComboAllDirectNonStyleDeterminedReasons);
  EXPECT_FALSE(CompositingReason::kComboAllDirectNonStyleDeterminedReasons &
               CompositingReason::kComboAllStyleDeterminedReasons);
}

TEST_P(CompositingReasonFinderTest, PromoteTrivial3D) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(0)'></div>
  )HTML");

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    PaintLayer* paint_layer = GetPaintLayerByElementId("target");
    EXPECT_EQ(kPaintsIntoOwnBacking, paint_layer->GetCompositingState());
  }
  EXPECT_REASONS(
      CompositingReason::kTrivial3DTransform,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest, PromoteNonTrivial3D) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(1px)'></div>
  )HTML");

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    PaintLayer* paint_layer = GetPaintLayerByElementId("target");
    EXPECT_EQ(kPaintsIntoOwnBacking, paint_layer->GetCompositingState());
  }
  EXPECT_REASONS(
      CompositingReason::k3DTransform,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

class CompositingReasonFinderTestLowEndPlatform
    : public TestingPlatformSupport {
 public:
  bool IsLowEndDevice() override { return true; }
};

TEST_P(CompositingReasonFinderTest, DontPromoteTrivial3DWithLowEndDevice) {
  ScopedTestingPlatformSupport<CompositingReasonFinderTestLowEndPlatform>
      platform;
  SetBodyInnerHTML(R"HTML(
    <div id='target'
      style='width: 100px; height: 100px; transform: translateZ(0)'></div>
  )HTML");

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    PaintLayer* paint_layer = GetPaintLayerByElementId("target");
    EXPECT_EQ(kNotComposited, paint_layer->GetCompositingState());
  }
  EXPECT_REASONS(
      CompositingReason::kNone,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest, OnlyAnchoredStickyPositionPromoted) {
  SetBodyInnerHTML(R"HTML(
    <style>
    .scroller {contain: paint; width: 400px; height: 400px; overflow: auto;
    will-change: transform;}
    .sticky { position: sticky; width: 10px; height: 10px;}</style>
    <div class='scroller'>
      <div id='sticky-top' class='sticky' style='top: 0px;'></div>
      <div id='sticky-no-anchor' class='sticky'></div>
      <div style='height: 2000px;'></div>
    </div>
  )HTML");

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(kPaintsIntoOwnBacking,
              GetPaintLayerByElementId("sticky-top")->GetCompositingState());
    EXPECT_EQ(
        kNotComposited,
        GetPaintLayerByElementId("sticky-no-anchor")->GetCompositingState());
  }
  EXPECT_REASONS(CompositingReason::kStickyPosition,
                 DirectReasonsForPaintProperties(
                     *GetLayoutObjectByElementId("sticky-top")));
  EXPECT_REASONS(CompositingReason::kNone,
                 DirectReasonsForPaintProperties(
                     *GetLayoutObjectByElementId("sticky-no-anchor")));
}

TEST_P(CompositingReasonFinderTest, OnlyScrollingStickyPositionPromoted) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .scroller {
        width: 400px;
        height: 400px;
        overflow: auto;
        will-change: transform;
      }
      .sticky {
        position: sticky;
        top: 0;
        width: 10px;
        height: 10px;
      }
      .overflow-hidden {
        width: 400px;
        height: 400px;
        overflow: hidden;
        will-change: transform;
      }
    </style>
    <div class='scroller'>
      <div id='sticky-scrolling' class='sticky'></div>
      <div style='height: 2000px;'></div>
    </div>
    <div class='scroller'>
      <div id='sticky-no-scrolling' class='sticky'></div>
    </div>
    <div class='overflow-hidden'>
      <div id='overflow-hidden-scrolling' class='sticky'></div>
      <div style='height: 2000px;'></div>
    </div>
    <div class='overflow-hidden'>
      <div id='overflow-hidden-no-scrolling' class='sticky'></div>
    </div>
  )HTML");

  auto& sticky_scrolling =
      *To<LayoutBoxModelObject>(GetLayoutObjectByElementId("sticky-scrolling"));
  EXPECT_REASONS(
      CompositingReason::kStickyPosition,
      CompositingReasonFinder::CompositingReasonsForScrollDependentPosition(
          *sticky_scrolling.Layer()));

  auto& sticky_no_scrolling = *To<LayoutBoxModelObject>(
      GetLayoutObjectByElementId("sticky-no-scrolling"));
  EXPECT_REASONS(
      CompositingReason::kNone,
      CompositingReasonFinder::CompositingReasonsForScrollDependentPosition(
          *sticky_no_scrolling.Layer()));

  auto& overflow_hidden_scrolling = *To<LayoutBoxModelObject>(
      GetLayoutObjectByElementId("overflow-hidden-scrolling"));
  EXPECT_REASONS(
      CompositingReason::kStickyPosition,
      CompositingReasonFinder::CompositingReasonsForScrollDependentPosition(
          *overflow_hidden_scrolling.Layer()));

  auto& overflow_hidden_no_scrolling = *To<LayoutBoxModelObject>(
      GetLayoutObjectByElementId("overflow-hidden-no-scrolling"));
  EXPECT_REASONS(
      CompositingReason::kNone,
      CompositingReasonFinder::CompositingReasonsForScrollDependentPosition(
          *overflow_hidden_no_scrolling.Layer()));

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(kPaintsIntoOwnBacking,
              sticky_scrolling.Layer()->GetCompositingState());
    EXPECT_EQ(kNotComposited,
              sticky_no_scrolling.Layer()->GetCompositingState());
    EXPECT_EQ(kPaintsIntoOwnBacking,
              overflow_hidden_scrolling.Layer()->GetCompositingState());
    EXPECT_EQ(kNotComposited,
              overflow_hidden_no_scrolling.Layer()->GetCompositingState());
  }
}

void CompositingReasonFinderTest::CheckCompositingReasonsForAnimation(
    bool supports_transform_animation) {
  auto* object = GetLayoutObjectByElementId("target");
  scoped_refptr<ComputedStyle> style =
      GetDocument().GetStyleResolver().CreateComputedStyle();

  style->SetSubtreeWillChangeContents(false);
  style->SetHasCurrentTransformAnimation(false);
  style->SetHasCurrentOpacityAnimation(false);
  style->SetHasCurrentFilterAnimation(false);
  style->SetHasCurrentBackdropFilterAnimation(false);
  object->SetStyle(style);

  EXPECT_REASONS(
      CompositingReason::kNone,
      CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  CompositingReasons expected_compositing_reason_for_transform_animation =
      supports_transform_animation
          ? CompositingReason::kActiveTransformAnimation
          : CompositingReason::kNone;

  style->SetHasCurrentTransformAnimation(true);
  EXPECT_EQ(expected_compositing_reason_for_transform_animation,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  style->SetHasCurrentOpacityAnimation(true);
  EXPECT_EQ(expected_compositing_reason_for_transform_animation |
                CompositingReason::kActiveOpacityAnimation,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  style->SetHasCurrentFilterAnimation(true);
  EXPECT_EQ(expected_compositing_reason_for_transform_animation |
                CompositingReason::kActiveOpacityAnimation |
                CompositingReason::kActiveFilterAnimation,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));

  style->SetHasCurrentBackdropFilterAnimation(true);
  EXPECT_EQ(expected_compositing_reason_for_transform_animation |
                CompositingReason::kActiveOpacityAnimation |
                CompositingReason::kActiveFilterAnimation |
                CompositingReason::kActiveBackdropFilterAnimation,
            CompositingReasonFinder::CompositingReasonsForAnimation(*object));
}

TEST_P(CompositingReasonFinderTest, CompositingReasonsForAnimationBox) {
  SetBodyInnerHTML("<div id='target'>Target</div>");
  CheckCompositingReasonsForAnimation(/*supports_transform_animation*/ true);
}

TEST_P(CompositingReasonFinderTest, CompositingReasonsForAnimationInline) {
  SetBodyInnerHTML("<span id='target'>Target</span>");
  CheckCompositingReasonsForAnimation(/*supports_transform_animation*/ false);
}

TEST_P(CompositingReasonFinderTest, DontPromoteEmptyIframe) {
  GetDocument().GetFrame()->GetSettings()->SetPreferCompositingToLCDTextEnabled(
      true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <iframe style="width:0; height:0; border: 0;" srcdoc="<!DOCTYPE html>"></iframe>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* child_frame =
      To<LocalFrame>(GetDocument().GetFrame()->Tree().FirstChild());
  ASSERT_TRUE(child_frame);
  LocalFrameView* child_frame_view = child_frame->View();
  ASSERT_TRUE(child_frame_view);
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_EQ(
        kNotComposited,
        child_frame_view->GetLayoutView()->Layer()->GetCompositingState());
  }
  EXPECT_TRUE(child_frame_view->CanThrottleRendering());
}

TEST_P(CompositingReasonFinderTest, PromoteCrossOriginIframe) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <iframe id=iframe></iframe>
  )HTML");

  HTMLFrameOwnerElement* iframe =
      To<HTMLFrameOwnerElement>(GetDocument().getElementById("iframe"));
  ASSERT_TRUE(iframe);
  iframe->contentDocument()->OverrideIsInitialEmptyDocument();
  To<LocalFrame>(iframe->ContentFrame())->View()->BeginLifecycleUpdates();
  ASSERT_FALSE(iframe->ContentFrame()->IsCrossOriginToMainFrame());
  UpdateAllLifecyclePhasesForTest();
  LayoutView* iframe_layout_view =
      To<LocalFrame>(iframe->ContentFrame())->ContentLayoutObject();
  ASSERT_TRUE(iframe_layout_view);
  PaintLayer* iframe_layer = iframe_layout_view->Layer();
  ASSERT_TRUE(iframe_layer);
  EXPECT_REASONS(CompositingReason::kNone,
                 iframe_layer->DirectCompositingReasons());
  EXPECT_FALSE(iframe_layer->GetScrollableArea()->NeedsCompositedScrolling());
  EXPECT_REASONS(CompositingReason::kNone,
                 DirectReasonsForPaintProperties(*iframe_layout_view));

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <iframe id=iframe sandbox></iframe>
  )HTML");
  iframe = To<HTMLFrameOwnerElement>(GetDocument().getElementById("iframe"));
  iframe->contentDocument()->OverrideIsInitialEmptyDocument();
  To<LocalFrame>(iframe->ContentFrame())->View()->BeginLifecycleUpdates();
  UpdateAllLifecyclePhasesForTest();
  iframe_layout_view =
      To<LocalFrame>(iframe->ContentFrame())->ContentLayoutObject();
  iframe_layer = iframe_layout_view->Layer();
  ASSERT_TRUE(iframe_layer);
  ASSERT_TRUE(iframe->ContentFrame()->IsCrossOriginToMainFrame());
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_REASONS(CompositingReason::kIFrame,
                   iframe_layer->DirectCompositingReasons());
  }
  EXPECT_FALSE(iframe_layer->GetScrollableArea()->NeedsCompositedScrolling());
  EXPECT_REASONS(CompositingReason::kIFrame,
                 DirectReasonsForPaintProperties(*iframe_layout_view));

  // Make the iframe contents scrollable.
  iframe->contentDocument()->body()->setAttribute(html_names::kStyleAttr,
                                                  "height: 2000px");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(iframe_layer->GetScrollableArea()->NeedsCompositedScrolling());
  EXPECT_REASONS(
      CompositingReason::kIFrame | CompositingReason::kOverflowScrolling,
      DirectReasonsForPaintProperties(*iframe_layout_view));
}

TEST_P(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorAndPreserve3D) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px; position: relative }
    </style>
    <div style="backface-visibility: hidden; transform-style: preserve-3d">
      <div id=target></div>
    </div>
  )HTML");

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    PaintLayer* target_layer = GetPaintLayerByElementId("target");
    EXPECT_REASONS(CompositingReason::kBackfaceInvisibility3DAncestor,
                   target_layer->PotentialCompositingReasonsFromNonStyle());
    EXPECT_EQ(kPaintsIntoOwnBacking, target_layer->GetCompositingState());
  }

  EXPECT_REASONS(
      CompositingReason::kBackfaceInvisibility3DAncestor,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorAndPreserve3DWithInterveningDiv) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px }
    </style>
    <div style="backface-visibility: hidden; transform-style: preserve-3d">
      <div>
        <div id=target style="position: relative"></div>
      </div>
    </div>
  )HTML");

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    PaintLayer* target_layer = GetPaintLayerByElementId("target");
    EXPECT_REASONS(CompositingReason::kBackfaceInvisibility3DAncestor,
                   target_layer->PotentialCompositingReasonsFromNonStyle());
    EXPECT_EQ(kPaintsIntoOwnBacking, target_layer->GetCompositingState());
  }
  EXPECT_REASONS(
      CompositingReason::kBackfaceInvisibility3DAncestor,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorWithInterveningStackingDiv) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px }
    </style>
    <div style="backface-visibility: hidden; transform-style: preserve-3d">
      <div id=intermediate style="isolation: isolate">
        <div id=target style="position: relative"></div>
      </div>
    </div>
  )HTML");

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    PaintLayer* intermediate_layer = GetPaintLayerByElementId("intermediate");
    PaintLayer* target_layer = GetPaintLayerByElementId("target");

    EXPECT_REASONS(
        CompositingReason::kBackfaceInvisibility3DAncestor,
        intermediate_layer->PotentialCompositingReasonsFromNonStyle());
    EXPECT_EQ(kPaintsIntoOwnBacking, intermediate_layer->GetCompositingState());

    EXPECT_REASONS(CompositingReason::kNone,
                   target_layer->PotentialCompositingReasonsFromNonStyle());
    EXPECT_NE(kPaintsIntoOwnBacking, target_layer->GetCompositingState());
  }

  EXPECT_REASONS(CompositingReason::kBackfaceInvisibility3DAncestor,
                 DirectReasonsForPaintProperties(
                     *GetLayoutObjectByElementId("intermediate")));
  EXPECT_REASONS(
      CompositingReason::kNone,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest,
       CompositeWithBackfaceVisibilityAncestorAndFlattening) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px; position: relative }
    </style>
    <div style="backface-visibility: hidden;">
      <div id=target></div>
    </div>
  )HTML");

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    PaintLayer* target_layer = GetPaintLayerByElementId("target");

    EXPECT_REASONS(CompositingReason::kNone,
                   target_layer->PotentialCompositingReasonsFromNonStyle());
    EXPECT_NE(kPaintsIntoOwnBacking, target_layer->GetCompositingState());
  }
  EXPECT_REASONS(
      CompositingReason::kNone,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest, CompositeWithBackfaceVisibility) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);

  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      div { width: 100px; height: 100px; position: relative }
    </style>
    <div id=target style="backface-visibility: hidden;">
      <div></div>
    </div>
  )HTML");

  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    PaintLayer* target_layer = GetPaintLayerByElementId("target");

    EXPECT_REASONS(CompositingReason::kNone,
                   target_layer->PotentialCompositingReasonsFromNonStyle());
    EXPECT_EQ(kPaintsIntoOwnBacking, target_layer->GetCompositingState());
  }
  EXPECT_REASONS(
      CompositingReason::kNone,
      DirectReasonsForPaintProperties(*GetLayoutObjectByElementId("target")));
}

TEST_P(CompositingReasonFinderTest, CompositedSVGText) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <text id="text" style="will-change: opacity">Text</text>
    </svg>
  )HTML");

  auto* svg_text = GetLayoutObjectByElementId("text");
  EXPECT_EQ(CompositingReason::kWillChangeOpacity,
            DirectReasonsForPaintProperties(*svg_text));
  auto* text = svg_text->SlowFirstChild();
  ASSERT_TRUE(text->IsText());
  EXPECT_REASONS(CompositingReason::kNone,
                 DirectReasonsForPaintProperties(*text));
}

TEST_P(CompositingReasonFinderTest, NotSupportedTransformAnimationsOnSVG) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { animation: transformKeyframes 1s infinite; }
      @keyframes transformKeyframes {
        0% { transform: rotate(-5deg); }
        100% { transform: rotate(5deg); }
      }
    </style>
    <svg>
      <defs id="defs" />
      <text id="text">text content
        <tspan id="tspan">tspan content</tspan>
      </text>
    </svg>
  )HTML");

  auto* defs = GetLayoutObjectByElementId("defs");
  EXPECT_REASONS(CompositingReason::kNone,
                 DirectReasonsForPaintProperties(*defs));

  auto* text = GetLayoutObjectByElementId("text");
  EXPECT_REASONS(CompositingReason::kActiveTransformAnimation,
                 DirectReasonsForPaintProperties(*text));

  auto* text_content = text->SlowFirstChild();
  ASSERT_TRUE(text_content->IsText());
  EXPECT_EQ(CompositingReason::kNone,
            DirectReasonsForPaintProperties(*text_content));

  auto* tspan = GetLayoutObjectByElementId("tspan");
  EXPECT_REASONS(CompositingReason::kNone,
                 DirectReasonsForPaintProperties(*tspan));

  auto* tspan_content = tspan->SlowFirstChild();
  ASSERT_TRUE(tspan_content->IsText());
  EXPECT_EQ(CompositingReason::kNone,
            DirectReasonsForPaintProperties(*tspan_content));
}

}  // namespace blink
