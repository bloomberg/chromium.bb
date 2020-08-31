// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_layer_painter.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using testing::ElementsAre;
using testing::UnorderedElementsAre;

namespace blink {

class PaintLayerPainterTest : public PaintControllerPaintTest {
  USING_FAST_MALLOC(PaintLayerPainterTest);

 public:
  void ExpectPaintedOutputInvisibleAndPaintsWithTransparency(
      const char* element_name,
      bool expected_invisible,
      bool expected_paints_with_transparency) {
    // The optimization to skip painting for effectively-invisible content is
    // limited to pre-CAP.
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
      return;

    PaintLayer* target_layer =
        ToLayoutBox(GetLayoutObjectByElementId(element_name))->Layer();
    bool invisible = PaintLayerPainter::PaintedOutputInvisible(
        target_layer->GetLayoutObject().StyleRef());
    EXPECT_EQ(expected_invisible, invisible);
    EXPECT_EQ(expected_paints_with_transparency,
              target_layer->PaintsWithTransparency(kGlobalPaintNormalPhase));
  }

  PaintController& MainGraphicsLayerPaintController() {
    return GetLayoutView()
        .Layer()
        ->GraphicsLayerBacking(&GetLayoutView())
        ->GetPaintController();
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(PaintLayerPainterTest);

TEST_P(PaintLayerPainterTest, CachedSubsequenceAndChunksWithBackgrounds) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div id='container1' style='position: relative; z-index: 1;
        width: 200px; height: 200px; background-color: blue'>
      <div id='content1' style='position: absolute; width: 100px;
          height: 100px; background-color: red'></div>
    </div>
    <div id='filler1' style='position: relative; z-index: 2;
        width: 20px; height: 20px; background-color: gray'></div>
    <div id='container2' style='position: relative; z-index: 3;
        width: 200px; height: 200px; background-color: blue'>
      <div id='content2' style='position: absolute; width: 100px;
          height: 100px; background-color: green;'></div>
    </div>
    <div id='filler2' style='position: relative; z-index: 4;
        width: 20px; height: 20px; background-color: gray'></div>
  )HTML");

  auto* container1 = GetLayoutObjectByElementId("container1");
  auto* content1 = GetLayoutObjectByElementId("content1");
  auto* filler1 = GetLayoutObjectByElementId("filler1");
  auto* container2 = GetLayoutObjectByElementId("container2");
  auto* content2 = GetLayoutObjectByElementId("content2");
  auto* filler2 = GetLayoutObjectByElementId("filler2");
  const auto& view_client = ViewScrollingBackgroundClient();

  auto* container1_layer = ToLayoutBoxModelObject(container1)->Layer();
  auto* filler1_layer = ToLayoutBoxModelObject(filler1)->Layer();
  auto* container2_layer = ToLayoutBoxModelObject(container2)->Layer();
  auto* filler2_layer = ToLayoutBoxModelObject(filler2)->Layer();
  auto chunk_state = GetLayoutView().FirstFragment().ContentsProperties();

  auto check_results = [&]() {
    EXPECT_THAT(
        RootPaintController().GetDisplayItemList(),
        ElementsAre(IsSameId(&view_client, kDocumentBackgroundType),
                    IsSameId(GetDisplayItemClientFromLayoutObject(container1),
                             kBackgroundType),
                    IsSameId(GetDisplayItemClientFromLayoutObject(content1),
                             kBackgroundType),
                    IsSameId(GetDisplayItemClientFromLayoutObject(filler1),
                             kBackgroundType),
                    IsSameId(GetDisplayItemClientFromLayoutObject(container2),
                             kBackgroundType),
                    IsSameId(GetDisplayItemClientFromLayoutObject(content2),
                             kBackgroundType),
                    IsSameId(GetDisplayItemClientFromLayoutObject(filler2),
                             kBackgroundType)));

    // Check that new paint chunks were forced for the layers.
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      EXPECT_SUBSEQUENCE(*container1_layer, 2, 3);
      EXPECT_SUBSEQUENCE(*filler1_layer, 3, 4);
      EXPECT_SUBSEQUENCE(*container2_layer, 4, 5);
      EXPECT_SUBSEQUENCE(*filler2_layer, 5, 6);

      EXPECT_THAT(
          RootPaintController().PaintChunks(),
          ElementsAre(
              IsPaintChunk(0, 0), IsPaintChunk(0, 1),  // LayoutView chunks.
              IsPaintChunk(
                  1, 3,
                  PaintChunk::Id(*container1_layer, DisplayItem::kLayerChunk),
                  chunk_state, nullptr, IntRect(0, 0, 200, 200)),
              IsPaintChunk(
                  3, 4,
                  PaintChunk::Id(*filler1_layer, DisplayItem::kLayerChunk),
                  chunk_state, nullptr, IntRect(0, 200, 20, 20)),
              IsPaintChunk(
                  4, 6,
                  PaintChunk::Id(*container2_layer, DisplayItem::kLayerChunk),
                  chunk_state, nullptr, IntRect(0, 220, 200, 200)),
              IsPaintChunk(
                  6, 7,
                  PaintChunk::Id(*filler2_layer, DisplayItem::kLayerChunk),
                  chunk_state, nullptr, IntRect(0, 420, 20, 20))));
    } else {
      EXPECT_SUBSEQUENCE(*container1_layer, 1, 2);
      EXPECT_SUBSEQUENCE(*filler1_layer, 2, 3);
      EXPECT_SUBSEQUENCE(*container2_layer, 3, 4);
      EXPECT_SUBSEQUENCE(*filler2_layer, 4, 5);

      EXPECT_THAT(
          RootPaintController().PaintChunks(),
          ElementsAre(
              IsPaintChunk(0, 1),  // LayoutView.
              IsPaintChunk(
                  1, 3,
                  PaintChunk::Id(*container1_layer, DisplayItem::kLayerChunk),
                  chunk_state, nullptr, IntRect(0, 0, 200, 200)),
              IsPaintChunk(
                  3, 4,
                  PaintChunk::Id(*filler1_layer, DisplayItem::kLayerChunk),
                  chunk_state, nullptr, IntRect(0, 200, 20, 20)),
              IsPaintChunk(
                  4, 6,
                  PaintChunk::Id(*container2_layer, DisplayItem::kLayerChunk),
                  chunk_state, nullptr, IntRect(0, 220, 200, 200)),
              IsPaintChunk(
                  6, 7,
                  PaintChunk::Id(*filler2_layer, DisplayItem::kLayerChunk),
                  chunk_state, nullptr, IntRect(0, 420, 20, 20))));
    }
  };

  check_results();

  To<HTMLElement>(content1->GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     "position: absolute; width: 100px; height: 100px; "
                     "background-color: green");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(PaintWithoutCommit());
  EXPECT_EQ(6, NumCachedNewItems());

  CommitAndFinishCycle();

  // We should still have the paint chunks forced by the cached subsequences.
  check_results();
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceAndChunksWithoutBackgrounds) {
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      ::-webkit-scrollbar { display: none }
    </style>
    <div id='container' style='position: relative; z-index: 0;
        width: 150px; height: 150px; overflow: scroll'>
      <div id='content' style='position: relative; z-index: 1;
          width: 200px; height: 100px'>
        <div id='inner-content'
             style='position: absolute; width: 100px; height: 100px'></div>
      </div>
      <div id='filler' style='position: relative; z-index: 2;
          width: 300px; height: 300px'></div>
    </div>
  )HTML");

  auto* container = GetLayoutObjectByElementId("container");
  auto* content = GetLayoutObjectByElementId("content");
  auto* inner_content = GetLayoutObjectByElementId("inner-content");
  auto* filler = GetLayoutObjectByElementId("filler");
  const auto& view_client = ViewScrollingBackgroundClient();

  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&view_client, kDocumentBackgroundType)));

  auto* container_layer = ToLayoutBoxModelObject(container)->Layer();
  auto* content_layer = ToLayoutBoxModelObject(content)->Layer();
  auto* inner_content_layer = ToLayoutBoxModelObject(inner_content)->Layer();
  auto* filler_layer = ToLayoutBoxModelObject(filler)->Layer();

  EXPECT_SUBSEQUENCE(*container_layer, 2, 6);
  EXPECT_SUBSEQUENCE(*content_layer, 4, 5);
  EXPECT_SUBSEQUENCE(*filler_layer, 5, 6);

  auto container_properties =
      container->FirstFragment().LocalBorderBoxProperties();
  auto content_properties = container->FirstFragment().ContentsProperties();
  HitTestData scroll_hit_test;
  scroll_hit_test.scroll_translation = &content_properties.Transform();
  scroll_hit_test.scroll_hit_test_rect = IntRect(0, 0, 150, 150);

  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(
          IsPaintChunk(0, 0), IsPaintChunk(0, 1),  // LayoutView chunks.
          IsPaintChunk(
              1, 1, PaintChunk::Id(*container_layer, DisplayItem::kLayerChunk),
              container_properties, nullptr, IntRect(0, 0, 150, 150)),
          IsPaintChunk(
              1, 1, PaintChunk::Id(*container, DisplayItem::kScrollHitTest),
              container_properties, &scroll_hit_test, IntRect(0, 0, 150, 150)),
          IsPaintChunk(1, 1,
                       PaintChunk::Id(*content_layer, DisplayItem::kLayerChunk),
                       content_properties, nullptr, IntRect(0, 0, 200, 100)),
          IsPaintChunk(
              1, 1, PaintChunk::Id(*filler_layer, DisplayItem::kLayerChunk),
              content_properties, nullptr, IntRect(0, 100, 300, 300))));

  To<HTMLElement>(inner_content->GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     "position: absolute; width: 100px; height: 100px; "
                     "top: 100px; background-color: green");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      ElementsAre(IsSameId(&view_client, kDocumentBackgroundType),
                  IsSameId(GetDisplayItemClientFromLayoutObject(inner_content),
                           kBackgroundType)));

  EXPECT_SUBSEQUENCE(*container_layer, 2, 7);
  EXPECT_SUBSEQUENCE(*content_layer, 4, 6);
  EXPECT_SUBSEQUENCE(*filler_layer, 6, 7);

  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(
          IsPaintChunk(0, 0), IsPaintChunk(0, 1),  // LayoutView chunks.
          IsPaintChunk(
              1, 1, PaintChunk::Id(*container_layer, DisplayItem::kLayerChunk),
              container_properties, nullptr, IntRect(0, 0, 150, 150)),
          IsPaintChunk(
              1, 1, PaintChunk::Id(*container, DisplayItem::kScrollHitTest),
              container_properties, &scroll_hit_test, IntRect(0, 0, 150, 150)),
          IsPaintChunk(1, 1,
                       PaintChunk::Id(*content_layer, DisplayItem::kLayerChunk),
                       content_properties, nullptr, IntRect(0, 0, 200, 100)),
          IsPaintChunk(
              1, 2,
              PaintChunk::Id(*inner_content_layer, DisplayItem::kLayerChunk),
              content_properties, nullptr, IntRect(0, 100, 100, 100)),
          IsPaintChunk(
              2, 2, PaintChunk::Id(*filler_layer, DisplayItem::kLayerChunk),
              content_properties, nullptr, IntRect(0, 100, 300, 300))));
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceOnCullRectChange) {
  // This test doesn't work in CompositeAfterPaint mode because we always paint
  // from the local root frame view, and we always expand cull rect for
  // scrolling.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <div id='container1' style='position: relative; z-index: 1;
       width: 200px; height: 200px; background-color: blue'>
      <div id='content1' style='position: absolute; width: 100px;
          height: 100px; background-color: green'></div>
    </div>
    <div id='container2' style='position: relative; z-index: 1;
        width: 200px; height: 200px; background-color: blue'>
      <div id='content2a' style='position: absolute; width: 100px;
          height: 100px; background-color: green'></div>
      <div id='content2b' style='position: absolute; top: 200px;
          width: 100px; height: 100px; background-color: green'></div>
    </div>
    <div id='container3' style='position: absolute; z-index: 2;
        left: 300px; top: 0; width: 200px; height: 200px;
        background-color: blue'>
      <div id='content3' style='position: absolute; width: 200px;
          height: 200px; background-color: green'></div>
    </div>
  )HTML");
  InvalidateAll(RootPaintController());

  const DisplayItemClient& container1 =
      *GetDisplayItemClientFromElementId("container1");
  const DisplayItemClient& content1 =
      *GetDisplayItemClientFromElementId("content1");
  const DisplayItemClient& container2 =
      *GetDisplayItemClientFromElementId("container2");
  const DisplayItemClient& content2a =
      *GetDisplayItemClientFromElementId("content2a");
  const DisplayItemClient& content2b =
      *GetDisplayItemClientFromElementId("content2b");
  const DisplayItemClient& container3 =
      *GetDisplayItemClientFromElementId("container3");
  const DisplayItemClient& content3 =
      *GetDisplayItemClientFromElementId("content3");

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  Paint(IntRect(0, 0, 400, 300));

  const auto& background_display_item_client = ViewScrollingBackgroundClient();

  // Container1 is fully in the interest rect;
  // Container2 is partly (including its stacking chidren) in the interest rect;
  // Content2b is out of the interest rect and output nothing;
  // Container3 is partly in the interest rect.
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&background_display_item_client,
                                   kDocumentBackgroundType),
                          IsSameId(&container1, kBackgroundType),
                          IsSameId(&content1, kBackgroundType),
                          IsSameId(&container2, kBackgroundType),
                          IsSameId(&content2a, kBackgroundType),
                          IsSameId(&container3, kBackgroundType),
                          IsSameId(&content3, kBackgroundType)));

  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(PaintWithoutCommit(IntRect(0, 100, 300, 1000)));

  // Container1 becomes partly in the interest rect, but uses cached subsequence
  // because it was fully painted before;
  // Container2's intersection with the interest rect changes;
  // Content2b is out of the interest rect and outputs nothing;
  // Container3 becomes out of the interest rect and outputs empty subsequence
  // pair.
  EXPECT_EQ(5, NumCachedNewItems());

  CommitAndFinishCycle();

  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&background_display_item_client,
                                   kDocumentBackgroundType),
                          IsSameId(&container1, kBackgroundType),
                          IsSameId(&content1, kBackgroundType),
                          IsSameId(&container2, kBackgroundType),
                          IsSameId(&content2a, kBackgroundType),
                          IsSameId(&content2b, kBackgroundType)));
}

TEST_P(PaintLayerPainterTest,
       CachedSubsequenceOnCullRectChangeUnderInvalidationChecking) {
  ScopedPaintUnderInvalidationCheckingForTest under_invalidation_checking(true);

  SetBodyInnerHTML(R"HTML(
    <style>p { width: 200px; height: 50px; background: green }</style>
    <div id='target' style='position: relative; z-index: 1'>
      <p></p><p></p><p></p><p></p>
    </div>
  )HTML");
  InvalidateAll(RootPaintController());

  // |target| will be fully painted.
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  Paint(IntRect(0, 0, 400, 300));

  // |target| will be partially painted. Should not trigger under-invalidation
  // checking DCHECKs.
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  Paint(IntRect(0, 100, 300, 1000));
}

TEST_P(PaintLayerPainterTest,
       CachedSubsequenceOnStyleChangeWithCullRectClipping) {
  SetBodyInnerHTML(R"HTML(
    <div id='container1' style='position: relative; z-index: 1;
        width: 200px; height: 200px; background-color: blue'>
      <div id='content1' style='position: absolute; width: 100px;
          height: 100px; background-color: red'></div>
    </div>
    <div id='container2' style='position: relative; z-index: 1;
        width: 200px; height: 200px; background-color: blue'>
      <div id='content2' style='position: absolute; width: 100px;
          height: 100px; background-color: green'></div>
    </div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  // PaintResult of all subsequences will be MayBeClippedByCullRect.
  Paint(IntRect(0, 0, 50, 300));

  const DisplayItemClient& container1 =
      *GetDisplayItemClientFromElementId("container1");
  const DisplayItemClient& content1 =
      *GetDisplayItemClientFromElementId("content1");
  const DisplayItemClient& container2 =
      *GetDisplayItemClientFromElementId("container2");
  const DisplayItemClient& content2 =
      *GetDisplayItemClientFromElementId("content2");

  const auto& background_display_item_client = ViewScrollingBackgroundClient();
  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&background_display_item_client,
                                   kDocumentBackgroundType),
                          IsSameId(&container1, kBackgroundType),
                          IsSameId(&content1, kBackgroundType),
                          IsSameId(&container2, kBackgroundType),
                          IsSameId(&content2, kBackgroundType)));

  To<HTMLElement>(GetElementById("content1"))
      ->setAttribute(html_names::kStyleAttr,
                     "position: absolute; width: 100px; height: 100px; "
                     "background-color: green");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(PaintWithoutCommit(IntRect(0, 0, 50, 300)));
  EXPECT_EQ(4, NumCachedNewItems());

  CommitAndFinishCycle();

  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&background_display_item_client,
                                   kDocumentBackgroundType),
                          IsSameId(&container1, kBackgroundType),
                          IsSameId(&content1, kBackgroundType),
                          IsSameId(&container2, kBackgroundType),
                          IsSameId(&content2, kBackgroundType)));
}

TEST_P(PaintLayerPainterTest, CachedSubsequenceRetainsPreviousPaintResult) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html, body { height: 100%; margin: 0 }
      ::-webkit-scrollbar { display:none }
    </style>
    <div id="target" style="height: 8000px; contain: paint">
      <div id="content1" style="height: 100px; background: blue"></div>
      <div style="height: 6000px"></div>
      <div id="content2" style="height: 100px; background: blue"></div>
    </div>
    <div id="change" style="display: none"></div>
  )HTML");

  const auto* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  const auto* target_layer = target->Layer();
  const auto* content1 = GetLayoutObjectByElementId("content1");
  const auto* content2 = GetLayoutObjectByElementId("content2");
  const auto& view_client = ViewScrollingBackgroundClient();
  // |target| is partially painted.
  EXPECT_EQ(kMayBeClippedByCullRect, target_layer->PreviousPaintResult());
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // CAP doesn't clip the cull rect by the scrolling contents rect, which
    // doesn't affect painted results.
    EXPECT_EQ(CullRect(IntRect(-4000, -4000, 8800, 8600)),
              target_layer->PreviousCullRect());
    // |content2| is out of the cull rect.
    EXPECT_THAT(RootPaintController().GetDisplayItemList(),
                ElementsAre(IsSameId(&view_client, kDocumentBackgroundType),
                            IsSameId(content1, kBackgroundType)));
    // |target| created subsequence.
    EXPECT_SUBSEQUENCE(*target_layer, 2, 4);
    EXPECT_EQ(0u, RootPaintController().PaintChunks()[2].size());
    EXPECT_EQ(1u, RootPaintController().PaintChunks()[3].size());
  } else {
    EXPECT_EQ(CullRect(IntRect(0, 0, 800, 4600)),
              target_layer->PreviousCullRect());
    // |content2| is out of the cull rect.
    EXPECT_THAT(RootPaintController().GetDisplayItemList(),
                ElementsAre(IsSameId(&view_client, kDocumentBackgroundType),
                            IsSameId(content1, kBackgroundType)));
    // |target| created subsequence.
    EXPECT_SUBSEQUENCE(*target_layer, 1, 2);
    EXPECT_EQ(1u, RootPaintController().PaintChunks()[1].size());
  }

  // Change something that triggers a repaint but |target| should use cached
  // subsequence.
  GetDocument().getElementById("change")->setAttribute(html_names::kStyleAttr,
                                                       "display: block");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_FALSE(target_layer->SelfNeedsRepaint());
  EXPECT_TRUE(PaintWithoutCommit());
  EXPECT_EQ(2, NumCachedNewItems());
  CommitAndFinishCycle();

  // |target| is still partially painted.
  EXPECT_EQ(kMayBeClippedByCullRect, target_layer->PreviousPaintResult());
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // CAP doesn't clip the cull rect by the scrolling contents rect, which
    // doesn't affect painted results.
    EXPECT_EQ(CullRect(IntRect(-4000, -4000, 8800, 8600)),
              target_layer->PreviousCullRect());
    EXPECT_THAT(RootPaintController().GetDisplayItemList(),
                ElementsAre(IsSameId(&view_client, kDocumentBackgroundType),
                            IsSameId(content1, kBackgroundType)));
    // |target| still created subsequence (cached).
    EXPECT_SUBSEQUENCE(*target_layer, 2, 4);
    EXPECT_EQ(0u, RootPaintController().PaintChunks()[2].size());
    EXPECT_EQ(1u, RootPaintController().PaintChunks()[3].size());
  } else {
    EXPECT_EQ(CullRect(IntRect(0, 0, 800, 4600)),
              target_layer->PreviousCullRect());
    EXPECT_THAT(RootPaintController().GetDisplayItemList(),
                ElementsAre(IsSameId(&view_client, kDocumentBackgroundType),
                            IsSameId(content1, kBackgroundType)));
    // |target| still created subsequence (cached).
    EXPECT_SUBSEQUENCE(*target_layer, 1, 2);
    EXPECT_EQ(1u, RootPaintController().PaintChunks()[1].size());
  }

  // Scroll the view so that both |content1| and |content2| are in the interest
  // rect.
  GetLayoutView().GetScrollableArea()->SetScrollOffset(
      ScrollOffset(0, 3000), mojom::blink::ScrollType::kProgrammatic);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  // Scrolling doesn't set SelfNeedsRepaint flag. Change of paint dirty rect of
  // a partially painted layer will trigger repaint.
  EXPECT_FALSE(target_layer->SelfNeedsRepaint());
  EXPECT_TRUE(PaintWithoutCommit());
  EXPECT_EQ(2, NumCachedNewItems());
  CommitAndFinishCycle();

  // |target| is still partially painted.
  EXPECT_EQ(kMayBeClippedByCullRect, target_layer->PreviousPaintResult());
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    // CAP doesn't clip the cull rect by the scrolling contents rect, which
    // doesn't affect painted results.
    EXPECT_EQ(CullRect(IntRect(-4000, -1000, 8800, 8600)),
              target_layer->PreviousCullRect());
    // Painted result should include both |content1| and |content2|.
    EXPECT_THAT(RootPaintController().GetDisplayItemList(),
                ElementsAre(IsSameId(&view_client, kDocumentBackgroundType),
                            IsSameId(content1, kBackgroundType),
                            IsSameId(content2, kBackgroundType)));
    // |target| still created subsequence (repainted).
    EXPECT_SUBSEQUENCE(*target_layer, 2, 4);
    EXPECT_EQ(0u, RootPaintController().PaintChunks()[2].size());
    EXPECT_EQ(2u, RootPaintController().PaintChunks()[3].size());
  } else {
    EXPECT_EQ(CullRect(IntRect(0, 0, 800, 7600)),
              target_layer->PreviousCullRect());
    // Painted result should include both |content1| and |content2|.
    EXPECT_THAT(RootPaintController().GetDisplayItemList(),
                ElementsAre(IsSameId(&view_client, kDocumentBackgroundType),
                            IsSameId(content1, kBackgroundType),
                            IsSameId(content2, kBackgroundType)));
    // |target| still created subsequence (repainted).
    EXPECT_SUBSEQUENCE(*target_layer, 1, 2);
    EXPECT_EQ(2u, RootPaintController().PaintChunks()[1].size());
  }
}

TEST_P(PaintLayerPainterTest, HintedPaintChunksWithBackgrounds) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      div { background: blue }
    </style>
    <div id='container1' style='position: relative; height: 150px; z-index: 1'>
      <div id='content1a' style='position: relative; height: 100px'></div>
      <div id='content1b' style='position: relative; height: 100px'></div>
    </div>
    <div id='container2' style='position: relative; z-index: 1'>
      <div id='content2a' style='position: relative; height: 100px'></div>
      <div id='content2b' style='position: relative; z-index: -1; height: 100px'></div>
    </div>
  )HTML");

  auto* container1 = ToLayoutBox(GetLayoutObjectByElementId("container1"));
  auto* content1a = ToLayoutBox(GetLayoutObjectByElementId("content1a"));
  auto* content1b = ToLayoutBox(GetLayoutObjectByElementId("content1b"));
  auto* container2 = ToLayoutBox(GetLayoutObjectByElementId("container2"));
  auto* content2a = ToLayoutBox(GetLayoutObjectByElementId("content2a"));
  auto* content2b = ToLayoutBox(GetLayoutObjectByElementId("content2b"));
  auto chunk_state = GetLayoutView().FirstFragment().ContentsProperties();

  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                   kDocumentBackgroundType),
                          IsSameId(container1, kBackgroundType),
                          IsSameId(content1a, kBackgroundType),
                          IsSameId(content1b, kBackgroundType),
                          IsSameId(container2, kBackgroundType),
                          IsSameId(content2b, kBackgroundType),
                          IsSameId(content2a, kBackgroundType)));

  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    HitTestData scroll_hit_test;
    scroll_hit_test.scroll_translation = &chunk_state.Transform();
    scroll_hit_test.scroll_hit_test_rect = IntRect(0, 0, 800, 600);
    EXPECT_THAT(
        RootPaintController().PaintChunks(),
        ElementsAre(
            IsPaintChunk(
                0, 0,
                PaintChunk::Id(GetLayoutView(), DisplayItem::kScrollHitTest),
                GetLayoutView().FirstFragment().LocalBorderBoxProperties(),
                &scroll_hit_test),
            IsPaintChunk(0, 1,
                         PaintChunk::Id(ViewScrollingBackgroundClient(),
                                        kDocumentBackgroundType),
                         chunk_state),
            // Includes |container1| and |content1a|.
            IsPaintChunk(
                1, 3,
                PaintChunk::Id(*container1->Layer(), DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 0, 800, 150)),
            // Includes |content1b| which overflows |container1|.
            IsPaintChunk(
                3, 4,
                PaintChunk::Id(*content1b->Layer(), DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 100, 800, 100)),
            IsPaintChunk(
                4, 5,
                PaintChunk::Id(*container2->Layer(), DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 150, 800, 200)),
            IsPaintChunk(
                5, 6,
                PaintChunk::Id(*content2b->Layer(), DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 250, 800, 100)),
            IsPaintChunk(
                6, 7,
                PaintChunk::Id(*content2a->Layer(), DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 150, 800, 100))));
  } else {
    EXPECT_THAT(
        RootPaintController().PaintChunks(),
        ElementsAre(
            IsPaintChunk(0, 1,
                         PaintChunk::Id(ViewScrollingBackgroundClient(),
                                        kDocumentBackgroundType),
                         chunk_state),
            // Includes |container1| and |content1a|.
            IsPaintChunk(
                1, 3,
                PaintChunk::Id(*container1->Layer(), DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 0, 800, 150)),
            // Includes |content1b| which overflows |container1|.
            IsPaintChunk(
                3, 4,
                PaintChunk::Id(*content1b->Layer(), DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 100, 800, 100)),
            IsPaintChunk(
                4, 5,
                PaintChunk::Id(*container2->Layer(), DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 150, 800, 200)),
            IsPaintChunk(
                5, 6,
                PaintChunk::Id(*content2b->Layer(), DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 250, 800, 100)),
            IsPaintChunk(
                6, 7,
                PaintChunk::Id(*content2a->Layer(), DisplayItem::kLayerChunk),
                chunk_state, nullptr, IntRect(0, 150, 800, 100))));
  }
}

TEST_P(PaintLayerPainterTest, HintedPaintChunksWithoutBackgrounds) {
  if (!RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div id='container1' style='position: relative; height: 150px; z-index: 1'>
      <div id='content1a' style='position: relative; height: 100px'></div>
      <div id='content1b' style='position: relative; height: 100px'></div>
    </div>
    <div id='container2' style='position: relative; z-index: 1'>
      <div id='content2a' style='position: relative; height: 100px'></div>
      <div id='content2b'
           style='position: relative; z-index: -1; height: 100px'></div>
    </div>
  )HTML");

  auto* container1 = ToLayoutBox(GetLayoutObjectByElementId("container1"));
  auto* content1b = ToLayoutBox(GetLayoutObjectByElementId("content1b"));
  auto* container2 = ToLayoutBox(GetLayoutObjectByElementId("container2"));
  auto* content2a = ToLayoutBox(GetLayoutObjectByElementId("content2a"));
  auto* content2b = ToLayoutBox(GetLayoutObjectByElementId("content2b"));
  auto chunk_state = GetLayoutView().FirstFragment().ContentsProperties();

  EXPECT_THAT(RootPaintController().GetDisplayItemList(),
              ElementsAre(IsSameId(&ViewScrollingBackgroundClient(),
                                   kDocumentBackgroundType)));

  HitTestData scroll_hit_test;
  scroll_hit_test.scroll_translation = &chunk_state.Transform();
  scroll_hit_test.scroll_hit_test_rect = IntRect(0, 0, 800, 600);
  EXPECT_THAT(
      RootPaintController().PaintChunks(),
      ElementsAre(
          IsPaintChunk(
              0, 0,
              PaintChunk::Id(GetLayoutView(), DisplayItem::kScrollHitTest),
              GetLayoutView().FirstFragment().LocalBorderBoxProperties(),
              &scroll_hit_test),
          IsPaintChunk(0, 1,
                       PaintChunk::Id(ViewScrollingBackgroundClient(),
                                      kDocumentBackgroundType),
                       chunk_state),
          IsPaintChunk(
              1, 1,
              PaintChunk::Id(*container1->Layer(), DisplayItem::kLayerChunk),
              chunk_state, nullptr, IntRect(0, 0, 800, 150)),
          IsPaintChunk(
              1, 1,
              PaintChunk::Id(*content1b->Layer(), DisplayItem::kLayerChunk),
              chunk_state, nullptr, IntRect(0, 100, 800, 100)),
          IsPaintChunk(
              1, 1,
              PaintChunk::Id(*container2->Layer(), DisplayItem::kLayerChunk),
              chunk_state, nullptr, IntRect(0, 150, 800, 200)),
          IsPaintChunk(
              1, 1,
              PaintChunk::Id(*content2b->Layer(), DisplayItem::kLayerChunk),
              chunk_state, nullptr, IntRect(0, 250, 800, 100)),
          IsPaintChunk(
              1, 1,
              PaintChunk::Id(*content2a->Layer(), DisplayItem::kLayerChunk),
              chunk_state, nullptr, IntRect(0, 150, 800, 100))));
}

TEST_P(PaintLayerPainterTest, PaintPhaseOutline) {
  AtomicString style_without_outline =
      "width: 50px; height: 50px; background-color: green";
  AtomicString style_with_outline =
      "outline: 1px solid blue; " + style_without_outline;
  SetBodyInnerHTML(R"HTML(
    <div id='self-painting-layer' style='position: absolute'>
      <div id='non-self-painting-layer' style='overflow: hidden'>
        <div>
          <div id='outline'></div>
        </div>
      </div>
    </div>
  )HTML");
  LayoutObject& outline_div =
      *GetDocument().getElementById("outline")->GetLayoutObject();
  To<HTMLElement>(outline_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_outline);
  UpdateAllLifecyclePhasesForTest();

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
  To<HTMLElement>(self_painting_layer_object.GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     "position: absolute; outline: 1px solid green");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), self_painting_layer_object,
      DisplayItem::PaintPhaseToDrawingType(PaintPhase::kSelfOutlineOnly)));

  // needsPaintPhaseDescendantOutlines should be set when any descendant on the
  // same layer has outline.
  To<HTMLElement>(outline_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_with_outline);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseDescendantOutlines());
  Paint();
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), outline_div,
      DisplayItem::PaintPhaseToDrawingType(PaintPhase::kSelfOutlineOnly)));

  // needsPaintPhaseDescendantOutlines should be reset when no outline is
  // actually painted.
  To<HTMLElement>(outline_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_outline);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseDescendantOutlines());
}

TEST_P(PaintLayerPainterTest, PaintPhaseFloat) {
  AtomicString style_without_float =
      "width: 50px; height: 50px; background-color: green";
  AtomicString style_with_float = "float: left; " + style_without_float;
  SetBodyInnerHTML(R"HTML(
    <div id='self-painting-layer' style='position: absolute'>
      <div id='non-self-painting-layer' style='overflow: hidden'>
        <div>
          <div id='float' style='width: 10px; height: 10px;
              background-color: blue'></div>
        </div>
      </div>
    </div>
  )HTML");
  LayoutObject& float_div =
      *GetDocument().getElementById("float")->GetLayoutObject();
  To<HTMLElement>(float_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_float);
  UpdateAllLifecyclePhasesForTest();

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
  To<HTMLElement>(float_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_with_float);
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseFloat());
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseFloat());
  Paint();
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), float_div,
      DisplayItem::kBoxDecorationBackground));

  // needsPaintPhaseFloat should be reset when there is no float actually
  // painted.
  To<HTMLElement>(float_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, style_without_float);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseFloat());
}

TEST_P(PaintLayerPainterTest, PaintPhaseFloatUnderInlineLayer) {
  SetBodyInnerHTML(R"HTML(
    <div id='self-painting-layer' style='position: absolute'>
      <div id='non-self-painting-layer' style='overflow: hidden'>
        <span id='span' style='position: relative'>
          <div id='float' style='width: 10px; height: 10px;
              background-color: blue; float: left'></div>
        </span>
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  LayoutObject& float_div =
      *GetDocument().getElementById("float")->GetLayoutObject();
  LayoutBoxModelObject& span = *ToLayoutBoxModelObject(
      GetDocument().getElementById("span")->GetLayoutObject());
  PaintLayer& span_layer = *span.Layer();
  ASSERT_TRUE(&span_layer == float_div.EnclosingLayer());
  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    ASSERT_TRUE(span_layer.NeedsPaintPhaseFloat());
  } else {
    ASSERT_FALSE(span_layer.NeedsPaintPhaseFloat());
  }
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

  if (RuntimeEnabledFeatures::LayoutNGEnabled()) {
    EXPECT_FALSE(self_painting_layer.NeedsPaintPhaseFloat());
    EXPECT_TRUE(span_layer.NeedsPaintPhaseFloat());
  } else {
    EXPECT_TRUE(self_painting_layer.NeedsPaintPhaseFloat());
    EXPECT_FALSE(span_layer.NeedsPaintPhaseFloat());
  }
  EXPECT_FALSE(non_self_painting_layer.NeedsPaintPhaseFloat());
  EXPECT_TRUE(DisplayItemListContains(
      RootPaintController().GetDisplayItemList(), float_div,
      DisplayItem::kBoxDecorationBackground));
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnLayerAddition) {
  SetBodyInnerHTML(R"HTML(
    <div id='will-be-layer'>
      <div style='height: 100px'>
        <div style='height: 20px; outline: 1px solid red;
            background-color: green'>outline and background</div>
        <div style='float: left'>float</div>
      </div>
    </div>
  )HTML");

  LayoutBoxModelObject& layer_div = *ToLayoutBoxModelObject(
      GetDocument().getElementById("will-be-layer")->GetLayoutObject());
  EXPECT_FALSE(layer_div.HasLayer());

  PaintLayer& html_layer =
      *ToLayoutBoxModelObject(
           GetDocument().documentElement()->GetLayoutObject())
           ->Layer();
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseFloat());

  To<HTMLElement>(layer_div.GetNode())
      ->setAttribute(html_names::kStyleAttr, "position: relative");
  UpdateAllLifecyclePhasesForTest();
  ASSERT_TRUE(layer_div.HasLayer());
  PaintLayer& layer = *layer_div.Layer();
  ASSERT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantOutlines());
  EXPECT_TRUE(layer.NeedsPaintPhaseFloat());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnBecomingSelfPainting) {
  SetBodyInnerHTML(R"HTML(
    <div id='will-be-self-painting' style='width: 100px; height: 100px;
    overflow: hidden'>
      <div>
        <div style='outline: 1px solid red; background-color: green'>
          outline and background
        </div>
      </div>
    </div>
  )HTML");

  LayoutBoxModelObject& layer_div = *ToLayoutBoxModelObject(
      GetDocument().getElementById("will-be-self-painting")->GetLayoutObject());
  ASSERT_TRUE(layer_div.HasLayer());
  EXPECT_FALSE(layer_div.Layer()->IsSelfPaintingLayer());

  PaintLayer& html_layer =
      *ToLayoutBoxModelObject(
           GetDocument().documentElement()->GetLayoutObject())
           ->Layer();
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantOutlines());

  To<HTMLElement>(layer_div.GetNode())
      ->setAttribute(
          html_names::kStyleAttr,
          "width: 100px; height: 100px; overflow: hidden; position: relative");
  UpdateAllLifecyclePhasesForTest();
  PaintLayer& layer = *layer_div.Layer();
  ASSERT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantOutlines());
}

TEST_P(PaintLayerPainterTest, PaintPhasesUpdateOnBecomingNonSelfPainting) {
  SetBodyInnerHTML(R"HTML(
    <div id='will-be-non-self-painting' style='width: 100px; height: 100px;
    overflow: hidden; position: relative'>
      <div>
        <div style='outline: 1px solid red; background-color: green'>
          outline and background
        </div>
      </div>
    </div>
  )HTML");

  LayoutBoxModelObject& layer_div =
      *ToLayoutBoxModelObject(GetDocument()
                                  .getElementById("will-be-non-self-painting")
                                  ->GetLayoutObject());
  ASSERT_TRUE(layer_div.HasLayer());
  PaintLayer& layer = *layer_div.Layer();
  EXPECT_TRUE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(layer.NeedsPaintPhaseDescendantOutlines());

  PaintLayer& html_layer =
      *ToLayoutBoxModelObject(
           GetDocument().documentElement()->GetLayoutObject())
           ->Layer();
  EXPECT_FALSE(html_layer.NeedsPaintPhaseDescendantOutlines());

  To<HTMLElement>(layer_div.GetNode())
      ->setAttribute(html_names::kStyleAttr,
                     "width: 100px; height: 100px; overflow: hidden");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(layer.IsSelfPaintingLayer());
  EXPECT_TRUE(html_layer.NeedsPaintPhaseDescendantOutlines());
}

TEST_P(PaintLayerPainterTest, DontPaintWithTinyOpacity) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001'></div>");
  ExpectPaintedOutputInvisibleAndPaintsWithTransparency("target", true, true);
}

TEST_P(PaintLayerPainterTest, DoPaintWithTinyOpacityAndWillChangeOpacity) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001; "
      "    will-change: opacity'></div>");
  ExpectPaintedOutputInvisibleAndPaintsWithTransparency("target", false, false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithTinyOpacityAndBackdropFilter) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001;"
      "    backdrop-filter: blur(2px);'></div>");
  ExpectPaintedOutputInvisibleAndPaintsWithTransparency("target", false, false);
}

TEST_P(PaintLayerPainterTest,
       DoPaintWithTinyOpacityAndBackdropFilterAndWillChangeOpacity) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001;"
      "    backdrop-filter: blur(2px); will-change: opacity'></div>");
  ExpectPaintedOutputInvisibleAndPaintsWithTransparency("target", false, false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithCompositedTinyOpacity) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.0001;"
      "    will-change: transform'></div>");
  ExpectPaintedOutputInvisibleAndPaintsWithTransparency("target", true, false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithNonTinyOpacity) {
  SetBodyInnerHTML(
      "<div id='target' style='background: blue; opacity: 0.1'></div>");
  ExpectPaintedOutputInvisibleAndPaintsWithTransparency("target", false, true);
}

TEST_P(PaintLayerPainterTest, DoPaintWithEffectAnimationZeroOpacity) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
      animation-name: example;
      animation-duration: 4s;
    }
    @keyframes example {
      from { opacity: 0.0;}
      to { opacity: 1.0;}
    }
    </style>
    <div id='target'></div>
  )HTML");
  ExpectPaintedOutputInvisibleAndPaintsWithTransparency("target", true, false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithTransformAnimationZeroOpacity) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div#target {
      animation-name: example;
      animation-duration: 4s;
      opacity: 0.0;
    }
    @keyframes example {
      from { transform: translate(0px, 0px); }
      to { transform: translate(3em, 0px); }
    }
    </style>
    <div id='target'>x</div></div>
  )HTML");
  ExpectPaintedOutputInvisibleAndPaintsWithTransparency("target", true, false);
}

TEST_P(PaintLayerPainterTest,
       DoPaintWithTransformAnimationZeroOpacityWillChangeOpacity) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div#target {
      animation-name: example;
      animation-duration: 4s;
      opacity: 0.0;
      will-change: opacity;
    }
    @keyframes example {
      from { transform: translate(0px, 0px); }
      to { transform: translate(3em, 0px); }
    }
    </style>
    <div id='target'>x</div></div>
  )HTML");
  ExpectPaintedOutputInvisibleAndPaintsWithTransparency("target", false, false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithWillChangeOpacity) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
      will-change: opacity;
    }
    </style>
    <div id='target'></div>
  )HTML");
  ExpectPaintedOutputInvisibleAndPaintsWithTransparency("target", false, false);
}

TEST_P(PaintLayerPainterTest, DoPaintWithZeroOpacityAndWillChangeOpacity) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
      opacity: 0;
      will-change: opacity;
    }
    </style>
    <div id='target'></div>
  )HTML");
  ExpectPaintedOutputInvisibleAndPaintsWithTransparency("target", false, false);
}

TEST_P(PaintLayerPainterTest,
       DoPaintWithNoContentAndZeroOpacityAndWillChangeOpacity) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      width: 100px;
      height: 100px;
      opacity: 0;
      will-change: opacity;
    }
    </style>
    <div id='target'></div>
  )HTML");
  ExpectPaintedOutputInvisibleAndPaintsWithTransparency("target", false, false);
}

using PaintLayerPainterTestCAP = PaintLayerPainterTest;

INSTANTIATE_CAP_TEST_SUITE_P(PaintLayerPainterTestCAP);

TEST_P(PaintLayerPainterTestCAP, SimpleCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
         style='width: 200px; height: 200px; position: relative'>
    </div>
  )HTML");

  EXPECT_EQ(IntRect(0, 0, 800, 600),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());
}

TEST_P(PaintLayerPainterTestCAP, TallLayerCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
         style='width: 200px; height: 10000px; position: relative'>
    </div>
  )HTML");

  // Viewport rect (0, 0, 800, 600) expanded by 4000 for scrolling.
  EXPECT_EQ(IntRect(-4000, -4000, 8800, 8600),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());
}

TEST_P(PaintLayerPainterTestCAP, WideLayerCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
         style='width: 10000px; height: 200px; position: relative'>
    </div>
  )HTML");

  // Same as TallLayerCullRect.
  EXPECT_EQ(IntRect(-4000, -4000, 8800, 8600),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());
}

TEST_P(PaintLayerPainterTestCAP, TallScrolledLayerCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='width: 200px; height: 10000px; position: relative'>
    </div>
  )HTML");

  // Viewport rect (0, 0, 800, 600) expanded by 4000.
  EXPECT_EQ(IntRect(-4000, -4000, 8800, 8600),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 6000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(IntRect(-4000, 2000, 8800, 8600),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 6500), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  // Used the previous cull rect because the scroll amount is small.
  EXPECT_EQ(IntRect(-4000, 2000, 8800, 8600),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 6600), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  // Used new cull rect.
  EXPECT_EQ(IntRect(-4000, 2600, 8800, 8600),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());
}

TEST_P(PaintLayerPainterTestCAP, WholeDocumentCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  GetDocument().GetSettings()->SetMainFrameClipsContent(false);
  SetBodyInnerHTML(R"HTML(
    <style>
      div { background: blue; }
      ::-webkit-scrollbar { display: none; }
    </style>
    <div id='relative'
         style='width: 200px; height: 10000px; position: relative'>
    </div>
    <div id='fixed' style='width: 200px; height: 200px; position: fixed'>
    </div>
    <div id='scroll' style='width: 200px; height: 200px; overflow: scroll'>
      <div id='below-scroll' style='height: 5000px; position: relative'></div>
      <div style='height: 200px'>Should not paint</div>
    </div>
    <div id='normal' style='width: 200px; height: 200px'></div>
  )HTML");

  // Viewport clipping is disabled.
  EXPECT_TRUE(GetLayoutView().Layer()->PreviousCullRect().IsInfinite());
  EXPECT_TRUE(
      GetPaintLayerByElementId("relative")->PreviousCullRect().IsInfinite());
  EXPECT_TRUE(
      GetPaintLayerByElementId("fixed")->PreviousCullRect().IsInfinite());
  EXPECT_TRUE(
      GetPaintLayerByElementId("scroll")->PreviousCullRect().IsInfinite());

  // Cull rect is normal for contents below scroll other than the viewport.
  EXPECT_EQ(
      IntRect(-4000, -4000, 8200, 8200),
      GetPaintLayerByElementId("below-scroll")->PreviousCullRect().Rect());

  EXPECT_THAT(
      RootPaintController().GetDisplayItemList(),
      UnorderedElementsAre(
          IsSameId(&ViewScrollingBackgroundClient(), kDocumentBackgroundType),
          IsSameId(GetDisplayItemClientFromElementId("relative"),
                   kBackgroundType),
          IsSameId(GetDisplayItemClientFromElementId("normal"),
                   kBackgroundType),
          IsSameId(GetDisplayItemClientFromElementId("scroll"),
                   kBackgroundType),
          IsSameId(&ToLayoutBox(GetLayoutObjectByElementId("scroll"))
                        ->GetScrollableArea()
                        ->GetScrollingBackgroundDisplayItemClient(),
                   kBackgroundType),
          IsSameId(GetDisplayItemClientFromElementId("below-scroll"),
                   kBackgroundType),
          IsSameId(GetDisplayItemClientFromElementId("fixed"),
                   kBackgroundType)));
}

TEST_P(PaintLayerPainterTestCAP, VerticalRightLeftWritingModeDocument) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html { writing-mode: vertical-rl; }
      body { margin: 0; }
    </style>
    <div id='target' style='width: 10000px; height: 200px; position: relative'>
    </div>
  )HTML");

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(-5000, 0), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

  // A scroll by -5000px is equivalent to a scroll by (10000 - 5000 - 800)px =
  // 4200px in non-RTL mode. Expanding the resulting rect by 4000px in each
  // direction yields this result.
  EXPECT_EQ(IntRect(200, -4000, 8800, 8600),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());
}

TEST_P(PaintLayerPainterTestCAP, ScaledCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <div style='width: 200px; height: 300px; overflow: scroll;
                transform: scaleX(2) scaleY(0.5)'>
      <div id='target' style='height: 400px; position: relative'></div>
    </div>
  )HTML");

  // The scale doesn't affect the cull rect.
  EXPECT_EQ(IntRect(-4000, -4000, 8200, 8300),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());
}

TEST_P(PaintLayerPainterTestCAP, ScaledAndRotatedCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <div style='width: 200px; height: 300px; overflow: scroll;
                transform: scaleX(2) scaleY(0.5) rotateZ(45deg)'>
      <div id='target' style='height: 400px; position: relative'></div>
    </div>
  )HTML");

  // The scale and the rotation don't affect the cull rect.
  EXPECT_EQ(IntRect(-4000, -4000, 8200, 8300),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());
}

TEST_P(PaintLayerPainterTestCAP, 3DRotated90DegreesCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <div style='width: 200px; height: 300px; overflow: scroll;
                transform: rotateY(90deg)'>
      <div id='target' style='height: 400px; position: relative'></div>
    </div>
  )HTML");

  // It's rotated 90 degrees about the X axis, which means its visual content
  // rect is empty, we fall back to the 4000px cull rect padding amount.
  EXPECT_EQ(IntRect(-4000, -4000, 8200, 8300),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());
}

TEST_P(PaintLayerPainterTestCAP, 3DRotatedNear90DegreesCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <div style='width: 200px; height: 300px; overflow: scroll;
                transform: rotateY(89.9999deg)'>
      <div id='target' style='height: 400px; position: relative'></div>
    </div>
  )HTML");

  // Because the layer is rotated to almost 90 degrees, floating-point error
  // leads to a reverse-projected rect that is much much larger than the
  // original layer size in certain dimensions. In such cases, we often fall
  // back to the 4000px cull rect padding amount.
  EXPECT_EQ(IntRect(-4000, -4000, 8200, 8300),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());
}

TEST_P(PaintLayerPainterTestCAP, PerspectiveCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
         style='width: 100px; height: 100px; transform: perspective(1000px)'>
    </div>
  )HTML");

  // Use infinite cull rect with perspective.
  EXPECT_TRUE(
      GetPaintLayerByElementId("target")->PreviousCullRect().IsInfinite());
}

TEST_P(PaintLayerPainterTestCAP, 3D45DegRotatedTallCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id='target'
         style='width: 200px; height: 10000px; transform: rotateY(45deg)'>
    </div>
  )HTML");

  // Use infinite cull rect with 3d transform.
  EXPECT_TRUE(
      GetPaintLayerByElementId("target")->PreviousCullRect().IsInfinite());
}

TEST_P(PaintLayerPainterTestCAP, FixedPositionCullRect) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='width: 1000px; height: 2000px;
                            position: fixed; top: 100px; left: 200px;'>
    </div>
  )HTML");

  EXPECT_EQ(IntRect(-200, -100, 800, 600),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());
}

TEST_P(PaintLayerPainterTestCAP, LayerOffscreenNearCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <div style='width: 200px; height: 300px; overflow: scroll;
                position: absolute; top: 3000px; left: 0px;'>
      <div id='target' style='height: 500px; position: relative'></div>
    </div>
  )HTML");

  EXPECT_EQ(IntRect(-4000, -4000, 8200, 8300),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());
}

TEST_P(PaintLayerPainterTestCAP, LayerOffscreenFarCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <div style='width: 200px; height: 300px; overflow: scroll;
                position: absolute; top: 9000px'>
      <div id='target' style='height: 500px; position: relative'></div>
    </div>
  )HTML");

  // The layer is too far away from the viewport.
  EXPECT_EQ(IntRect(),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());
}

TEST_P(PaintLayerPainterTestCAP, ScrollingLayerCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <style>
      div::-webkit-scrollbar { width: 5px; }
    </style>
    <div style='width: 200px; height: 200px; overflow: scroll'>
      <div id='target'
           style='width: 100px; height: 10000px; position: relative'>
      </div>
    </div>
  )HTML");

  // In screen space, the scroller is (8, 8, 195, 193) (because of overflow clip
  // of 'target', scrollbar and root margin).
  // Applying the viewport clip of the root has no effect because
  // the clip is already small. Mapping it down into the graphics layer
  // space yields (0, 0, 195, 193). This is then expanded by 4000px.
  EXPECT_EQ(IntRect(-4000, -4000, 8195, 8193),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());
}

TEST_P(PaintLayerPainterTestCAP, NonCompositedScrollingLayerCullRect) {
  GetDocument().GetSettings()->SetPreferCompositingToLCDTextEnabled(false);
  SetBodyInnerHTML(R"HTML(
    <style>
      div::-webkit-scrollbar { width: 5px; }
    </style>
    <div style='width: 200px; height: 200px; overflow: scroll'>
      <div id='target'
           style='width: 100px; height: 10000px; position: relative'>
      </div>
    </div>
  )HTML");

  // See ScrollingLayerCullRect for the calculation.
  EXPECT_EQ(IntRect(0, 0, 195, 193),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());
}

TEST_P(PaintLayerPainterTestCAP, ClippedBigLayer) {
  SetBodyInnerHTML(R"HTML(
    <div style='width: 1px; height: 1px; overflow: hidden'>
      <div id='target'
           style='width: 10000px; height: 10000px; position: relative'>
      </div>
    </div>
  )HTML");

  // The viewport is not scrollable because of the clip, so the cull rect is
  // just the viewport rect.
  EXPECT_EQ(IntRect(0, 0, 800, 600),
            GetPaintLayerByElementId("target")->PreviousCullRect().Rect());
}

}  // namespace blink
