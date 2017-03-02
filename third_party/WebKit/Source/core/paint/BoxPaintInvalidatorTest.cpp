// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/HTMLNames.h"
#include "core/frame/FrameView.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
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
        RenderingTest(SingleChildLocalFrameClient::create()) {}

 protected:
  const RasterInvalidationTracking* getRasterInvalidationTracking() const {
    // TODO(wangxianzhu): Test SPv2.
    return layoutView()
        .layer()
        ->graphicsLayerBacking()
        ->getRasterInvalidationTracking();
  }

 private:
  void SetUp() override {
    RenderingTest::SetUp();
    document().setCompatibilityMode(Document::NoQuirksMode);
    enableCompositing();
    setBodyInnerHTML(
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

TEST_P(BoxPaintInvalidatorTest, IncrementalInvalidationExpand) {
  document().view()->setTracksPaintInvalidations(true);
  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "width: 100px; height: 200px");
  document().view()->updateAllLifecyclePhases();
  const auto& rasterInvalidations =
      getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(2u, rasterInvalidations.size());
  EXPECT_EQ(IntRect(60, 0, 60, 240), rasterInvalidations[0].rect);
  EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[0].reason);
  EXPECT_EQ(IntRect(0, 120, 120, 120), rasterInvalidations[1].rect);
  EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[1].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, IncrementalInvalidationShrink) {
  document().view()->setTracksPaintInvalidations(true);
  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "width: 20px; height: 80px");
  document().view()->updateAllLifecyclePhases();
  const auto& rasterInvalidations =
      getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(2u, rasterInvalidations.size());
  EXPECT_EQ(IntRect(30, 0, 40, 140), rasterInvalidations[0].rect);
  EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[0].reason);
  EXPECT_EQ(IntRect(0, 100, 70, 40), rasterInvalidations[1].rect);
  EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[1].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, IncrementalInvalidationMixed) {
  document().view()->setTracksPaintInvalidations(true);
  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "width: 100px; height: 80px");
  document().view()->updateAllLifecyclePhases();
  const auto& rasterInvalidations =
      getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(2u, rasterInvalidations.size());
  EXPECT_EQ(IntRect(60, 0, 60, 120), rasterInvalidations[0].rect);
  EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[0].reason);
  EXPECT_EQ(IntRect(0, 100, 70, 40), rasterInvalidations[1].rect);
  EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[1].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, SubpixelVisualRectChagne) {
  ScopedSlimmingPaintInvalidationForTest scopedSlimmingPaintInvalidation(true);

  Element* target = document().getElementById("target");

  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "width: 100.6px; height: 70.3px");
  document().view()->updateAllLifecyclePhases();
  const auto* rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(2u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(60, 0, 61, 111), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationIncremental, (*rasterInvalidations)[0].reason);
  EXPECT_EQ(IntRect(0, 90, 70, 50), (*rasterInvalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationIncremental, (*rasterInvalidations)[1].reason);
  document().view()->setTracksPaintInvalidations(false);

  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "width: 50px; height: 100px");
  document().view()->updateAllLifecyclePhases();
  rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(2u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(60, 0, 61, 111), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationIncremental, (*rasterInvalidations)[0].reason);
  EXPECT_EQ(IntRect(0, 90, 70, 50), (*rasterInvalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationIncremental, (*rasterInvalidations)[1].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, SubpixelVisualRectChangeWithTransform) {
  ScopedSlimmingPaintInvalidationForTest scopedSlimmingPaintInvalidation(true);

  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::classAttr, "border transform");
  document().view()->updateAllLifecyclePhases();

  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "width: 100.6px; height: 70.3px");
  document().view()->updateAllLifecyclePhases();
  const auto* rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(2u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(0, 0, 140, 280), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationBorderBoxChange, (*rasterInvalidations)[0].reason);
  EXPECT_EQ(IntRect(0, 0, 242, 222), (*rasterInvalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationBorderBoxChange, (*rasterInvalidations)[1].reason);
  document().view()->setTracksPaintInvalidations(false);

  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "width: 50px; height: 100px");
  document().view()->updateAllLifecyclePhases();
  rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(2u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(0, 0, 242, 222), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationBorderBoxChange, (*rasterInvalidations)[0].reason);
  EXPECT_EQ(IntRect(0, 0, 140, 280), (*rasterInvalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationBorderBoxChange, (*rasterInvalidations)[1].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, SubpixelWithinPixelsChange) {
  ScopedSlimmingPaintInvalidationForTest scopedSlimmingPaintInvalidation(true);

  Element* target = document().getElementById("target");
  LayoutObject* targetObject = target->layoutObject();
  EXPECT_EQ(LayoutRect(0, 0, 70, 140), targetObject->visualRect());

  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr,
                       "margin-top: 0.6px; width: 50px; height: 99.3px");
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(LayoutUnit(), LayoutUnit(0.6), LayoutUnit(70),
                       LayoutUnit(139.3)),
            targetObject->visualRect());
  const auto* rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(1u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(0, 0, 70, 140), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationBoundsChange, (*rasterInvalidations)[0].reason);
  document().view()->setTracksPaintInvalidations(false);

  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr,
                       "margin-top: 0.6px; width: 49.3px; height: 98.5px");
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(LayoutUnit(), LayoutUnit(0.6), LayoutUnit(69.3),
                       LayoutUnit(138.5)),
            targetObject->visualRect());
  rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(2u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(59, 0, 11, 140), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationIncremental, (*rasterInvalidations)[0].reason);
  EXPECT_EQ(IntRect(0, 119, 70, 21), (*rasterInvalidations)[1].rect);
  EXPECT_EQ(PaintInvalidationIncremental, (*rasterInvalidations)[1].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, ResizeRotated) {
  ScopedSlimmingPaintInvalidationForTest scopedSlimmingPaintInvalidation(true);

  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr, "transform: rotate(45deg)");
  document().view()->updateAllLifecyclePhases();

  // Should do full invalidation a rotated object is resized.
  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr,
                       "transform: rotate(45deg); width: 200px");
  document().view()->updateAllLifecyclePhases();
  const auto* rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(1u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(-99, 0, 255, 255), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationBorderBoxChange, (*rasterInvalidations)[0].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, ResizeRotatedChild) {
  ScopedSlimmingPaintInvalidationForTest scopedSlimmingPaintInvalidation(true);

  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::styleAttr,
                       "transform: rotate(45deg); width: 200px");
  target->setInnerHTML(
      "<div id=child style='width: 50px; height: 50px; background: "
      "red'></div>");
  document().view()->updateAllLifecyclePhases();
  Element* child = document().getElementById("child");

  // Should do full invalidation a rotated object is resized.
  document().view()->setTracksPaintInvalidations(true);
  child->setAttribute(HTMLNames::styleAttr,
                      "width: 100px; height: 50px; background: red");
  document().view()->updateAllLifecyclePhases();
  const auto* rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(1u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(-43, 21, 107, 107), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(PaintInvalidationBorderBoxChange, (*rasterInvalidations)[0].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, CompositedLayoutViewResize) {
  enableCompositing();
  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::classAttr, "");
  target->setAttribute(HTMLNames::styleAttr, "height: 2000px");
  document().view()->updateAllLifecyclePhases();

  // Resize the content.
  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "height: 3000px");
  document().view()->updateAllLifecyclePhases();
  const auto& rasterInvalidations =
      getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(1u, rasterInvalidations.size());
  EXPECT_EQ(IntRect(0, 2000, 800, 1000), rasterInvalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(&layoutView()),
            rasterInvalidations[0].client);
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    EXPECT_EQ(PaintInvalidationBackgroundOnScrollingContentsLayer,
              rasterInvalidations[0].reason);
  } else {
    EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[0].reason);
  }

  document().view()->setTracksPaintInvalidations(false);

  // Resize the viewport. No paint invalidation.
  document().view()->setTracksPaintInvalidations(true);
  document().view()->resize(800, 1000);
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(getRasterInvalidationTracking());
  document().view()->setTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, CompositedLayoutViewGradientResize) {
  enableCompositing();
  document().body()->setAttribute(HTMLNames::classAttr, "gradient");
  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::classAttr, "");
  target->setAttribute(HTMLNames::styleAttr, "height: 2000px");
  document().view()->updateAllLifecyclePhases();

  // Resize the content.
  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "height: 3000px");
  document().view()->updateAllLifecyclePhases();

  const auto& rasterInvalidations =
      getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(1u, rasterInvalidations.size());
  EXPECT_EQ(IntRect(0, 0, 800, 3000), rasterInvalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(&layoutView()),
            rasterInvalidations[0].client);
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    EXPECT_EQ(PaintInvalidationBackgroundOnScrollingContentsLayer,
              rasterInvalidations[0].reason);
  } else {
    EXPECT_EQ(PaintInvalidationLayoutOverflowBoxChange,
              rasterInvalidations[0].reason);
  }

  document().view()->setTracksPaintInvalidations(false);

  // Resize the viewport. No paint invalidation.
  document().view()->setTracksPaintInvalidations(true);
  document().view()->resize(800, 1000);
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(getRasterInvalidationTracking());
  document().view()->setTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, NonCompositedLayoutViewResize) {
  setBodyInnerHTML(
      "<style>"
      "  body { margin: 0 }"
      "  iframe { display: block; width: 100px; height: 100px; border: none; }"
      "</style>"
      "<iframe id='iframe'></iframe>");
  setChildFrameHTML(
      "<style>"
      "  ::-webkit-scrollbar { display: none }"
      "  body { margin: 0; background: green; height: 0 }"
      "</style>"
      "<div id='content' style='width: 200px; height: 200px'></div>");
  document().view()->updateAllLifecyclePhases();
  Element* iframe = document().getElementById("iframe");
  Element* content = childDocument().getElementById("content");
  EXPECT_EQ(layoutView(),
            content->layoutObject()->containerForPaintInvalidation());

  // Resize the content.
  document().view()->setTracksPaintInvalidations(true);
  content->setAttribute(HTMLNames::styleAttr, "height: 500px");
  document().view()->updateAllLifecyclePhases();
  // No invalidation because the changed part of layout overflow is clipped.
  EXPECT_FALSE(getRasterInvalidationTracking());
  document().view()->setTracksPaintInvalidations(false);

  // Resize the iframe.
  document().view()->setTracksPaintInvalidations(true);
  iframe->setAttribute(HTMLNames::styleAttr, "height: 200px");
  document().view()->updateAllLifecyclePhases();
  const auto& rasterInvalidations =
      getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(2u, rasterInvalidations.size());
  EXPECT_EQ(IntRect(0, 100, 100, 100), rasterInvalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(iframe->layoutObject()),
            rasterInvalidations[0].client);
  EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[0].reason);
  EXPECT_EQ(
      static_cast<const DisplayItemClient*>(content->layoutObject()->view()),
      rasterInvalidations[1].client);
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    // TODO(skobes): Treat LayoutView in the same way as normal objects having
    // background-attachment: local. crbug.com/568847.
    EXPECT_EQ(IntRect(0, 0, 100, 200), rasterInvalidations[1].rect);
    EXPECT_EQ(PaintInvalidationFull, rasterInvalidations[1].reason);
  } else {
    EXPECT_EQ(IntRect(0, 100, 100, 100), rasterInvalidations[1].rect);
    EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[1].reason);
  }
  document().view()->setTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, NonCompositedLayoutViewGradientResize) {
  setBodyInnerHTML(
      "<style>"
      "  body { margin: 0 }"
      "  iframe { display: block; width: 100px; height: 100px; border: none; }"
      "</style>"
      "<iframe id='iframe'></iframe>");
  setChildFrameHTML(
      "<style>"
      "  ::-webkit-scrollbar { display: none }"
      "  body {"
      "    margin: 0;"
      "    height: 0;"
      "    background-image: linear-gradient(blue, yellow);"
      "  }"
      "</style>"
      "<div id='content' style='width: 200px; height: 200px'></div>");
  document().view()->updateAllLifecyclePhases();
  Element* iframe = document().getElementById("iframe");
  Element* content = childDocument().getElementById("content");
  LayoutView* frameLayoutView = content->layoutObject()->view();
  EXPECT_EQ(layoutView(),
            content->layoutObject()->containerForPaintInvalidation());

  // Resize the content.
  document().view()->setTracksPaintInvalidations(true);
  content->setAttribute(HTMLNames::styleAttr, "height: 500px");
  document().view()->updateAllLifecyclePhases();
  const auto* rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(1u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(0, 0, 100, 100), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(frameLayoutView),
            (*rasterInvalidations)[0].client);
  EXPECT_EQ(PaintInvalidationLayoutOverflowBoxChange,
            (*rasterInvalidations)[0].reason);
  document().view()->setTracksPaintInvalidations(false);

  // Resize the iframe.
  document().view()->setTracksPaintInvalidations(true);
  iframe->setAttribute(HTMLNames::styleAttr, "height: 200px");
  document().view()->updateAllLifecyclePhases();
  rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(2u, rasterInvalidations->size());
  EXPECT_EQ(IntRect(0, 100, 100, 100), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(iframe->layoutObject()),
            (*rasterInvalidations)[0].client);
  EXPECT_EQ(PaintInvalidationIncremental, (*rasterInvalidations)[0].reason);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(frameLayoutView),
            (*rasterInvalidations)[1].client);
  EXPECT_EQ(IntRect(0, 0, 100, 200), (*rasterInvalidations)[1].rect);
  if (RuntimeEnabledFeatures::rootLayerScrollingEnabled()) {
    // TODO(skobes): Treat LayoutView in the same way as normal objects having
    // background-attachment: local. crbug.com/568847.
    EXPECT_EQ(PaintInvalidationFull, (*rasterInvalidations)[1].reason);
  } else {
    EXPECT_EQ(PaintInvalidationViewBackground,
              (*rasterInvalidations)[1].reason);
  }
  document().view()->setTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, CompositedBackgroundAttachmentLocalResize) {
  enableCompositing();

  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::classAttr, "border local-background");
  target->setAttribute(HTMLNames::styleAttr, "will-change: transform");
  target->setInnerHTML(
      "<div id=child style='width: 500px; height: 500px'></div>",
      ASSERT_NO_EXCEPTION);
  Element* child = document().getElementById("child");
  document().view()->updateAllLifecyclePhases();

  // Resize the content.
  document().view()->setTracksPaintInvalidations(true);
  child->setAttribute(HTMLNames::styleAttr, "width: 500px; height: 1000px");
  document().view()->updateAllLifecyclePhases();
  LayoutBoxModelObject* targetObj =
      toLayoutBoxModelObject(target->layoutObject());
  GraphicsLayer* containerLayer =
      targetObj->layer()->graphicsLayerBacking(targetObj);
  GraphicsLayer* contentsLayer = targetObj->layer()->graphicsLayerBacking();
  // No invalidation on the container layer.
  EXPECT_FALSE(containerLayer->getRasterInvalidationTracking());
  // Incremental invalidation of background on contents layer.
  const auto& contentsRasterInvalidations =
      contentsLayer->getRasterInvalidationTracking()
          ->trackedRasterInvalidations;
  ASSERT_EQ(1u, contentsRasterInvalidations.size());
  EXPECT_EQ(IntRect(0, 500, 500, 500), contentsRasterInvalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(target->layoutObject()),
            contentsRasterInvalidations[0].client);
  EXPECT_EQ(PaintInvalidationBackgroundOnScrollingContentsLayer,
            contentsRasterInvalidations[0].reason);
  document().view()->setTracksPaintInvalidations(false);

  // Resize the container.
  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr,
                       "will-change: transform; height: 200px");
  document().view()->updateAllLifecyclePhases();
  // No invalidation on the contents layer.
  EXPECT_FALSE(contentsLayer->getRasterInvalidationTracking());
  // Incremental invalidation on the container layer.
  const auto& containerRasterInvalidations =
      containerLayer->getRasterInvalidationTracking()
          ->trackedRasterInvalidations;
  ASSERT_EQ(1u, containerRasterInvalidations.size());
  EXPECT_EQ(IntRect(0, 120, 70, 120), containerRasterInvalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(target->layoutObject()),
            containerRasterInvalidations[0].client);
  EXPECT_EQ(PaintInvalidationIncremental,
            containerRasterInvalidations[0].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest,
       CompositedBackgroundAttachmentLocalGradientResize) {
  enableCompositing();

  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::classAttr,
                       "border local-background gradient");
  target->setAttribute(HTMLNames::styleAttr, "will-change: transform");
  target->setInnerHTML(
      "<div id='child' style='width: 500px; height: 500px'></div>",
      ASSERT_NO_EXCEPTION);
  Element* child = document().getElementById("child");
  document().view()->updateAllLifecyclePhases();

  // Resize the content.
  document().view()->setTracksPaintInvalidations(true);
  child->setAttribute(HTMLNames::styleAttr, "width: 500px; height: 1000px");
  document().view()->updateAllLifecyclePhases();
  LayoutBoxModelObject* targetObj =
      toLayoutBoxModelObject(target->layoutObject());
  GraphicsLayer* containerLayer =
      targetObj->layer()->graphicsLayerBacking(targetObj);
  GraphicsLayer* contentsLayer = targetObj->layer()->graphicsLayerBacking();
  // No invalidation on the container layer.
  EXPECT_FALSE(containerLayer->getRasterInvalidationTracking());
  // Full invalidation of background on contents layer because the gradient
  // background is resized.
  const auto& contentsRasterInvalidations =
      contentsLayer->getRasterInvalidationTracking()
          ->trackedRasterInvalidations;
  ASSERT_EQ(1u, contentsRasterInvalidations.size());
  EXPECT_EQ(IntRect(0, 0, 500, 1000), contentsRasterInvalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(target->layoutObject()),
            contentsRasterInvalidations[0].client);
  EXPECT_EQ(PaintInvalidationBackgroundOnScrollingContentsLayer,
            contentsRasterInvalidations[0].reason);
  document().view()->setTracksPaintInvalidations(false);

  // Resize the container.
  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr,
                       "will-change: transform; height: 200px");
  document().view()->updateAllLifecyclePhases();
  EXPECT_FALSE(contentsLayer->getRasterInvalidationTracking());
  // Full invalidation on the container layer.
  const auto& containerRasterInvalidations =
      containerLayer->getRasterInvalidationTracking()
          ->trackedRasterInvalidations;
  ASSERT_EQ(1u, containerRasterInvalidations.size());
  EXPECT_EQ(IntRect(0, 0, 70, 240), containerRasterInvalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(target->layoutObject()),
            containerRasterInvalidations[0].client);
  EXPECT_EQ(PaintInvalidationBorderBoxChange,
            containerRasterInvalidations[0].reason);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_P(BoxPaintInvalidatorTest, NonCompositedBackgroundAttachmentLocalResize) {
  Element* target = document().getElementById("target");
  target->setAttribute(HTMLNames::classAttr, "border local-background");
  target->setInnerHTML(
      "<div id=child style='width: 500px; height: 500px'></div>",
      ASSERT_NO_EXCEPTION);
  Element* child = document().getElementById("child");
  document().view()->updateAllLifecyclePhases();
  EXPECT_EQ(&layoutView(),
            &target->layoutObject()->containerForPaintInvalidation());

  // Resize the content.
  document().view()->setTracksPaintInvalidations(true);
  child->setAttribute(HTMLNames::styleAttr, "width: 500px; height: 1000px");
  document().view()->updateAllLifecyclePhases();
  // No invalidation because the changed part is invisible.
  EXPECT_FALSE(getRasterInvalidationTracking());

  // Resize the container.
  document().view()->setTracksPaintInvalidations(true);
  target->setAttribute(HTMLNames::styleAttr, "height: 200px");
  document().view()->updateAllLifecyclePhases();
  const auto& rasterInvalidations =
      getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(1u, rasterInvalidations.size());
  EXPECT_EQ(IntRect(0, 120, 70, 120), rasterInvalidations[0].rect);
  EXPECT_EQ(static_cast<const DisplayItemClient*>(target->layoutObject()),
            rasterInvalidations[0].client);
  EXPECT_EQ(PaintInvalidationIncremental, rasterInvalidations[0].reason);
  document().view()->setTracksPaintInvalidations(false);
}

}  // namespace blink
