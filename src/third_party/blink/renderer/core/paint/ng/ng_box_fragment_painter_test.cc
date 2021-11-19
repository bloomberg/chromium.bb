// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"

#include "components/paint_preview/common/paint_preview_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using testing::ElementsAre;

namespace blink {

namespace {

void ExtractLinks(const cc::PaintOpBuffer* buffer, std::vector<GURL>* links) {
  for (cc::PaintOpBuffer::Iterator it(buffer); it; ++it) {
    if (it->GetType() == cc::PaintOpType::Annotate) {
      auto* annotate_op = static_cast<cc::AnnotateOp*>(*it);
      links->push_back(GURL(
          std::string(reinterpret_cast<const char*>(annotate_op->data->data()),
                      annotate_op->data->size())));
    } else if (it->GetType() == cc::PaintOpType::DrawRecord) {
      auto* record_op = static_cast<cc::DrawRecordOp*>(*it);
      ExtractLinks(record_op->record.get(), links);
    }
  }
}

}  // namespace

class NGBoxFragmentPainterTest : public PaintControllerPaintTest,
                                 private ScopedLayoutNGForTest {
 public:
  NGBoxFragmentPainterTest(LocalFrameClient* local_frame_client = nullptr)
      : PaintControllerPaintTest(local_frame_client),
        ScopedLayoutNGForTest(true) {}
};

INSTANTIATE_PAINT_TEST_SUITE_P(NGBoxFragmentPainterTest);

TEST_P(NGBoxFragmentPainterTest, ScrollHitTestOrder) {
  GetPage().GetSettings().SetPreferCompositingToLCDTextEnabled(false);
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0; }
      #scroller {
        width: 40px;
        height: 40px;
        overflow: scroll;
        font-size: 500px;
      }
    </style>
    <div id='scroller'>TEXT</div>
  )HTML");
  auto& scroller = *GetLayoutBoxByElementId("scroller");
  const DisplayItemClient& root_fragment = scroller;

  NGInlineCursor cursor;
  cursor.MoveTo(*scroller.SlowFirstChild());
  const DisplayItemClient& text_fragment =
      *cursor.Current().GetDisplayItemClient();

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(text_fragment.Id(), kForegroundType)));
  HitTestData scroll_hit_test;
  scroll_hit_test.scroll_translation =
      scroller.FirstFragment().PaintProperties()->ScrollTranslation();
  scroll_hit_test.scroll_hit_test_rect = gfx::Rect(0, 0, 40, 40);
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
    EXPECT_THAT(
        ContentPaintChunks(),
        ElementsAre(
            VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
            IsPaintChunk(1, 1,
                         PaintChunk::Id(scroller.Layer()->Id(),
                                        DisplayItem::kLayerChunk),
                         scroller.FirstFragment().LocalBorderBoxProperties()),
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(root_fragment.Id(), DisplayItem::kScrollHitTest),
                scroller.FirstFragment().LocalBorderBoxProperties(),
                &scroll_hit_test, gfx::Rect(0, 0, 40, 40)),
            IsPaintChunk(1, 2)));
  } else {
    EXPECT_THAT(
        ContentPaintChunks(),
        ElementsAre(
            VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
            IsPaintChunk(
                1, 1,
                PaintChunk::Id(root_fragment.Id(), DisplayItem::kScrollHitTest),
                scroller.FirstFragment().LocalBorderBoxProperties(),
                &scroll_hit_test, gfx::Rect(0, 0, 40, 40)),
            IsPaintChunk(1, 2)));
  }
}

TEST_P(NGBoxFragmentPainterTest, AddUrlRects) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div>
      <p>
        <a href="https://www.chromium.org">Chromium</a>
      </p>
      <p>
        <a href="https://www.wikipedia.org">Wikipedia</a>
      </p>
    </div>
  )HTML");
  // Use Paint Preview to test this as printing falls back to the legacy layout
  // engine.

  // PaintPreviewTracker records URLs via the GraphicsContext under certain
  // flagsets when painting. This is the simplest way to check if URLs were
  // annotated.
  Document::PaintPreviewScope paint_preview(GetDocument(),
                                            Document::kPaintingPreview);
  UpdateAllLifecyclePhasesForTest();

  paint_preview::PaintPreviewTracker tracker(base::UnguessableToken::Create(),
                                             absl::nullopt, true);
  auto* builder = MakeGarbageCollected<PaintRecordBuilder>();
  builder->Context().SetPaintPreviewTracker(&tracker);

  GetDocument().View()->PaintContentsOutsideOfLifecycle(
      builder->Context(),
      kGlobalPaintNormalPhase | kGlobalPaintAddUrlMetadata |
          kGlobalPaintFlattenCompositingLayers,
      CullRect::Infinite());

  auto record = builder->EndRecording();
  std::vector<GURL> links;
  ExtractLinks(record.get(), &links);
  ASSERT_EQ(links.size(), 2U);
  EXPECT_EQ(links[0].spec(), "https://www.chromium.org/");
  EXPECT_EQ(links[1].spec(), "https://www.wikipedia.org/");
}

TEST_P(NGBoxFragmentPainterTest, SelectionTablePainting) {
  // This test passes if it does not timeout
  // Repro case of crbug.com/1182106.
  SetBodyInnerHTML(R"HTML(
    <!doctype html>
    <table id="t1"><tbody id="b1"><tr id="r1"><td id="c1">
    <table id="t2"><tbody id="b2"><tr id="r2"><td id="c2">
    <table id="t3"><tbody id="b3"><tr id="r3"><td id="c3">
    <table id="t4"><tbody id="b4"><tr id="r4"><td id="c4">
    <table id="t5"><tbody id="b5"><tr id="r5"><td id="c5">
      <table id="target">
        <tbody id="b6">
          <tr id="r6"> <!-- 8388608 steps-->
            <td id="c6.1">
              <table id="t7">
                <tbody id="b7">
                  <tr id="r7">
                    <td><img src="./resources/blue-100.png" style="width:100px">Drag me</td>
                  </tr>
                </tbody>
              </table>
            </td>
            <td id="c6.2">
              <table id="t8" style="float:left;width:100%">
                <tbody id="b8">
                  <tr id="r8">
                    <td id="c8">Float</td>
                  </tr>
                </tbody>
              </table>
            </td>
          </tr>
        </tbody>
      </table>
    </td></tr></tbody></table>
    </td></tr></tbody></table>
    </td></tr></tbody></table>
    </td></tr></tbody></table>
    </td></tr></tbody></table>
  )HTML");
  // Drag image will only paint if there is selection.
  GetDocument().View()->GetFrame().Selection().SelectAll();
  GetDocument().GetLayoutView()->CommitPendingSelection();
  UpdateAllLifecyclePhasesForTest();
  auto* builder = MakeGarbageCollected<PaintRecordBuilder>();
  GetDocument().View()->PaintContentsOutsideOfLifecycle(
      builder->Context(),
      kGlobalPaintSelectionDragImageOnly | kGlobalPaintFlattenCompositingLayers,
      CullRect::Infinite());

  auto record = builder->EndRecording();
}

TEST_P(NGBoxFragmentPainterTest, ClippedText) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="overflow: hidden; position: relative;
                            width: 100px; height: 100px">
      A<br>B<br>C<br>D
    </div>
  )HTML");
  // Initially all the texts are painted.
  auto num_all_display_items = ContentDisplayItems().size();
  auto* target = GetDocument().getElementById("target");

  target->SetInlineStyleProperty(CSSPropertyID::kHeight, "0px");
  UpdateAllLifecyclePhasesForTest();
  // None of the texts should be painted.
  EXPECT_EQ(num_all_display_items - 4, ContentDisplayItems().size());

  target->SetInlineStyleProperty(CSSPropertyID::kHeight, "1px");
  UpdateAllLifecyclePhasesForTest();
  // Only "A" should be painted.
  EXPECT_EQ(num_all_display_items - 3, ContentDisplayItems().size());
}

TEST_P(NGBoxFragmentPainterTest, NodeAtPointWithSvgInline) {
  ScopedSVGTextNGForTest enable_svg_text_ng(true);
  SetBodyInnerHTML(R"HTML(
<svg xmlns="http://www.w3.org/2000/svg" width="900" height="900"
     viewBox="0 0 100 100" id="svg">
 <g font-size="13">
  <text x="10%" y="25%" id="pass">Expected paragraph.</text>
  <text x="10%" y="54%">
  <tspan id="fail">Should not be selected.</tspan>
  </text>
 </g>
</svg>)HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* root = GetDocument().getElementById("svg")->GetLayoutBox();
  HitTestResult result;
  root->NodeAtPoint(result, HitTestLocation(FloatPoint(256, 192)),
                    PhysicalOffset(0, 0), kHitTestForeground);
  EXPECT_EQ(GetDocument().getElementById("pass"), result.InnerElement());
}

}  // namespace blink
