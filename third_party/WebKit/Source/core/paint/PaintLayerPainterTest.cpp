// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/PaintLayerPainter.h"

#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/paint/PaintControllerPaintTest.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/testing/RuntimeEnabledFeaturesTestHelpers.h"

namespace blink {

struct PaintLayerPainterTestParam {
  PaintLayerPainterTestParam(bool root_layer_scrolling, bool slimming_paint_v2)
      : root_layer_scrolling(root_layer_scrolling),
        slimming_paint_v2(slimming_paint_v2) {}

  bool root_layer_scrolling;
  bool slimming_paint_v2;
};

class PaintLayerPainterTest
    : public testing::WithParamInterface<PaintLayerPainterTestParam>,
      private ScopedRootLayerScrollingForTest,
      public PaintControllerPaintTestBase {
  USING_FAST_MALLOC(PaintLayerPainterTest);

 public:
  PaintLayerPainterTest()
      : ScopedRootLayerScrollingForTest(GetParam().root_layer_scrolling),
        PaintControllerPaintTestBase(GetParam().slimming_paint_v2) {}

  void ExpectPaintedOutputInvisible(const char* element_name,
                                    bool expected_value) {
    // The optimization to skip painting for effectively-invisible content is
    // limited to SPv1.
    if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
      return;

    PaintLayer* target_layer =
        ToLayoutBox(GetLayoutObjectByElementId(element_name))->Layer();
    PaintLayerPaintingInfo painting_info(nullptr, LayoutRect(),
                                         kGlobalPaintNormalPhase, LayoutSize());
    bool invisible =
        PaintLayerPainter(*target_layer).PaintedOutputInvisible(painting_info);
    EXPECT_EQ(expected_value, invisible)
        << "Failed painted output visibility [spv2_enabled="
        << RuntimeEnabledFeatures::SlimmingPaintV2Enabled()
        << ", expected=" << expected_value << ", actual=" << invisible << "].";
  }

  PaintController& MainGraphicsLayerPaintController() {
    return GetLayoutView()
        .Layer()
        ->GraphicsLayerBacking(&GetLayoutView())
        ->GetPaintController();
  }

 private:
  void SetUp() override {
    PaintControllerPaintTestBase::SetUp();
    EnableCompositing();
  }
};

INSTANTIATE_TEST_CASE_P(
    All,
    PaintLayerPainterTest,
    ::testing::Values(PaintLayerPainterTestParam(
                          false,
                          false),  // non-root-layer-scrolls, slimming-paint-v1
                      PaintLayerPainterTestParam(
                          false,
                          true),  // non-root-layer-scrolls, slimming-paint-v2
                      PaintLayerPainterTestParam(
                          true,
                          false),  // root-layer-scrolls, slimming-paint-v1
                      PaintLayerPainterTestParam(
                          true,
                          true)));  // root-layer-scrolls, slimming-paint-v2

TEST_P(PaintLayerPainterTest, CachedSubsequence) {
  SetBodyInnerHTML(
      "<div id='container1' style='position: relative; z-index: 1; width: "
      "200px; height: 200px; background-color: blue'>"
      "  <div id='content1' style='position: absolute; width: 100px; height: "
      "100px; background-color: red'></div>"
      "</div>"
      "<div id='container2' style='position: relative; z-index: 1; width: "
      "200px; height: 200px; background-color: blue'>"
      "  <div id='content2' style='position: absolute; width: 100px; height: "
      "100px; background-color: green'></div>"
      "</div>");

  LayoutObject& container1 =
      *GetDocument().getElementById("container1")->GetLayoutObject();
  LayoutObject& content1 =
      *GetDocument().getElementById("content1")->GetLayoutObject();
  LayoutObject& container2 =
      *GetDocument().getElementById("container2")->GetLayoutObject();
  LayoutObject& content2 =
      *GetDocument().getElementById("content2")->GetLayoutObject();

  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    // With SPv1 and RLS, the LayoutView gets painted into the
    // MainGraphicsLayer while the rest into the ScrollingContentsLayer
    EXPECT_DISPLAY_LIST(
        MainGraphicsLayerPaintController().GetDisplayItemList(), 1,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType));

    EXPECT_DISPLAY_LIST(RootPaintController().GetDisplayItemList(), 4,
                        TestDisplayItem(container1, kBackgroundType),
                        TestDisplayItem(content1, kBackgroundType),
                        TestDisplayItem(container2, kBackgroundType),
                        TestDisplayItem(content2, kBackgroundType));
  } else {
    EXPECT_DISPLAY_LIST(
        RootPaintController().GetDisplayItemList(), 5,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
        TestDisplayItem(container1, kBackgroundType),
        TestDisplayItem(content1, kBackgroundType),
        TestDisplayItem(container2, kBackgroundType),
        TestDisplayItem(content2, kBackgroundType));
  }

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    // check that new paint chunks were forced for |container1| and
    // |container2|.
    Vector<PaintChunk> paint_chunks =
        RootPaintController().GetPaintArtifact().PaintChunks();
    EXPECT_EQ(3u, paint_chunks.size());
    EXPECT_EQ(GetLayoutView().Layer(), &paint_chunks[0].id.client);
    EXPECT_EQ(ToLayoutBoxModelObject(container1).Layer(),
              &paint_chunks[1].id.client);
    EXPECT_EQ(ToLayoutBoxModelObject(container2).Layer(),
              &paint_chunks[2].id.client);
  }

  ToHTMLElement(content1.GetNode())
      ->setAttribute(HTMLNames::styleAttr,
                     "position: absolute; width: 100px; height: 100px; "
                     "background-color: green");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(PaintWithoutCommit());

  // With RLS and SPv1, the LayoutView is painted into a separate GraphicsLayer
  // so it doesn't contribute to NumCachedNewItems since it isn't invalidated.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      RuntimeEnabledFeatures::RootLayerScrollingEnabled())
    EXPECT_EQ(3, NumCachedNewItems());
  else
    EXPECT_EQ(4, NumCachedNewItems());

  Commit();

  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    EXPECT_DISPLAY_LIST(
        MainGraphicsLayerPaintController().GetDisplayItemList(), 1,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType));

    EXPECT_DISPLAY_LIST(RootPaintController().GetDisplayItemList(), 4,
                        TestDisplayItem(container1, kBackgroundType),
                        TestDisplayItem(content1, kBackgroundType),
                        TestDisplayItem(container2, kBackgroundType),
                        TestDisplayItem(content2, kBackgroundType));
  } else {
    EXPECT_DISPLAY_LIST(
        RootPaintController().GetDisplayItemList(), 5,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
        TestDisplayItem(container1, kBackgroundType),
        TestDisplayItem(content1, kBackgroundType),
        TestDisplayItem(container2, kBackgroundType),
        TestDisplayItem(content2, kBackgroundType));
  }
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceOnInterestRectChange) {
  // TODO(wangxianzhu): SPv2 deals with interest rect differently, so disable
  // this test for SPv2 temporarily.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  SetBodyInnerHTML(
      "<div id='container1' style='position: relative; z-index: 1; width: "
      "200px; height: 200px; background-color: blue'>"
      "  <div id='content1' style='position: absolute; width: 100px; height: "
      "100px; background-color: green'></div>"
      "</div>"
      "<div id='container2' style='position: relative; z-index: 1; width: "
      "200px; height: 200px; background-color: blue'>"
      "  <div id='content2a' style='position: absolute; width: 100px; height: "
      "100px; background-color: green'></div>"
      "  <div id='content2b' style='position: absolute; top: 200px; width: "
      "100px; height: 100px; background-color: green'></div>"
      "</div>"
      "<div id='container3' style='position: absolute; z-index: 2; left: "
      "300px; top: 0; width: 200px; height: 200px; background-color: blue'>"
      "  <div id='content3' style='position: absolute; width: 200px; height: "
      "200px; background-color: green'></div>"
      "</div>");
  RootPaintController().InvalidateAll();

  LayoutObject& container1 =
      *GetDocument().getElementById("container1")->GetLayoutObject();
  LayoutObject& content1 =
      *GetDocument().getElementById("content1")->GetLayoutObject();
  LayoutObject& container2 =
      *GetDocument().getElementById("container2")->GetLayoutObject();
  LayoutObject& content2a =
      *GetDocument().getElementById("content2a")->GetLayoutObject();
  LayoutObject& content2b =
      *GetDocument().getElementById("content2b")->GetLayoutObject();
  LayoutObject& container3 =
      *GetDocument().getElementById("container3")->GetLayoutObject();
  LayoutObject& content3 =
      *GetDocument().getElementById("content3")->GetLayoutObject();

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  IntRect interest_rect(0, 0, 400, 300);
  Paint(&interest_rect);

  // Container1 is fully in the interest rect;
  // Container2 is partly (including its stacking chidren) in the interest rect;
  // Content2b is out of the interest rect and output nothing;
  // Container3 is partly in the interest rect.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    // With SPv1 and RLS, the LayoutView gets painted into the
    // MainGraphicsLayer while the rest into the ScrollingContentsLayer
    EXPECT_DISPLAY_LIST(
        MainGraphicsLayerPaintController().GetDisplayItemList(), 1,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType));
    EXPECT_DISPLAY_LIST(RootPaintController().GetDisplayItemList(), 6,
                        TestDisplayItem(container1, kBackgroundType),
                        TestDisplayItem(content1, kBackgroundType),
                        TestDisplayItem(container2, kBackgroundType),
                        TestDisplayItem(content2a, kBackgroundType),
                        TestDisplayItem(container3, kBackgroundType),
                        TestDisplayItem(content3, kBackgroundType));
  } else {
    EXPECT_DISPLAY_LIST(
        RootPaintController().GetDisplayItemList(), 7,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
        TestDisplayItem(container1, kBackgroundType),
        TestDisplayItem(content1, kBackgroundType),
        TestDisplayItem(container2, kBackgroundType),
        TestDisplayItem(content2a, kBackgroundType),
        TestDisplayItem(container3, kBackgroundType),
        TestDisplayItem(content3, kBackgroundType));
  }

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  IntRect new_interest_rect(0, 100, 300, 1000);
  EXPECT_TRUE(PaintWithoutCommit(&new_interest_rect));

  // Container1 becomes partly in the interest rect, but uses cached subsequence
  // because it was fully painted before;
  // Container2's intersection with the interest rect changes;
  // Content2b is out of the interest rect and outputs nothing;
  // Container3 becomes out of the interest rect and outputs empty subsequence
  // pair.
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      RuntimeEnabledFeatures::RootLayerScrollingEnabled())
    EXPECT_EQ(4, NumCachedNewItems());
  else
    EXPECT_EQ(5, NumCachedNewItems());

  Commit();

  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    EXPECT_DISPLAY_LIST(
        MainGraphicsLayerPaintController().GetDisplayItemList(), 1,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType));
    EXPECT_DISPLAY_LIST(RootPaintController().GetDisplayItemList(), 5,
                        TestDisplayItem(container1, kBackgroundType),
                        TestDisplayItem(content1, kBackgroundType),
                        TestDisplayItem(container2, kBackgroundType),
                        TestDisplayItem(content2a, kBackgroundType),
                        TestDisplayItem(content2b, kBackgroundType));
  } else {
    EXPECT_DISPLAY_LIST(
        RootPaintController().GetDisplayItemList(), 6,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
        TestDisplayItem(container1, kBackgroundType),
        TestDisplayItem(content1, kBackgroundType),
        TestDisplayItem(container2, kBackgroundType),
        TestDisplayItem(content2a, kBackgroundType),
        TestDisplayItem(content2b, kBackgroundType));
  }
}

TEST_P(PaintLayerPainterTest,
       CachedSubsequenceOnStyleChangeWithInterestRectClipping) {
  SetBodyInnerHTML(
      "<div id='container1' style='position: relative; z-index: 1; width: "
      "200px; height: 200px; background-color: blue'>"
      "  <div id='content1' style='position: absolute; width: 100px; height: "
      "100px; background-color: red'></div>"
      "</div>"
      "<div id='container2' style='position: relative; z-index: 1; width: "
      "200px; height: 200px; background-color: blue'>"
      "  <div id='content2' style='position: absolute; width: 100px; height: "
      "100px; background-color: green'></div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // PaintResult of all subsequences will be MayBeClippedByPaintDirtyRect.
  IntRect interest_rect(0, 0, 50, 300);
  Paint(&interest_rect);

  LayoutObject& container1 =
      *GetDocument().getElementById("container1")->GetLayoutObject();
  LayoutObject& content1 =
      *GetDocument().getElementById("content1")->GetLayoutObject();
  LayoutObject& container2 =
      *GetDocument().getElementById("container2")->GetLayoutObject();
  LayoutObject& content2 =
      *GetDocument().getElementById("content2")->GetLayoutObject();

  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    // With SPv1 and RLS, the LayoutView gets painted into the
    // MainGraphicsLayer while the rest into the ScrollingContentsLayer
    EXPECT_DISPLAY_LIST(
        MainGraphicsLayerPaintController().GetDisplayItemList(), 1,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType));
    EXPECT_DISPLAY_LIST(RootPaintController().GetDisplayItemList(), 4,
                        TestDisplayItem(container1, kBackgroundType),
                        TestDisplayItem(content1, kBackgroundType),
                        TestDisplayItem(container2, kBackgroundType),
                        TestDisplayItem(content2, kBackgroundType));
  } else {
    EXPECT_DISPLAY_LIST(
        RootPaintController().GetDisplayItemList(), 5,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
        TestDisplayItem(container1, kBackgroundType),
        TestDisplayItem(content1, kBackgroundType),
        TestDisplayItem(container2, kBackgroundType),
        TestDisplayItem(content2, kBackgroundType));
  }

  ToHTMLElement(content1.GetNode())
      ->setAttribute(HTMLNames::styleAttr,
                     "position: absolute; width: 100px; height: 100px; "
                     "background-color: green");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(PaintWithoutCommit(&interest_rect));

  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      RuntimeEnabledFeatures::RootLayerScrollingEnabled())
    EXPECT_EQ(3, NumCachedNewItems());
  else
    EXPECT_EQ(4, NumCachedNewItems());

  Commit();

  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled() &&
      RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    EXPECT_DISPLAY_LIST(
        MainGraphicsLayerPaintController().GetDisplayItemList(), 1,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType));
    EXPECT_DISPLAY_LIST(RootPaintController().GetDisplayItemList(), 4,
                        TestDisplayItem(container1, kBackgroundType),
                        TestDisplayItem(content1, kBackgroundType),
                        TestDisplayItem(container2, kBackgroundType),
                        TestDisplayItem(content2, kBackgroundType));
  } else {
    EXPECT_DISPLAY_LIST(
        RootPaintController().GetDisplayItemList(), 5,
        TestDisplayItem(GetLayoutView(), kDocumentBackgroundType),
        TestDisplayItem(container1, kBackgroundType),
        TestDisplayItem(content1, kBackgroundType),
        TestDisplayItem(container2, kBackgroundType),
        TestDisplayItem(content2, kBackgroundType));
  }
}

TEST_P(PaintLayerPainterTest, PaintPhaseOutline) {
  AtomicString style_without_outline =
      "width: 50px; height: 50px; background-color: green";
  AtomicString style_with_outline =
      "outline: 1px solid blue; " + style_without_outline;
  SetBodyInnerHTML(
      "<div id='self-painting-layer' style='position: absolute'>"
      "  <div id='non-self-painting-layer' style='overflow: hidden'>"
      "    <div>"
      "      <div id='outline'></div>"
      "    </div>"
      "  </div>"
      "</div>");
  LayoutObject& outline_div =
      *GetDocument().getElementById("outline")->GetLayoutObject();
  ToHTMLElement(outline_div.GetNode())
      ->setAttribute(HTMLNames::styleAttr, style_without_outline);
  GetDocument().View()->UpdateAllLifecyclePhases();

  LayoutBoxModelObject& self_painting_layer_object = *ToLayoutBoxModelObject(
      GetDocument().getElementById("self-painting-layer")->GetLayoutObject());
  PaintLayer& self_painting_layer = *self_painting_layer_object.Layer();
  ASSERT_TRUE(self_painting_layer.IsSelfPaintingLayer());
  PaintLayer& non_self_painting_layer =
      *ToLayoutBoxModelObject(GetDocument()
                                  .getElementById("non-self-painting-layer")
                                  ->GetLayoutObject())
           ->Layer();
  ASSERT_FALSE(non_self_painting_layer.IsSelfPaintingLayer());
  ASSERT_TRUE(&non_self_painting_layer == outline_div.EnclosingLayer());

  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseDescendantOutlines());

  // Outline on the self-painting-layer node itself doesn't affect
  // PaintPhaseDescendantOutlines.
  ToHTMLElement(self_painting_layer_object.GetNode())
      ->setAttribute(HTMLNames::styleAttr,
                     "position: absolute; outline: 1px solid green");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), self_painting_layer_object,
      DisplayItem::PaintPhaseToDrawingType(kPaintPhaseSelfOutlineOnly)));

  // needsPaintPhaseDescendantOutlines should be set when any descendant on the
  // same layer has outline.
  ToHTMLElement(outline_div.GetNode())
      ->setAttribute(HTMLNames::styleAttr, style_with_outline);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  Paint();
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), outline_div,
      DisplayItem::PaintPhaseToDrawingType(kPaintPhaseSelfOutlineOnly)));

  // needsPaintPhaseDescendantOutlines should be reset when no outline is
  // actually painted.
  ToHTMLElement(outline_div.GetNode())
      ->setAttribute(HTMLNames::styleAttr, style_without_outline);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
}

TEST_P(PaintLayerPainterTest, PaintPhaseFloat) {
  AtomicString style_without_float =
      "width: 50px; height: 50px; background-color: green";
  AtomicString style_with_float = "float: left; " + style_without_float;
  SetBodyInnerHTML(
      "<div id='self-painting-layer' style='position: absolute'>"
      "  <div id='non-self-painting-layer' style='overflow: hidden'>"
      "    <div>"
      "      <div id='float' style='width: 10px; height: 10px; "
      "background-color: blue'></div>"
      "    </div>"
      "  </div>"
      "</div>");
  LayoutObject& float_div =
      *GetDocument().getElementById("float")->GetLayoutObject();
  ToHTMLElement(float_div.GetNode())
      ->setAttribute(HTMLNames::styleAttr, style_without_float);
  GetDocument().View()->UpdateAllLifecyclePhases();

  LayoutBoxModelObject& self_painting_layer_object = *ToLayoutBoxModelObject(
      GetDocument().getElementById("self-painting-layer")->GetLayoutObject());
  PaintLayer& self_painting_layer = *self_painting_layer_object.Layer();
  ASSERT_TRUE(self_painting_layer.IsSelfPaintingLayer());
  PaintLayer& non_self_painting_layer =
      *ToLayoutBoxModelObject(GetDocument()
                                  .getElementById("non-self-painting-layer")
                                  ->GetLayoutObject())
           ->Layer();
  ASSERT_FALSE(non_self_painting_layer.IsSelfPaintingLayer());
  ASSERT_TRUE(&non_self_painting_layer == float_div.EnclosingLayer());

  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseFloat());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseFloat());

  // needsPaintPhaseFloat should be set when any descendant on the same layer
  // has float.
  ToHTMLElement(float_div.GetNode())
      ->setAttribute(HTMLNames::styleAttr, style_with_float);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseFloat());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseFloat());
  Paint();
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), float_div,
      DisplayItem::kBoxDecorationBackground));

  // needsPaintPhaseFloat should be reset when there is no float actually
  // painted.
  ToHTMLElement(float_div.GetNode())
      ->setAttribute(HTMLNames::styleAttr, style_without_float);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseFloat());
}

TEST_P(PaintLayerPainterTest, PaintPhaseFloatUnderInlineLayer) {
  SetBodyInnerHTML(
      "<div id='self-painting-layer' style='position: absolute'>"
      "  <div id='non-self-painting-layer' style='overflow: hidden'>"
      "    <span id='span' style='position: relative'>"
      "      <div id='float' style='width: 10px; height: 10px; "
      "background-color: blue; float: left'></div>"
      "    </span>"
      "  </div>"
      "</div>");
  GetDocument().View()->UpdateAllLifecyclePhases();

  LayoutObject& float_div =
      *GetDocument().getElementById("float")->GetLayoutObject();
  LayoutBoxModelObject& span = *ToLayoutBoxModelObject(
      GetDocument().getElementById("span")->GetLayoutObject());
  PaintLayer& span_layer = *span.Layer();
  ASSERT_TRUE(&span_layer == float_div.EnclosingLayer());
  ASSERT_FALSE(span_layer.NeedsPaintPhaseFloat());
  LayoutBoxModelObject& self_painting_layer_object = *ToLayoutBoxModelObject(
      GetDocument().getElementById("self-painting-layer")->GetLayoutObject());
  PaintLayer& self_painting_layer = *self_painting_layer_object.Layer();
  ASSERT_TRUE(self_painting_layer.IsSelfPaintingLayer());
  PaintLayer& non_self_painting_layer =
      *ToLayoutBoxModelObject(GetDocument()
                                  .getElementById("non-self-painting-layer")
                                  ->GetLayoutObject())
           ->Layer();
  ASSERT_FALSE(non_self_painting_layer.IsSelfPaintingLayer());

  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseFloat());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseFloat());
  EXPECT_FALSE(span_layer.NeedsPaintPhaseFloat());
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), float_div,
      DisplayItem::kBoxDecorationBackground));
}

TEST_P(PaintLayerPainterTest, PaintPhaseBlockBackground) {
  AtomicString style_without_background = "width: 50px; height: 50px";
  AtomicString style_with_background =
      "background: blue; " + style_without_background;
  SetBodyInnerHTML(
      "<div id='self-painting-layer' style='position: absolute'>"
      "  <div id='non-self-painting-layer' style='overflow: hidden'>"
      "    <div>"
      "      <div id='background'></div>"
      "    </div>"
      "  </div>"
      "</div>");
  LayoutObject& background_div =
      *GetDocument().getElementById("background")->GetLayoutObject();
  ToHTMLElement(background_div.GetNode())
      ->setAttribute(HTMLNames::styleAttr, style_without_background);
  GetDocument().View()->UpdateAllLifecyclePhases();

  LayoutBoxModelObject& self_painting_layer_object = *ToLayoutBoxModelObject(
      GetDocument().getElementById("self-painting-layer")->GetLayoutObject());
  PaintLayer& self_painting_layer = *self_painting_layer_object.Layer();
  ASSERT_TRUE(self_painting_layer.IsSelfPaintingLayer());
  PaintLayer& non_self_painting_layer =
      *ToLayoutBoxModelObject(GetDocument()
                                  .getElementById("non-self-painting-layer")
                                  ->GetLayoutObject())
           ->Layer();
  ASSERT_FALSE(non_self_painting_layer.IsSelfPaintingLayer());
  ASSERT_TRUE(&non_self_painting_layer == background_div.EnclosingLayer());

  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseDescendantBlockBackgrounds());
  EXPECT_FALSE(
      non_self_painting_layer.NeedsPaintPhaseDescendantBlockBackgrounds());

  // Background on the self-painting-layer node itself doesn't affect
  // PaintPhaseDescendantBlockBackgrounds.
  ToHTMLElement(self_painting_layer_object.GetNode())
      ->setAttribute(HTMLNames::styleAttr,
                     "position: absolute; background: green");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseDescendantBlockBackgrounds());
  EXPECT_FALSE(
      non_self_painting_layer.NeedsPaintPhaseDescendantBlockBackgrounds());
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), self_painting_layer_object,
      DisplayItem::kBoxDecorationBackground));

  // needsPaintPhaseDescendantBlockBackgrounds should be set when any descendant
  // on the same layer has Background.
  ToHTMLElement(background_div.GetNode())
      ->setAttribute(HTMLNames::styleAttr, style_with_background);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseDescendantBlockBackgrounds());
  EXPECT_FALSE(
      non_self_painting_layer.NeedsPaintPhaseDescendantBlockBackgrounds());
  Paint();
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), background_div,
      DisplayItem::kBoxDecorationBackground));

  // needsPaintPhaseDescendantBlockBackgrounds should be reset when no outline
  // is actually painted.
  ToHTMLElement(background_div.GetNode())
      ->setAttribute(HTMLNames::styleAttr, style_without_background);
  GetDocument().View()->UpdateAllLifecyclePhases();
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
      !RuntimeEnabledFeatures::RootLayerScrollingEnabled()) {
    // In RootLayerScrolls+SPv1, the empty paint phase optimization doesn't
    // apply to the composited scrolling layer so we don't need this check.
    EXPECT_FALSE(
        self_painting_layer.NeedsPaintPhaseDescendantBlockBackgrounds());
  }
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnLayerRemoval) {
  SetBodyInnerHTML(
      "<div id='layer' style='position: relative'>"
      "  <div style='height: 100px'>"
      "    <div style='height: 20px; outline: 1px solid red; background-color: "
      "green'>outline and background</div>"
      "    <div style='float: left'>float</div>"
      "  </div>"
      "</div>");

  LayoutBoxModelObject& layer_div = *ToLayoutBoxModelObject(
      GetDocument().getElementById("layer")->GetLayoutObject());
  PaintLayer& layer = *layer_div.Layer();
  ASSERT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(layer.NeedsPaintPhaseFloat());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantBlockBackgrounds());

  PaintLayer& html_layer =
      *ToLayoutBoxModelObject(
           GetDocument().documentElement()->GetLayoutObject())
           ->Layer();
  EXPECT_FALSE(html_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(html_layer.NeedsPaintPhaseFloat());
  EXPECT_FALSE(html_layer.NeedsPaintPhaseDescendantBlockBackgrounds());

  ToHTMLElement(layer_div.GetNode())->setAttribute(HTMLNames::styleAttr, "");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_FALSE(layer_div.HasLayer());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseFloat());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnLayerAddition) {
  SetBodyInnerHTML(
      "<div id='will-be-layer'>"
      "  <div style='height: 100px'>"
      "    <div style='height: 20px; outline: 1px solid red; background-color: "
      "green'>outline and background</div>"
      "    <div style='float: left'>float</div>"
      "  </div>"
      "</div>");

  LayoutBoxModelObject& layer_div = *ToLayoutBoxModelObject(
      GetDocument().getElementById("will-be-layer")->GetLayoutObject());
  EXPECT_FALSE(layer_div.HasLayer());

  PaintLayer& html_layer =
      *ToLayoutBoxModelObject(
           GetDocument().documentElement()->GetLayoutObject())
           ->Layer();
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseFloat());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantBlockBackgrounds());

  ToHTMLElement(layer_div.GetNode())
      ->setAttribute(HTMLNames::styleAttr, "position: relative");
  GetDocument().View()->UpdateAllLifecyclePhases();
  ASSERT_TRUE(layer_div.HasLayer());
  PaintLayer& layer = *layer_div.Layer();
  ASSERT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(layer.NeedsPaintPhaseFloat());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnBecomingSelfPainting) {
  SetBodyInnerHTML(
      "<div id='will-be-self-painting' style='width: 100px; height: 100px; "
      "overflow: hidden'>"
      "  <div>"
      "    <div style='outline: 1px solid red; background-color: "
      "green'>outline and background</div>"
      "  </div>"
      "</div>");

  LayoutBoxModelObject& layer_div = *ToLayoutBoxModelObject(
      GetDocument().getElementById("will-be-self-painting")->GetLayoutObject());
  ASSERT_TRUE(layer_div.HasLayer());
  EXPECT_FALSE(layer_div.Layer()->IsSelfPaintingLayer());

  PaintLayer& html_layer =
      *ToLayoutBoxModelObject(
           GetDocument().documentElement()->GetLayoutObject())
           ->Layer();
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantBlockBackgrounds());

  ToHTMLElement(layer_div.GetNode())
      ->setAttribute(
          HTMLNames::styleAttr,
          "width: 100px; height: 100px; overflow: hidden; position: relative");
  GetDocument().View()->UpdateAllLifecyclePhases();
  PaintLayer& layer = *layer_div.Layer();
  ASSERT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnBecomingNonSelfPainting) {
  SetBodyInnerHTML(
      "<div id='will-be-non-self-painting' style='width: 100px; height: 100px; "
      "overflow: hidden; position: relative'>"
      "  <div>"
      "    <div style='outline: 1px solid red; background-color: "
      "green'>outline and background</div>"
      "  </div>"
      "</div>");

  LayoutBoxModelObject& layer_div =
      *ToLayoutBoxModelObject(GetDocument()
                                  .getElementById("will-be-non-self-painting")
                                  ->GetLayoutObject());
  ASSERT_TRUE(layer_div.HasLayer());
  PaintLayer& layer = *layer_div.Layer();
  EXPECT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantBlockBackgrounds());

  PaintLayer& html_layer =
      *ToLayoutBoxModelObject(
           GetDocument().documentElement()->GetLayoutObject())
           ->Layer();
  EXPECT_FALSE(html_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(html_layer.NeedsPaintPhaseDescendantBlockBackgrounds());

  ToHTMLElement(layer_div.GetNode())
      ->setAttribute(HTMLNames::styleAttr,
                     "width: 100px; height: 100px; overflow: hidden");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest,
       TableCollapsedBorderNeedsPaintPhaseDescendantBlockBackgrounds) {
  // TODO(wangxianzhu): Enable this test slimmingPaintInvalidation when its
  // fully functional.
  if (RuntimeEnabledFeatures::SlimmingPaintInvalidationEnabled())
    return;

  // "position: relative" makes the table and td self-painting layers.
  // The table's layer should be marked needsPaintPhaseDescendantBlockBackground
  // because it will paint collapsed borders in the phase.
  SetBodyInnerHTML(
      "<table id='table' style='position: relative; border-collapse: collapse'>"
      "  <tr><td style='position: relative; border: 1px solid "
      "green'>Cell</td></tr>"
      "</table>");

  LayoutBoxModelObject& table =
      *ToLayoutBoxModelObject(GetLayoutObjectByElementId("table"));
  ASSERT_TRUE(table.HasLayer());
  PaintLayer& layer = *table.Layer();
  EXPECT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest,
       TableCollapsedBorderNeedsPaintPhaseDescendantBlockBackgroundsDynamic) {
  // TODO(wangxianzhu): Enable this test slimmingPaintInvalidation when its
  // fully functional.
  if (RuntimeEnabledFeatures::SlimmingPaintInvalidationEnabled())
    return;

  SetBodyInnerHTML(
      "<table id='table' style='position: relative'>"
      "  <tr><td style='position: relative; border: 1px solid "
      "green'>Cell</td></tr>"
      "</table>");

  LayoutBoxModelObject& table =
      *ToLayoutBoxModelObject(GetLayoutObjectByElementId("table"));
  ASSERT_TRUE(table.HasLayer());
  PaintLayer& layer = *table.Layer();
  EXPECT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_FALSE(layer.NeedsPaintPhaseDescendantBlockBackgrounds());

  ToHTMLElement(table.GetNode())
      ->setAttribute(HTMLNames::styleAttr,
                     "position: relative; border-collapse: collapse");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantBlockBackgrounds());
}

TEST_P(PaintLayerPainterTest, DontPaintWithTinyOpacity) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001'></div>");
  ExpectPaintedOutputInvisible("target", true);
}

TEST_P(PaintLayerPainterTest, DoPaintWithTinyOpacityAndWillChangeOpacity) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001; "
      "will-change: opacity'></div>");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithTinyOpacityAndBackdropFilter) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001;"
      "  backdrop-filter: blur(2px);'></div>");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest,
       DoPaintWithTinyOpacityAndBackdropFilterAndWillChangeOpacity) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001;"
      "  backdrop-filter: blur(2px); will-change: opacity'></div>");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithCompositedTinyOpacity) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001;"
      " will-change: transform'></div>");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithNonTinyOpacity) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.1'></div>");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithEffectAnimationZeroOpacity) {
  SetBodyInnerHTML(
      "<style> "
      "div { "
      "  width: 100px; "
      "  height: 100px; "
      "  animation-name: example; "
      "  animation-duration: 4s; "
      "} "
      "@keyframes example { "
      "  from { opacity: 0.0;} "
      "  to { opacity: 1.0;} "
      "} "
      "</style> "
      "<div id='target'></div>");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithTransformAnimationZeroOpacity) {
  SetBodyInnerHTML(
      "<style> "
      "div#target { "
      "  animation-name: example; "
      "  animation-duration: 4s; "
      "  opacity: 0.0; "
      "} "
      "@keyframes example { "
      " from { transform: translate(0px, 0px); } "
      " to { transform: translate(3em, 0px); } "
      "} "
      "</style> "
      "<div id='target'>x</div></div>");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest,
       DoPaintWithTransformAnimationZeroOpacityWillChangeOpacity) {
  SetBodyInnerHTML(
      "<style> "
      "div#target { "
      "  animation-name: example; "
      "  animation-duration: 4s; "
      "  opacity: 0.0; "
      "  will-change: opacity; "
      "} "
      "@keyframes example { "
      " from { transform: translate(0px, 0px); } "
      " to { transform: translate(3em, 0px); } "
      "} "
      "</style> "
      "<div id='target'>x</div></div>");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithWillChangeOpacity) {
  SetBodyInnerHTML(
      "<style> "
      "div { "
      "  width: 100px; "
      "  height: 100px; "
      "  will-change: opacity;"
      "}"
      "</style> "
      "<div id='target'></div>");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithZeroOpacityAndWillChangeOpacity) {
  SetBodyInnerHTML(
      "<style> "
      "div { "
      "  width: 100px; "
      "  height: 100px; "
      "  opacity: 0; "
      "  will-change: opacity;"
      "}"
      "</style> "
      "<div id='target'></div>");
  ExpectPaintedOutputInvisible("target", false);
}

TEST_P(PaintLayerPainterTest,
       DoPaintWithNoContentAndZeroOpacityAndWillChangeOpacity) {
  SetBodyInnerHTML(
      "<style> "
      "div { "
      "  width: 100px; "
      "  height: 100px; "
      "  opacity: 0; "
      "  will-change: opacity;"
      "}"
      "</style> "
      "<div id='target'></div>");
  ExpectPaintedOutputInvisible("target", false);
}

}  // namespace blink
