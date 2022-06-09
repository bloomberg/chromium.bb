// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/caret_display_item_client.h"

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/paint/paint_and_raster_invalidation_test.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"

namespace blink {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

class CaretDisplayItemClientTest : public PaintAndRasterInvalidationTest {
 protected:
  void SetUp() override {
    PaintAndRasterInvalidationTest::SetUp();
    Selection().SetCaretBlinkingSuspended(true);
  }

  FrameSelection& Selection() const {
    return GetDocument().View()->GetFrame().Selection();
  }

  const CaretDisplayItemClient& GetCaretDisplayItemClient() const {
    return Selection().CaretDisplayItemClientForTesting();
  }

  const PhysicalRect& CaretLocalRect() const {
    return GetCaretDisplayItemClient().local_rect_;
  }

  const LayoutBlock* CaretLayoutBlock() const {
    return GetCaretDisplayItemClient().layout_block_;
  }

  const LayoutBlock* PreviousCaretLayoutBlock() const {
    return GetCaretDisplayItemClient().previous_layout_block_;
  }

  bool ShouldPaintCursorCaret(const LayoutBlock& block) {
    return Selection().ShouldPaintCaret(block);
  }

  Text* AppendTextNode(const String& data) {
    Text* text = GetDocument().createTextNode(data);
    GetDocument().body()->AppendChild(text);
    return text;
  }

  Element* AppendBlock(const String& data) {
    Element* block = GetDocument().CreateRawElement(html_names::kDivTag);
    Text* text = GetDocument().createTextNode(data);
    block->AppendChild(text);
    GetDocument().body()->AppendChild(block);
    return block;
  }

  void UpdateAllLifecyclePhasesForCaretTest() {
    // Partial lifecycle updates should not affect caret paint invalidation.
    GetDocument().View()->UpdateLifecycleToLayoutClean(
        DocumentUpdateReason::kTest);
    UpdateAllLifecyclePhasesForTest();
    // Partial lifecycle updates should not affect caret paint invalidation.
    GetDocument().View()->UpdateLifecycleToLayoutClean(
        DocumentUpdateReason::kTest);
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(CaretDisplayItemClientTest);

TEST_P(CaretDisplayItemClientTest, CaretPaintInvalidation) {
  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  Text* text = AppendTextNode("Hello, World!");
  UpdateAllLifecyclePhasesForCaretTest();
  const auto* block = To<LayoutBlock>(GetDocument().body()->GetLayoutObject());

  // Focus the body. Should invalidate the new caret.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  GetDocument().body()->focus();

  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());

  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(ShouldPaintCursorCaret(*block));
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  EXPECT_EQ(PhysicalRect(0, 0, 1, 1), CaretLocalRect());

  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  GetCaretDisplayItemClient().Id(), "Caret",
                  gfx::Rect(8, 8, 1, 1), PaintInvalidationReason::kAppeared}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Move the caret to the end of the text. Should invalidate both the old and
  // new carets.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder().Collapse(Position(text, 5)).Build());

  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());

  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(ShouldPaintCursorCaret(*block));
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  int delta = CaretLocalRect().X().ToInt();
  EXPECT_GT(delta, 0);
  EXPECT_EQ(PhysicalRect(delta, 0, 1, 1), CaretLocalRect());

  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{GetCaretDisplayItemClient().Id(),
                                         "Caret", gfx::Rect(8, 8, 1, 1),
                                         PaintInvalidationReason::kCaret},
                  RasterInvalidationInfo{GetCaretDisplayItemClient().Id(),
                                         "Caret", gfx::Rect(8 + delta, 8, 1, 1),
                                         PaintInvalidationReason::kCaret}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Remove selection. Should invalidate the old caret.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  Selection().SetSelectionAndEndTyping(SelectionInDOMTree());

  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());

  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_FALSE(ShouldPaintCursorCaret(*block));
  // The caret display item client painted nothing, so is not validated.
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());
  EXPECT_EQ(PhysicalRect(), CaretLocalRect());

  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(RasterInvalidationInfo{
                  GetCaretDisplayItemClient().Id(), "Caret",
                  gfx::Rect(8 + delta, 8, 1, 1),
                  PaintInvalidationReason::kDisappeared}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(CaretDisplayItemClientTest, CaretMovesBetweenBlocks) {
  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  auto* block_element1 = AppendBlock("Block1");
  auto* block_element2 = AppendBlock("Block2");
  UpdateAllLifecyclePhasesForTest();
  auto* block1 = To<LayoutBlockFlow>(block_element1->GetLayoutObject());
  auto* block2 = To<LayoutBlockFlow>(block_element2->GetLayoutObject());

  // Focus the body.
  GetDocument().body()->focus();

  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());

  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());

  EXPECT_EQ(PhysicalRect(0, 0, 1, 1), CaretLocalRect());
  EXPECT_TRUE(ShouldPaintCursorCaret(*block1));
  EXPECT_FALSE(ShouldPaintCursorCaret(*block2));

  // Move the caret into block2. Should invalidate both the old and new carets.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element2, 0))
          .Build());

  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());

  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());

  EXPECT_EQ(PhysicalRect(0, 0, 1, 1), CaretLocalRect());
  EXPECT_FALSE(ShouldPaintCursorCaret(*block1));
  EXPECT_TRUE(ShouldPaintCursorCaret(*block2));

  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{GetCaretDisplayItemClient().Id(),
                                         "Caret", gfx::Rect(8, 8, 1, 1),
                                         PaintInvalidationReason::kCaret},
                  RasterInvalidationInfo{GetCaretDisplayItemClient().Id(),
                                         "Caret", gfx::Rect(8, 9, 1, 1),
                                         PaintInvalidationReason::kCaret}));
  GetDocument().View()->SetTracksRasterInvalidations(false);

  // Move the caret back into block1.
  GetDocument().View()->SetTracksRasterInvalidations(true);
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element1, 0))
          .Build());

  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());

  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());

  EXPECT_EQ(PhysicalRect(0, 0, 1, 1), CaretLocalRect());
  EXPECT_TRUE(ShouldPaintCursorCaret(*block1));
  EXPECT_FALSE(ShouldPaintCursorCaret(*block2));

  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{GetCaretDisplayItemClient().Id(),
                                         "Caret", gfx::Rect(8, 8, 1, 1),
                                         PaintInvalidationReason::kCaret},
                  RasterInvalidationInfo{GetCaretDisplayItemClient().Id(),
                                         "Caret", gfx::Rect(8, 9, 1, 1),
                                         PaintInvalidationReason::kCaret}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(CaretDisplayItemClientTest, UpdatePreviousLayoutBlock) {
  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  auto* block_element1 = AppendBlock("Block1");
  auto* block_element2 = AppendBlock("Block2");
  UpdateAllLifecyclePhasesForCaretTest();
  auto* block1 = To<LayoutBlock>(block_element1->GetLayoutObject());
  auto* block2 = To<LayoutBlock>(block_element2->GetLayoutObject());

  // Set caret into block2.
  GetDocument().body()->focus();
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element2, 0))
          .Build());
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(ShouldPaintCursorCaret(*block2));
  EXPECT_EQ(block2, CaretLayoutBlock());
  EXPECT_FALSE(ShouldPaintCursorCaret(*block1));
  EXPECT_FALSE(PreviousCaretLayoutBlock());

  // Move caret into block1. Should set previousCaretLayoutBlock to block2.
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element1, 0))
          .Build());
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(ShouldPaintCursorCaret(*block1));
  EXPECT_EQ(block1, CaretLayoutBlock());
  EXPECT_FALSE(ShouldPaintCursorCaret(*block2));
  EXPECT_EQ(block2, PreviousCaretLayoutBlock());

  // Move caret into block2. Partial update should not change
  // previousCaretLayoutBlock.
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element2, 0))
          .Build());
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_TRUE(ShouldPaintCursorCaret(*block2));
  EXPECT_EQ(block2, CaretLayoutBlock());
  EXPECT_FALSE(ShouldPaintCursorCaret(*block1));
  EXPECT_EQ(block2, PreviousCaretLayoutBlock());

  // Remove block2. Should clear caretLayoutBlock and previousCaretLayoutBlock.
  block_element2->parentNode()->RemoveChild(block_element2);
  EXPECT_FALSE(CaretLayoutBlock());
  EXPECT_FALSE(PreviousCaretLayoutBlock());

  // Set caret into block1.
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element1, 0))
          .Build());
  UpdateAllLifecyclePhasesForCaretTest();
  // Remove selection.
  Selection().SetSelectionAndEndTyping(SelectionInDOMTree());
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  EXPECT_EQ(block1, PreviousCaretLayoutBlock());
}

TEST_P(CaretDisplayItemClientTest, CaretHideMoveAndShow) {
  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  Text* text = AppendTextNode("Hello, World!");
  GetDocument().body()->focus();
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_EQ(PhysicalRect(0, 0, 1, 1), CaretLocalRect());

  GetDocument().View()->SetTracksRasterInvalidations(true);
  // Simulate that the blinking cursor becomes invisible.
  Selection().SetCaretEnabled(false);
  // Move the caret to the end of the text.
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder().Collapse(Position(text, 5)).Build());
  // Simulate that the cursor blinking is restarted.
  Selection().SetCaretEnabled(true);

  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());

  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(GetCaretDisplayItemClient().IsValid());
  int delta = CaretLocalRect().X().ToInt();
  EXPECT_GT(delta, 0);
  EXPECT_EQ(PhysicalRect(delta, 0, 1, 1), CaretLocalRect());

  EXPECT_THAT(GetRasterInvalidationTracking()->Invalidations(),
              UnorderedElementsAre(
                  RasterInvalidationInfo{GetCaretDisplayItemClient().Id(),
                                         "Caret", gfx::Rect(8, 8, 1, 1),
                                         PaintInvalidationReason::kCaret},
                  RasterInvalidationInfo{GetCaretDisplayItemClient().Id(),
                                         "Caret", gfx::Rect(8 + delta, 8, 1, 1),
                                         PaintInvalidationReason::kCaret}));
  GetDocument().View()->SetTracksRasterInvalidations(false);
}

TEST_P(CaretDisplayItemClientTest, CompositingChange) {
  SetBodyInnerHTML(
      "<style>"
      "  body { margin: 0 }"
      "  #container { position: absolute; top: 55px; left: 66px; }"
      "</style>"
      "<div id='container'>"
      "  <div id='editor' contenteditable style='padding: 50px'>ABCDE</div>"
      "</div>");

  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  auto* container = GetDocument().getElementById("container");
  auto* editor = GetDocument().getElementById("editor");
  auto* editor_block = To<LayoutBlock>(editor->GetLayoutObject());
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder().Collapse(Position(editor, 0)).Build());

  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_TRUE(ShouldPaintCursorCaret(*editor_block));
  EXPECT_EQ(editor_block, CaretLayoutBlock());
  EXPECT_EQ(PhysicalRect(50, 50, 1, 1), CaretLocalRect());

  // Composite container.
  container->setAttribute(html_names::kStyleAttr, "will-change: transform");
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_EQ(PhysicalRect(50, 50, 1, 1), CaretLocalRect());

  // Uncomposite container.
  container->setAttribute(html_names::kStyleAttr, "");
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(GetCaretDisplayItemClient().IsValid());
  UpdateAllLifecyclePhasesForCaretTest();
  EXPECT_EQ(PhysicalRect(50, 50, 1, 1), CaretLocalRect());
}

class ParameterizedComputeCaretRectTest
    : public EditingTestBase,
      private ScopedLayoutNGForTest,
      public testing::WithParamInterface<bool> {
 public:
  ParameterizedComputeCaretRectTest() : ScopedLayoutNGForTest(GetParam()) {}

 protected:
  PhysicalRect ComputeCaretRect(const PositionWithAffinity& position) const {
    return CaretDisplayItemClient::ComputeCaretRectAndPainterBlock(position)
        .caret_rect;
  }
  HitTestResult HitTestResultAtLocation(const HitTestLocation& location) {
    return GetFrame().GetEventHandler().HitTestResultAtLocation(location);
  }
  HitTestResult HitTestResultAtLocation(int x, int y) {
    HitTestLocation location(gfx::Point(x, y));
    return HitTestResultAtLocation(location);
  }
};

TEST_P(CaretDisplayItemClientTest, FullDocumentPaintingWithCaret) {
  SetBodyInnerHTML(
      "<div id='div' contentEditable='true' style='outline:none'>XYZ</div>");
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  auto& div = *To<Element>(GetDocument().body()->firstChild());
  auto& layout_text = *To<Text>(div.firstChild())->GetLayoutObject();
  const DisplayItemClient* text_inline_box = layout_text.FirstTextBox();
  if (layout_text.IsInLayoutNGInlineFormattingContext()) {
    NGInlineCursor cursor;
    cursor.MoveTo(layout_text);
    text_inline_box = cursor.Current().GetDisplayItemClient();
  }
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(text_inline_box->Id(), kForegroundType)));

  div.focus();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(text_inline_box->Id(), kForegroundType),
                          // New!
                          IsSameId(GetCaretDisplayItemClient().Id(),
                                   DisplayItem::kCaret)));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ParameterizedComputeCaretRectTest,
                         testing::Bool());

TEST_P(ParameterizedComputeCaretRectTest, CaretRectAfterEllipsisNoCrash) {
  SetBodyInnerHTML(
      "<style>pre{width:30px; overflow:hidden; text-overflow:ellipsis}</style>"
      "<pre id=target>long long long long long long text</pre>");
  const Node* text = GetElementById("target")->firstChild();
  const Position position = Position::LastPositionInNode(*text);
  // Shouldn't crash inside. The actual result doesn't matter and may change.
  ComputeCaretRect(PositionWithAffinity(position));
}

TEST_P(ParameterizedComputeCaretRectTest, CaretRectAvoidNonEditable) {
  LoadAhem();
  InsertStyleElement(
      "body { margin: 0; padding: 0; font: 10px/10px Ahem; }"
      "div { width: 70px; padding: 0px 10px; }"
      "span { padding: 0px 15px }");
  SetBodyContent(
      "<div contenteditable><span contenteditable=\"false\">foo</span></div>");

  const PositionWithAffinity& caret_position1 =
      HitTestResultAtLocation(20, 5).GetPosition();
  const PhysicalRect& rect1 = ComputeCaretRect(caret_position1);
  EXPECT_EQ(PhysicalRect(10, 0, 1, 10), rect1);

  // TODO(jfernandez): It should be 89, but LayoutBox::LocalCaretRect is buggy
  // and it adds the padding-left twice.
  // TODO(jfernandez): As a matter of fact, 69 would be better result IMHO,
  // positioning the caret at the end of the non-editable area.
  // TODO(jfernandez): We might avoid using LayoutBox::LocalCaretRect when using
  // LayoutNG
  const PositionWithAffinity& caret_position2 =
      HitTestResultAtLocation(60, 5).GetPosition();
  const PhysicalRect& rect2 = ComputeCaretRect(caret_position2);
  EXPECT_EQ(PhysicalRect(99, 0, 1, 10), rect2);
}

}  // namespace blink
