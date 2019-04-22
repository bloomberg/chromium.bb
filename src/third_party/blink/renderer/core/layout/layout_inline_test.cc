// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_inline.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

using ::testing::UnorderedElementsAre;

class LayoutInlineTest : public RenderingTest {};

// Helper class to run the same test code with and without LayoutNG
class ParameterizedLayoutInlineTest : public testing::WithParamInterface<bool>,
                                      private ScopedLayoutNGForTest,
                                      public LayoutInlineTest {
 public:
  ParameterizedLayoutInlineTest() : ScopedLayoutNGForTest(GetParam()) {}

 protected:
  bool LayoutNGEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All, ParameterizedLayoutInlineTest, testing::Bool());

TEST_P(ParameterizedLayoutInlineTest, LinesBoundingBox) {
  LoadAhem();
  SetBodyInnerHTML(
      "<style>"
      "html { font-family: Ahem; font-size: 13px; }"
      // LayoutNG requires box decorations at this moment. crbug.com/789390
      "span { background-color: yellow; }"
      ".vertical { writing-mode: vertical-rl; }"
      "</style>"
      "<p><span id=ltr1>abc<br>xyz</span></p>"
      "<p><span id=ltr2>12 345 6789</span></p>"
      "<p dir=rtl><span id=rtl1>abc<br>xyz</span></p>"
      "<p dir=rtl><span id=rtl2>12 345 6789</span></p>"
      "<p class=vertical><span id=vertical>abc<br>xyz</span></p>");
  EXPECT_EQ(
      LayoutRect(LayoutPoint(0, 0), LayoutSize(39, 26)),
      ToLayoutInline(GetLayoutObjectByElementId("ltr1"))->LinesBoundingBox());
  EXPECT_EQ(
      LayoutRect(LayoutPoint(0, 0), LayoutSize(143, 13)),
      ToLayoutInline(GetLayoutObjectByElementId("ltr2"))->LinesBoundingBox());
  EXPECT_EQ(
      LayoutRect(LayoutPoint(745, 0), LayoutSize(39, 26)),
      ToLayoutInline(GetLayoutObjectByElementId("rtl1"))->LinesBoundingBox());
  EXPECT_EQ(
      LayoutRect(LayoutPoint(641, 0), LayoutSize(143, 13)),
      ToLayoutInline(GetLayoutObjectByElementId("rtl2"))->LinesBoundingBox());
  EXPECT_EQ(LayoutRect(LayoutPoint(0, 0), LayoutSize(26, 39)),
            ToLayoutInline(GetLayoutObjectByElementId("vertical"))
                ->LinesBoundingBox());
}

TEST_F(LayoutInlineTest, SimpleContinuation) {
  SetBodyInnerHTML(
      "<span id='splitInline'><i id='before'></i><h1 id='blockChild'></h1><i "
      "id='after'></i></span>");

  LayoutInline* split_inline_part1 =
      ToLayoutInline(GetLayoutObjectByElementId("splitInline"));
  ASSERT_TRUE(split_inline_part1);
  ASSERT_TRUE(split_inline_part1->FirstChild());
  EXPECT_EQ(split_inline_part1->FirstChild(),
            GetLayoutObjectByElementId("before"));
  EXPECT_FALSE(split_inline_part1->FirstChild()->NextSibling());

  auto* block = To<LayoutBlockFlow>(split_inline_part1->Continuation());
  ASSERT_TRUE(block);
  ASSERT_TRUE(block->FirstChild());
  EXPECT_EQ(block->FirstChild(), GetLayoutObjectByElementId("blockChild"));
  EXPECT_FALSE(block->FirstChild()->NextSibling());

  LayoutInline* split_inline_part2 = ToLayoutInline(block->Continuation());
  ASSERT_TRUE(split_inline_part2);
  ASSERT_TRUE(split_inline_part2->FirstChild());
  EXPECT_EQ(split_inline_part2->FirstChild(),
            GetLayoutObjectByElementId("after"));
  EXPECT_FALSE(split_inline_part2->FirstChild()->NextSibling());
  EXPECT_FALSE(split_inline_part2->Continuation());
}

TEST_F(LayoutInlineTest, RegionHitTest) {
  SetBodyInnerHTML(R"HTML(
    <div><span id='lotsOfBoxes'>
    This is a test line<br>This is a test line<br>This is a test line<br>
    This is a test line<br>This is a test line<br>This is a test line<br>
    This is a test line<br>This is a test line<br>This is a test line<br>
    This is a test line<br>This is a test line<br>This is a test line<br>
    This is a test line<br>This is a test line<br>This is a test line<br>
    This is a test line<br>This is a test line<br>This is a test line<br>
    </span></div>
  )HTML");

  UpdateAllLifecyclePhasesForTest();

  LayoutInline* lots_of_boxes =
      ToLayoutInline(GetLayoutObjectByElementId("lotsOfBoxes"));
  ASSERT_TRUE(lots_of_boxes);

  HitTestRequest hit_request(HitTestRequest::kTouchEvent |
                             HitTestRequest::kListBased);

  LayoutRect hit_rect(1, 3, 2, 4);
  HitTestLocation location(hit_rect);
  HitTestResult hit_result(hit_request, location);
  LayoutPoint hit_offset;

  // The return value of HitTestCulledInline() indicates whether the hit test
  // rect is completely contained by the part of |lots_of_boxes| being hit-
  // tested. Legacy hit tests the entire LayoutObject all at once while NG hit
  // tests line by line. Therefore, legacy returns true while NG is false.
  //
  // Note: The legacy behavior seems wrong. In a full list-based hit testing,
  // after testing the node in the last intersecting line, the |true| return
  // value of HitTestCulledInline() terminates the hit test process, and nodes
  // in the previous lines are not tested.
  //
  // TODO(xiaochengh): Expose this issue in a real Chrome use case.

  if (!lots_of_boxes->IsInLayoutNGInlineFormattingContext()) {
    bool hit_outcome =
        lots_of_boxes->HitTestCulledInline(hit_result, location, hit_offset);
    // Assert checks that we both hit something and that the area covered
    // by "something" totally contains the hit region.
    EXPECT_TRUE(hit_outcome);
    return;
  }

  const auto* div = To<LayoutBlockFlow>(lots_of_boxes->Parent());
  for (const NGPaintFragment* line : div->PaintFragment()->Children()) {
    DCHECK(line->PhysicalFragment().IsLineBox());
    bool hit_outcome = lots_of_boxes->HitTestCulledInline(hit_result, location,
                                                          hit_offset, line);
    EXPECT_FALSE(hit_outcome);
  }
  // Make sure that the inline is hit
  const Node* span = lots_of_boxes->GetNode();
  EXPECT_EQ(span, hit_result.InnerNode());
}

// crbug.com/844746
TEST_P(ParameterizedLayoutInlineTest, RelativePositionedHitTest) {
  LoadAhem();
  SetBodyInnerHTML(
      "<div style='font: 10px/10px Ahem'>"
      "  <span style='position: relative'>XXX</span>"
      "</div>");

  HitTestRequest hit_request(HitTestRequest::kReadOnly |
                             HitTestRequest::kActive);
  const LayoutPoint container_offset(8, 8);
  const LayoutPoint hit_location(18, 15);
  HitTestLocation location(hit_location);

  Element* div = GetDocument().QuerySelector("div");
  Element* span = GetDocument().QuerySelector("span");
  Node* text = span->firstChild();

  // Shouldn't hit anything in SPAN as it's in another paint layer
  {
    LayoutObject* layout_div = div->GetLayoutObject();
    HitTestResult hit_result(hit_request, location);
    bool hit_outcome =
        layout_div->HitTestAllPhases(hit_result, location, container_offset);
    EXPECT_TRUE(hit_outcome);
    EXPECT_EQ(div, hit_result.InnerNode());
  }

  // SPAN and its descendants can be hit only with a hit test that starts from
  // the SPAN itself.
  {
    LayoutObject* layout_span = span->GetLayoutObject();
    HitTestResult hit_result(hit_request, location);
    bool hit_outcome =
        layout_span->HitTestAllPhases(hit_result, location, container_offset);
    EXPECT_TRUE(hit_outcome);
    EXPECT_EQ(text, hit_result.InnerNode());
  }

  // Hit test from LayoutView to verify that everything works together.
  {
    HitTestResult hit_result(hit_request, location);
    bool hit_outcome = GetLayoutView().HitTest(location, hit_result);
    EXPECT_TRUE(hit_outcome);
    EXPECT_EQ(text, hit_result.InnerNode());
  }
}

TEST_P(ParameterizedLayoutInlineTest, MultilineRelativePositionedHitTest) {
  LoadAhem();
  SetBodyInnerHTML(
      "<div style='font: 10px/10px Ahem; width: 30px'>"
      "  <span id=span style='position: relative'>"
      "    XXX"
      "    <span id=line2 style='background-color: red'>YYY</span>"
      "    <img style='width: 10px; height: 10px; vertical-align: bottom'>"
      "  </span>"
      "</div>");

  LayoutObject* layout_span = GetLayoutObjectByElementId("span");
  HitTestRequest hit_request(HitTestRequest::kReadOnly |
                             HitTestRequest::kActive |
                             HitTestRequest::kIgnorePointerEventsNone);
  const LayoutPoint container_offset(8, 8);

  // Hit test first line
  {
    LayoutPoint hit_location(13, 13);
    HitTestLocation location(hit_location);
    Node* target = GetElementById("span")->firstChild();

    HitTestResult hit_result(hit_request, location);
    bool hit_outcome =
        layout_span->HitTestAllPhases(hit_result, location, container_offset);
    EXPECT_TRUE(hit_outcome);
    EXPECT_EQ(target, hit_result.InnerNode());

    // Initiate a hit test from LayoutView to verify the "natural" process.
    HitTestResult layout_view_hit_result(hit_request, location);
    bool layout_view_hit_outcome =
        GetLayoutView().HitTest(location, layout_view_hit_result);
    EXPECT_TRUE(layout_view_hit_outcome);
    EXPECT_EQ(target, layout_view_hit_result.InnerNode());
  }

  // Hit test second line
  {
    LayoutPoint hit_location(13, 23);
    HitTestLocation location(hit_location);
    Node* target = GetElementById("line2")->firstChild();

    HitTestResult hit_result(hit_request, location);
    bool hit_outcome =
        layout_span->HitTestAllPhases(hit_result, location, container_offset);
    EXPECT_TRUE(hit_outcome);
    EXPECT_EQ(target, hit_result.InnerNode());

    // Initiate a hit test from LayoutView to verify the "natural" process.
    HitTestResult layout_view_hit_result(hit_request, location);
    bool layout_view_hit_outcome =
        GetLayoutView().HitTest(location, layout_view_hit_result);
    EXPECT_TRUE(layout_view_hit_outcome);
    EXPECT_EQ(target, layout_view_hit_result.InnerNode());
  }

  // Hit test image in third line
  {
    LayoutPoint hit_location(13, 33);
    HitTestLocation location(hit_location);
    Node* target = GetDocument().QuerySelector("img");

    HitTestResult hit_result(hit_request, location);
    bool hit_outcome =
        layout_span->HitTestAllPhases(hit_result, location, container_offset);
    EXPECT_TRUE(hit_outcome);
    EXPECT_EQ(target, hit_result.InnerNode());

    // Initiate a hit test from LayoutView to verify the "natural" process.
    HitTestResult layout_view_hit_result(hit_request, location);
    bool layout_view_hit_outcome =
        GetLayoutView().HitTest(location, layout_view_hit_result);
    EXPECT_TRUE(layout_view_hit_outcome);
    EXPECT_EQ(target, layout_view_hit_result.InnerNode());
  }
}

TEST_P(ParameterizedLayoutInlineTest, VisualRectInDocument) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin:0px;
        font: 20px/20px Ahem;
      }
    </style>
    <div>
      <span>xx<br>
        <span id="target" tabindex="-1">yy
          <div style="width:111px;height:222px;background:yellow"></div>
          yy
        </span>
      </span>
    </div>
  )HTML");

  LayoutInline* target = ToLayoutInline(GetLayoutObjectByElementId("target"));
  EXPECT_EQ(LayoutRect(0, 20, 111, 222 + 20 * 2),
            target->VisualRectInDocument());
  EXPECT_EQ(LayoutRect(0, 20, 111, 222 + 20 * 2),
            target->VisualRectInDocument(kUseGeometryMapper));
}

// When adding focus ring rects, we should avoid adding duplicated rect for
// continuations.
TEST_P(ParameterizedLayoutInlineTest, FocusRingRecursiveContinuations) {
  // TODO(crbug.com/835484): The test is broken for LayoutNG.
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin: 0;
        font: 20px/20px Ahem;
      }
    </style>
    <span id="target">SPAN0
      <div>DIV1
        <span>SPAN1
          <div>DIV2</div>
        </span>
      </div>
    </span>
  )HTML");

  Vector<LayoutRect> rects;
  GetLayoutObjectByElementId("target")->AddOutlineRects(
      rects, LayoutPoint(), NGOutlineType::kIncludeBlockVisualOverflow);

  EXPECT_THAT(rects,
              UnorderedElementsAre(LayoutRect(0, 0, 100, 20),    // 'SPAN0'
                                   LayoutRect(0, 20, 800, 40),   // div DIV1
                                   LayoutRect(0, 20, 200, 20),   // 'DIV1 SPAN1'
                                   LayoutRect(0, 40, 800, 20),   // div DIV2
                                   LayoutRect(0, 40, 80, 20)));  // 'DIV2'
}

// When adding focus ring rects, we should avoid adding line box rects of
// recursive inlines repeatedly.
TEST_P(ParameterizedLayoutInlineTest, FocusRingRecursiveInlines) {
  // TODO(crbug.com/835484): The test is broken for LayoutNG.
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin: 0;
        font: 20px/20px Ahem;
      }
    </style>
    <div style="width: 200px">
      <span id="target">
        <b><b><b><i><i><i>INLINE</i></i> <i><i>TEXT</i></i>
        <div style="position: relative; top: -5px">
          <b><b>BLOCK</b> <i>CONTENTS</i></b>
        </div>
        </i></b></b></b>
      </span>
    </div>
  )HTML");

  Vector<LayoutRect> rects;
  GetLayoutObjectByElementId("target")->AddOutlineRects(
      rects, LayoutPoint(), NGOutlineType::kIncludeBlockVisualOverflow);

  EXPECT_THAT(rects,
              UnorderedElementsAre(LayoutRect(0, 0, 120, 20),   // 'INLINE'
                                   LayoutRect(0, 20, 80, 20),   // 'TEXT'
                                   LayoutRect(0, 35, 200, 40),  // the inner div
                                   LayoutRect(0, 35, 100, 20),  // 'BLOCK'
                                   LayoutRect(0, 55, 160, 20)));  // 'CONTENTS'
}

}  // namespace blink
