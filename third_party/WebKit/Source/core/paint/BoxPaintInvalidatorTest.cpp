// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BoxPaintInvalidator.h"

#include "core/HTMLNames.h"
#include "core/frame/FrameTestHelpers.h"
#include "core/frame/LocalFrameView.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/RasterInvalidationTracking.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"

namespace blink {

class BoxPaintInvalidatorTest : public ::testing::WithParamInterface<bool>,
                                private ScopedRootLayerScrollingForTest,
                                public RenderingTest {
 public:
  BoxPaintInvalidatorTest()
      : ScopedRootLayerScrollingForTest(GetParam()),
        RenderingTest(SingleChildLocalFrameClient::Create()) {}

 protected:
  const RasterInvalidationTracking* GetRasterInvalidationTracking() const {
    // TODO(wangxianzhu): Test SPv2.
    return GetLayoutView()
        .Layer()
        ->GraphicsLayerBacking()
        ->GetRasterInvalidationTracking();
  }

  PaintInvalidationReason ComputePaintInvalidationReason(
      const LayoutBox& box,
      const LayoutRect& old_visual_rect,
      const LayoutPoint& old_location,
      const LayoutPoint& new_location) {
    PaintInvalidatorContext context;
    context.old_visual_rect = old_visual_rect;
    context.old_location = old_location;
    context.new_location = new_location;
    return BoxPaintInvalidator(box, context).ComputePaintInvalidationReason();
  }

  // This applies when the target is set to meet conditions that we should do
  // full paint invalidation instead of incremental invalidation on geometry
  // change.
  void ExpectFullPaintInvalidationOnGeometryChange(const char* test_title) {
    SCOPED_TRACE(test_title);

    GetDocument().View()->UpdateAllLifecyclePhases();
    auto& target = *GetDocument().getElementById("target");
    const auto& box = *ToLayoutBox(target.GetLayoutObject());
    LayoutRect visual_rect = box.VisualRect();

    // No geometry change.
    EXPECT_EQ(
        PaintInvalidationReason::kNone,
        ComputePaintInvalidationReason(box, visual_rect, visual_rect.Location(),
                                       visual_rect.Location()));

    target.setAttribute(
        HTMLNames::styleAttr,
        target.getAttribute(HTMLNames::styleAttr) + "; width: 200px");
    GetDocument().View()->UpdateLifecycleToLayoutClean();
    // Simulate that PaintInvalidator updates visual rect.
    box.GetMutableForPainting().SetVisualRect(
        LayoutRect(visual_rect.Location(), box.Size()));

    EXPECT_EQ(
        PaintInvalidationReason::kGeometry,
        ComputePaintInvalidationReason(box, visual_rect, visual_rect.Location(),
                                       box.VisualRect().Location()));
  }

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    GetDocument().SetCompatibilityMode(Document::kNoQuirksMode);
    EnableCompositing();
    SetBodyInnerHTML(
        "<style>"
        "  body {"
        "    margin: 0;"
        "    height: 0;"
        "  }"
        "  ::-webkit-scrollbar { display: none }"
        "  #target {"
        "    width: 50px;"
        "    height: 100px;"
        "    transform-origin: 0 0;"
        "  }"
        "  .border {"
        "    border-width: 20px 10px;"
        "    border-style: solid;"
        "    border-color: red;"
        "  }"
        "  .local-background {"
        "    background-attachment: local;"
        "    overflow: scroll;"
        "  }"
        "  .gradient {"
        "    background-image: linear-gradient(blue, yellow)"
        "  }"
        "  .transform {"
        "    transform: scale(2);"
        "  }"
        "</style>"
        "<div id='target' class='border'></div>");
  }
};

INSTANTIATE_TEST_CASE_P(All, BoxPaintInvalidatorTest, ::testing::Bool());

TEST_P(BoxPaintInvalidatorTest, SlowMapToVisualRectInAncestorSpaceLayoutView) {
  SetBodyInnerHTML(
      "<!doctype html>"
      "<style>"
      "#parent {"
      "    display: inline-block;"
      "    width: 300px;"
      "    height: 300px;"
      "    margin-top: 200px;"
      "    filter: blur(3px);"  // Forces the slow path in
                                // SlowMapToVisualRectInAncestorSpace.
      "    border: 1px solid rebeccapurple;"
      "}"
      "</style>"
      "<div id=parent>"
      "  <iframe id='target' src='data:text/html,<body style='background: "
      "blue;'></body>'></iframe>"
      "</div>");

  auto& target = *GetDocument().getElementById("target");
  EXPECT_RECT_EQ(IntRect(2, 202, 318, 168),
                 EnclosingIntRect(ToHTMLFrameOwnerElement(target)
                                      .contentDocument()
                                      ->GetLayoutView()
                                      ->VisualRect()));
}

TEST_P(BoxPaintInvalidatorTest, ComputePaintInvalidationReasonPaintingNothing) {
  ScopedSlimmingPaintV2ForTest spv2(true);

  auto& target = *GetDocument().getElementById("target");
  auto& box = *ToLayoutBox(target.GetLayoutObject());
  // Remove border.
  target.setAttribute(HTMLNames::classAttr, "");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_TRUE(box.PaintedOutputOfObjectHasNoEffectRegardlessOfSize());
  LayoutRect visual_rect = box.VisualRect();
  EXPECT_EQ(LayoutRect(0, 0, 50, 100), visual_rect);

  // No geometry change.
  EXPECT_EQ(
      PaintInvalidationReason::kNone,
      ComputePaintInvalidationReason(box, visual_rect, visual_rect.Location(),
                                     visual_rect.Location()));

  // Location change.
  EXPECT_EQ(PaintInvalidationReason::kNone,
            ComputePaintInvalidationReason(
                box, visual_rect, visual_rect.Location() + LayoutSize(10, 20),
                visual_rect.Location()));

  // Visual rect size change.
  LayoutRect old_visual_rect = visual_rect;
  LayoutRect new_visual_rect = box.VisualRect();
  target.setAttribute(HTMLNames::styleAttr, "width: 200px");
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  // Simulate that PaintInvalidator updates visual rect.
  box.GetMutableForPainting().SetVisualRect(
      LayoutRect(visual_rect.Location(), box.Size()));

  EXPECT_EQ(PaintInvalidationReason::kNone,
            ComputePaintInvalidationReason(box, old_visual_rect,
                                           old_visual_rect.Location(),
                                           new_visual_rect.Location()));
}

TEST_P(BoxPaintInvalidatorTest, ComputePaintInvalidationReasonBasic) {
  ScopedSlimmingPaintV2ForTest spv2(true);

  auto& target = *GetDocument().getElementById("target");
  auto& box = *ToLayoutBox(target.GetLayoutObject());
  // Remove border.
  target.setAttribute(HTMLNames::classAttr, "");
  target.setAttribute(HTMLNames::styleAttr, "background: blue");
  GetDocument().View()->UpdateAllLifecyclePhases();

  LayoutRect visual_rect = box.VisualRect();
  EXPECT_EQ(LayoutRect(0, 0, 50, 100), visual_rect);

  // No geometry change.
  EXPECT_EQ(
      PaintInvalidationReason::kNone,
      ComputePaintInvalidationReason(box, visual_rect, visual_rect.Location(),
                                     visual_rect.Location()));

  // Location change.
  EXPECT_EQ(PaintInvalidationReason::kGeometry,
            ComputePaintInvalidationReason(
                box, visual_rect, visual_rect.Location() + LayoutSize(10, 20),
                visual_rect.Location()));

  // Visual rect size change.
  LayoutRect old_visual_rect = visual_rect;
  LayoutRect new_visual_rect = box.VisualRect();
  target.setAttribute(HTMLNames::styleAttr, "background: blue; width: 200px");
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  // Simulate that PaintInvalidator updates visual rect.
  box.GetMutableForPainting().SetVisualRect(
      LayoutRect(visual_rect.Location(), box.Size()));
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            ComputePaintInvalidationReason(box, old_visual_rect,
                                           old_visual_rect.Location(),
                                           new_visual_rect.Location()));

  // Visual rect size change, with location in backing different from location
  // of visual rect.
  LayoutPoint fake_location = visual_rect.Location() + LayoutSize(10, 20);
  EXPECT_EQ(PaintInvalidationReason::kGeometry,
            ComputePaintInvalidationReason(box, old_visual_rect, fake_location,
                                           fake_location));

  // Should use the existing full paint invalidation reason regardless of
  // geometry change.
  box.SetShouldDoFullPaintInvalidation(PaintInvalidationReason::kStyle);
  EXPECT_EQ(
      PaintInvalidationReason::kStyle,
      ComputePaintInvalidationReason(box, visual_rect, visual_rect.Location(),
                                     visual_rect.Location()));
  EXPECT_EQ(PaintInvalidationReason::kStyle,
            ComputePaintInvalidationReason(
                box, visual_rect, visual_rect.Location() + LayoutSize(10, 20),
                visual_rect.Location()));
}

TEST_P(BoxPaintInvalidatorTest, ComputePaintInvalidationReasonOtherCases) {
  ScopedSlimmingPaintV2ForTest spv2(true);
  auto& target = *GetDocument().getElementById("target");

  // The target initially has border.
  ExpectFullPaintInvalidationOnGeometryChange("With border");

  // Clear border.
  target.setAttribute(HTMLNames::classAttr, "");
  target.setAttribute(HTMLNames::styleAttr, "border-radius: 5px");
  ExpectFullPaintInvalidationOnGeometryChange("With border-radius");

  target.setAttribute(HTMLNames::styleAttr, "-webkit-mask: url(#)");
  ExpectFullPaintInvalidationOnGeometryChange("With mask");

  target.setAttribute(HTMLNames::styleAttr, "filter: blur(5px)");
  ExpectFullPaintInvalidationOnGeometryChange("With filter");

  target.setAttribute(HTMLNames::styleAttr, "outline: 2px solid blue");
  ExpectFullPaintInvalidationOnGeometryChange("With outline");

  target.setAttribute(HTMLNames::styleAttr, "box-shadow: inset 3px 2px");
  ExpectFullPaintInvalidationOnGeometryChange("With box-shadow");

  target.setAttribute(HTMLNames::styleAttr, "-webkit-appearance: button");
  ExpectFullPaintInvalidationOnGeometryChange("With appearance");
}

TEST_P(BoxPaintInvalidatorTest, IncrementalInvalidationExpand) {
  GetDocument().View()->SetTracksPaintInvalidations(true);
  Element* target = GetDocument().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "width: 100px; height: 200px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  const auto& raster_invalidations =
      GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(2u, raster_invalidations.size());
  EXPECT_EQ(IntRect(60, 0, 60, 240), raster_invalidations[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            raster_invalidations[0].reason);
  EXPECT_EQ(IntRect(0, 120, 120, 120), raster_invalidations[1].rect);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            raster_invalidations[1].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, IncrementalInvalidationShrink) {
  GetDocument().View()->SetTracksPaintInvalidations(true);
  Element* target = GetDocument().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "width: 20px; height: 80px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  const auto& raster_invalidations =
      GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(2u, raster_invalidations.size());
  EXPECT_EQ(IntRect(30, 0, 40, 140), raster_invalidations[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            raster_invalidations[0].reason);
  EXPECT_EQ(IntRect(0, 100, 70, 40), raster_invalidations[1].rect);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            raster_invalidations[1].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, IncrementalInvalidationMixed) {
  GetDocument().View()->SetTracksPaintInvalidations(true);
  Element* target = GetDocument().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "width: 100px; height: 80px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  const auto& raster_invalidations =
      GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(2u, raster_invalidations.size());
  EXPECT_EQ(IntRect(60, 0, 60, 120), raster_invalidations[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            raster_invalidations[0].reason);
  EXPECT_EQ(IntRect(0, 100, 70, 40), raster_invalidations[1].rect);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            raster_invalidations[1].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, SubpixelVisualRectChagne) {
  Element* target = GetDocument().getElementById("target");

  GetDocument().View()->SetTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "width: 100.6px; height: 70.3px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  const auto* raster_invalidations =
      &GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(2u, raster_invalidations->size());
  EXPECT_EQ(IntRect(60, 0, 61, 111), (*raster_invalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            (*raster_invalidations)[0].reason);
  EXPECT_EQ(IntRect(0, 90, 70, 50), (*raster_invalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            (*raster_invalidations)[1].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);

  GetDocument().View()->SetTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "width: 50px; height: 100px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  raster_invalidations = &GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(2u, raster_invalidations->size());
  EXPECT_EQ(IntRect(60, 0, 61, 111), (*raster_invalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            (*raster_invalidations)[0].reason);
  EXPECT_EQ(IntRect(0, 90, 70, 50), (*raster_invalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            (*raster_invalidations)[1].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, SubpixelVisualRectChangeWithTransform) {
  Element* target = GetDocument().getElementById("target");
  target->setAttribute(HTMLNames::classAttr, "border transform");
  GetDocument().View()->UpdateAllLifecyclePhases();

  GetDocument().View()->SetTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "width: 100.6px; height: 70.3px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  const auto* raster_invalidations =
      &GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(2u, raster_invalidations->size());
  EXPECT_EQ(IntRect(0, 0, 140, 280), (*raster_invalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kGeometry,
            (*raster_invalidations)[0].reason);
  EXPECT_EQ(IntRect(0, 0, 242, 222), (*raster_invalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationReason::kGeometry,
            (*raster_invalidations)[1].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);

  GetDocument().View()->SetTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "width: 50px; height: 100px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  raster_invalidations = &GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(2u, raster_invalidations->size());
  EXPECT_EQ(IntRect(0, 0, 242, 222), (*raster_invalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kGeometry,
            (*raster_invalidations)[0].reason);
  EXPECT_EQ(IntRect(0, 0, 140, 280), (*raster_invalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationReason::kGeometry,
            (*raster_invalidations)[1].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, SubpixelWithinPixelsChange) {
  Element* target = GetDocument().getElementById("target");
  LayoutObject* target_object = target->GetLayoutObject();
  EXPECT_EQ(LayoutRect(0, 0, 70, 140), target_object->VisualRect());

  GetDocument().View()->SetTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr,
                       "margin-top: 0.6px; width: 50px; height: 99.3px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(LayoutUnit(), LayoutUnit(0.6), LayoutUnit(70),
                       LayoutUnit(139.3)),
            target_object->VisualRect());
  const auto* raster_invalidations =
      &GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(1u, raster_invalidations->size());
  EXPECT_EQ(IntRect(0, 0, 70, 140), (*raster_invalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kGeometry,
            (*raster_invalidations)[0].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);

  GetDocument().View()->SetTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr,
                       "margin-top: 0.6px; width: 49.3px; height: 98.5px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(LayoutUnit(), LayoutUnit(0.6), LayoutUnit(69.3),
                       LayoutUnit(138.5)),
            target_object->VisualRect());
  raster_invalidations = &GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(2u, raster_invalidations->size());
  EXPECT_EQ(IntRect(59, 0, 11, 140), (*raster_invalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            (*raster_invalidations)[0].reason);
  EXPECT_EQ(IntRect(0, 119, 70, 21), (*raster_invalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            (*raster_invalidations)[1].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, ResizeRotated) {
  Element* target = GetDocument().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "transform: rotate(45deg)");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Should do full invalidation a rotated object is resized.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr,
                       "transform: rotate(45deg); width: 200px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  const auto* raster_invalidations =
      &GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(1u, raster_invalidations->size());
  EXPECT_EQ(IntRect(-99, 0, 255, 255), (*raster_invalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kGeometry,
            (*raster_invalidations)[0].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, ResizeRotatedChild) {
  Element* target = GetDocument().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr,
                       "transform: rotate(45deg); width: 200px");
  target->setInnerHTML(
      "<div id=child style='width: 50px; height: 50px; background: "
      "red'></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* child = GetDocument().getElementById("child");

  // Should do full invalidation a rotated object is resized.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  child->setAttribute(HTMLNames::styleAttr,
                      "width: 100px; height: 50px; background: red");
  GetDocument().View()->UpdateAllLifecyclePhases();
  const auto* raster_invalidations =
      &GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(1u, raster_invalidations->size());
  EXPECT_EQ(IntRect(-43, 21, 107, 107), (*raster_invalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationReason::kGeometry,
            (*raster_invalidations)[0].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, CompositedLayoutViewResize) {
  EnableCompositing();
  Element* target = GetDocument().getElementById("target");
  target->setAttribute(HTMLNames::classAttr, "");
  target->setAttribute(HTMLNames::styleAttr, "height: 2000px");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Resize the content.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "height: 3000px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  const auto& raster_invalidations =
      GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(1u, raster_invalidations.size());
  EXPECT_EQ(IntRect(0, 2000, 800, 1000), raster_invalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(&GetLayoutView()),
            raster_invalidations[0].client);
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    EXPECT_EQ(PaintInvalidationReason::kBackgroundOnScrollingContentsLayer,
              raster_invalidations[0].reason);
  } else {
    EXPECT_EQ(PaintInvalidationReason::kIncremental,
              raster_invalidations[0].reason);
  }

  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Resize the viewport. No paint invalidation.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  GetDocument().View()->Resize(800, 1000);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetRasterInvalidationTracking());
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, CompositedLayoutViewGradientResize) {
  EnableCompositing();
  GetDocument().body()->setAttribute(HTMLNames::classAttr, "gradient");
  Element* target = GetDocument().getElementById("target");
  target->setAttribute(HTMLNames::classAttr, "");
  target->setAttribute(HTMLNames::styleAttr, "height: 2000px");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Resize the content.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "height: 3000px");
  GetDocument().View()->UpdateAllLifecyclePhases();

  const auto& raster_invalidations =
      GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(1u, raster_invalidations.size());
  EXPECT_EQ(IntRect(0, 0, 800, 3000), raster_invalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(&GetLayoutView()),
            raster_invalidations[0].client);
  if (RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    EXPECT_EQ(PaintInvalidationReason::kBackgroundOnScrollingContentsLayer,
              raster_invalidations[0].reason);
  } else {
    EXPECT_EQ(PaintInvalidationReason::kBackground,
              raster_invalidations[0].reason);
  }

  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Resize the viewport. No paint invalidation.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  GetDocument().View()->Resize(800, 1000);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(GetRasterInvalidationTracking());
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, NonCompositedLayoutViewResize) {
  SetBodyInnerHTML(
      "<style>"
      "  body { margin: 0 }"
      "  iframe { display: block; width: 100px; height: 100px; border: none; }"
      "</style>"
      "<iframe id='iframe'></iframe>");
  SetChildFrameHTML(
      "<style>"
      "  ::-webkit-scrollbar { display: none }"
      "  body { margin: 0; background: green; height: 0 }"
      "</style>"
      "<div id='content' style='width: 200px; height: 200px'></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* iframe = GetDocument().getElementById("iframe");
  Element* content = ChildDocument().getElementById("content");
  EXPECT_EQ(GetLayoutView(),
            content->GetLayoutObject()->ContainerForPaintInvalidation());

  // Resize the content.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  content->setAttribute(HTMLNames::styleAttr, "height: 500px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  // No invalidation because the changed part of layout overflow is clipped.
  EXPECT_FALSE(GetRasterInvalidationTracking());
  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Resize the iframe.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  iframe->setAttribute(HTMLNames::styleAttr, "height: 200px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  const auto& raster_invalidations =
      GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(2u, raster_invalidations.size());
  EXPECT_EQ(IntRect(0, 100, 100, 100), raster_invalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(iframe->GetLayoutObject()),
            raster_invalidations[0].client);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            raster_invalidations[0].reason);
  EXPECT_EQ(
      static_cast<const DisplayItemClient*>(content->GetLayoutObject()->View()),
      raster_invalidations[1].client);
  EXPECT_EQ(IntRect(0, 100, 100, 100), raster_invalidations[1].rect);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            raster_invalidations[1].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, NonCompositedLayoutViewGradientResize) {
  SetBodyInnerHTML(
      "<style>"
      "  body { margin: 0 }"
      "  iframe { display: block; width: 100px; height: 100px; border: none; }"
      "</style>"
      "<iframe id='iframe'></iframe>");
  SetChildFrameHTML(
      "<style>"
      "  ::-webkit-scrollbar { display: none }"
      "  body {"
      "    margin: 0;"
      "    height: 0;"
      "    background-image: linear-gradient(blue, yellow);"
      "  }"
      "</style>"
      "<div id='content' style='width: 200px; height: 200px'></div>");
  GetDocument().View()->UpdateAllLifecyclePhases();
  Element* iframe = GetDocument().getElementById("iframe");
  Element* content = ChildDocument().getElementById("content");
  LayoutView* frame_layout_view = content->GetLayoutObject()->View();
  EXPECT_EQ(GetLayoutView(),
            content->GetLayoutObject()->ContainerForPaintInvalidation());

  // Resize the content.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  content->setAttribute(HTMLNames::styleAttr, "height: 500px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  const auto* raster_invalidations =
      &GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(1u, raster_invalidations->size());
  EXPECT_EQ(IntRect(0, 0, 100, 100), (*raster_invalidations)[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(frame_layout_view),
            (*raster_invalidations)[0].client);
  EXPECT_EQ(PaintInvalidationReason::kBackground,
            (*raster_invalidations)[0].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Resize the iframe.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  iframe->setAttribute(HTMLNames::styleAttr, "height: 200px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  raster_invalidations = &GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(2u, raster_invalidations->size());
  EXPECT_EQ(IntRect(0, 100, 100, 100), (*raster_invalidations)[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(iframe->GetLayoutObject()),
            (*raster_invalidations)[0].client);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            (*raster_invalidations)[0].reason);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(frame_layout_view),
            (*raster_invalidations)[1].client);
  EXPECT_EQ(IntRect(0, 0, 100, 200), (*raster_invalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationReason::kBackground,
            (*raster_invalidations)[1].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, CompositedBackgroundAttachmentLocalResize) {
  EnableCompositing();

  Element* target = GetDocument().getElementById("target");
  target->setAttribute(HTMLNames::classAttr, "border local-background");
  target->setAttribute(HTMLNames::styleAttr, "will-change: transform");
  target->setInnerHTML(
      "<div id=child style='width: 500px; height: 500px'></div>",
      ASSERT_NO_EXCEPTION);
  Element* child = GetDocument().getElementById("child");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Resize the content.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  child->setAttribute(HTMLNames::styleAttr, "width: 500px; height: 1000px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  LayoutBoxModelObject* target_obj =
      ToLayoutBoxModelObject(target->GetLayoutObject());
  GraphicsLayer* container_layer =
      target_obj->Layer()->GraphicsLayerBacking(target_obj);
  GraphicsLayer* contents_layer = target_obj->Layer()->GraphicsLayerBacking();
  // No invalidation on the container layer.
  EXPECT_FALSE(container_layer->GetRasterInvalidationTracking());
  // Incremental invalidation of background on contents layer.
  const auto& contents_raster_invalidations =
      contents_layer->GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(1u, contents_raster_invalidations.size());
  EXPECT_EQ(IntRect(0, 500, 500, 500), contents_raster_invalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(target->GetLayoutObject()),
            contents_raster_invalidations[0].client);
  EXPECT_EQ(PaintInvalidationReason::kBackgroundOnScrollingContentsLayer,
            contents_raster_invalidations[0].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Resize the container.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr,
                       "will-change: transform; height: 200px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  // No invalidation on the contents layer.
  EXPECT_FALSE(contents_layer->GetRasterInvalidationTracking());
  // Incremental invalidation on the container layer.
  const auto& container_raster_invalidations =
      container_layer->GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(1u, container_raster_invalidations.size());
  EXPECT_EQ(IntRect(0, 120, 70, 120), container_raster_invalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(target->GetLayoutObject()),
            container_raster_invalidations[0].client);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            container_raster_invalidations[0].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest,
       CompositedBackgroundAttachmentLocalGradientResize) {
  EnableCompositing();

  Element* target = GetDocument().getElementById("target");
  target->setAttribute(HTMLNames::classAttr,
                       "border local-background gradient");
  target->setAttribute(HTMLNames::styleAttr, "will-change: transform");
  target->setInnerHTML(
      "<div id='child' style='width: 500px; height: 500px'></div>",
      ASSERT_NO_EXCEPTION);
  Element* child = GetDocument().getElementById("child");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Resize the content.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  child->setAttribute(HTMLNames::styleAttr, "width: 500px; height: 1000px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  LayoutBoxModelObject* target_obj =
      ToLayoutBoxModelObject(target->GetLayoutObject());
  GraphicsLayer* container_layer =
      target_obj->Layer()->GraphicsLayerBacking(target_obj);
  GraphicsLayer* contents_layer = target_obj->Layer()->GraphicsLayerBacking();
  // No invalidation on the container layer.
  EXPECT_FALSE(container_layer->GetRasterInvalidationTracking());
  // Full invalidation of background on contents layer because the gradient
  // background is resized.
  const auto& contents_raster_invalidations =
      contents_layer->GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(1u, contents_raster_invalidations.size());
  EXPECT_EQ(IntRect(0, 0, 500, 1000), contents_raster_invalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(target->GetLayoutObject()),
            contents_raster_invalidations[0].client);
  EXPECT_EQ(PaintInvalidationReason::kBackgroundOnScrollingContentsLayer,
            contents_raster_invalidations[0].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Resize the container.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr,
                       "will-change: transform; height: 200px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(contents_layer->GetRasterInvalidationTracking());
  // Full invalidation on the container layer.
  const auto& container_raster_invalidations =
      container_layer->GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(1u, container_raster_invalidations.size());
  EXPECT_EQ(IntRect(0, 0, 70, 240), container_raster_invalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(target->GetLayoutObject()),
            container_raster_invalidations[0].client);
  EXPECT_EQ(PaintInvalidationReason::kGeometry,
            container_raster_invalidations[0].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, NonCompositedBackgroundAttachmentLocalResize) {
  Element* target = GetDocument().getElementById("target");
  target->setAttribute(HTMLNames::classAttr, "border local-background");
  target->setInnerHTML(
      "<div id=child style='width: 500px; height: 500px'></div>",
      ASSERT_NO_EXCEPTION);
  Element* child = GetDocument().getElementById("child");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(&GetLayoutView(),
            &target->GetLayoutObject()->ContainerForPaintInvalidation());

  // Resize the content.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  child->setAttribute(HTMLNames::styleAttr, "width: 500px; height: 1000px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  // No invalidation because the changed part is invisible.
  EXPECT_FALSE(GetRasterInvalidationTracking());

  // Resize the container.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "height: 200px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  const auto& raster_invalidations =
      GetRasterInvalidationTracking()->invalidations;
  ASSERT_EQ(1u, raster_invalidations.size());
  EXPECT_EQ(IntRect(0, 120, 70, 120), raster_invalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(target->GetLayoutObject()),
            raster_invalidations[0].client);
  EXPECT_EQ(PaintInvalidationReason::kIncremental,
            raster_invalidations[0].reason);
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

}  // namespace blink
