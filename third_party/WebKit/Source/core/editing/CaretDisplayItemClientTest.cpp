// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/CaretDisplayItemClient.h"

#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionTemplate.h"
#include "core/frame/LocalFrameView.h"
#include "core/html_names.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/LayoutView.h"
#include "core/page/FocusController.h"
#include "core/paint/PaintLayer.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/RasterInvalidationTracking.h"

namespace blink {

class CaretDisplayItemClientTest : public RenderingTest {
 protected:
  void SetUp() override {
    RenderingTest::SetUp();
    EnableCompositing();
    Selection().SetCaretBlinkingSuspended(true);
  }

  const RasterInvalidationTracking* GetRasterInvalidationTracking() const {
    // TODO(wangxianzhu): Test SPv2.
    return GetLayoutView()
        .Layer()
        ->GraphicsLayerBacking()
        ->GetRasterInvalidationTracking();
  }

  FrameSelection& Selection() const {
    return GetDocument().View()->GetFrame().Selection();
  }

  const DisplayItemClient& GetCaretDisplayItemClient() const {
    return Selection().CaretDisplayItemClientForTesting();
  }

  const LayoutBlock* CaretLayoutBlock() const {
    return static_cast<const CaretDisplayItemClient&>(
               GetCaretDisplayItemClient())
        .layout_block_;
  }

  const LayoutBlock* PreviousCaretLayoutBlock() const {
    return static_cast<const CaretDisplayItemClient&>(
               GetCaretDisplayItemClient())
        .previous_layout_block_;
  }

  Text* AppendTextNode(const String& data) {
    Text* text = GetDocument().createTextNode(data);
    GetDocument().body()->AppendChild(text);
    return text;
  }

  Element* AppendBlock(const String& data) {
    Element* block = GetDocument().createElement("div");
    Text* text = GetDocument().createTextNode(data);
    block->AppendChild(text);
    GetDocument().body()->AppendChild(block);
    return block;
  }

  void UpdateAllLifecyclePhases() {
    // Partial lifecycle updates should not affect caret paint invalidation.
    GetDocument().View()->UpdateLifecycleToLayoutClean();
    GetDocument().View()->UpdateAllLifecyclePhases();
    // Partial lifecycle updates should not affect caret paint invalidation.
    GetDocument().View()->UpdateLifecycleToLayoutClean();
  }
};

TEST_F(CaretDisplayItemClientTest, CaretPaintInvalidation) {
  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  Text* text = AppendTextNode("Hello, World!");
  UpdateAllLifecyclePhases();
  const auto* block = ToLayoutBlock(GetDocument().body()->GetLayoutObject());

  // Focus the body. Should invalidate the new caret.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  GetDocument().body()->focus();
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(block->ShouldPaintCursorCaret());

  LayoutRect caret_visual_rect = GetCaretDisplayItemClient().VisualRect();
  EXPECT_EQ(1, caret_visual_rect.Width());
  EXPECT_EQ(block->Location(), caret_visual_rect.Location());

  const auto* raster_invalidations =
      &GetRasterInvalidationTracking()->Invalidations();
  ASSERT_EQ(1u, raster_invalidations->size());
  EXPECT_EQ(EnclosingIntRect(caret_visual_rect),
            (*raster_invalidations)[0].rect);
  EXPECT_EQ(block, (*raster_invalidations)[0].client);
  EXPECT_EQ(PaintInvalidationReason::kCaret, (*raster_invalidations)[0].reason);

  std::unique_ptr<JSONArray> object_invalidations =
      GetDocument().View()->TrackedObjectPaintInvalidationsAsJSON();
  ASSERT_EQ(1u, object_invalidations->size());
  String s;
  JSONObject::Cast(object_invalidations->at(0))->Get("object")->AsString(&s);
  EXPECT_EQ("Caret", s);
  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Move the caret to the end of the text. Should invalidate both the old and
  // new carets.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder().Collapse(Position(text, 5)).Build());
  UpdateAllLifecyclePhases();
  EXPECT_TRUE(block->ShouldPaintCursorCaret());

  LayoutRect new_caret_visual_rect = GetCaretDisplayItemClient().VisualRect();
  EXPECT_EQ(caret_visual_rect.Size(), new_caret_visual_rect.Size());
  EXPECT_EQ(caret_visual_rect.Y(), new_caret_visual_rect.Y());
  EXPECT_LT(caret_visual_rect.X(), new_caret_visual_rect.X());

  raster_invalidations = &GetRasterInvalidationTracking()->Invalidations();
  ASSERT_EQ(2u, raster_invalidations->size());
  EXPECT_EQ(EnclosingIntRect(caret_visual_rect),
            (*raster_invalidations)[0].rect);
  EXPECT_EQ(block, (*raster_invalidations)[0].client);
  EXPECT_EQ(PaintInvalidationReason::kCaret, (*raster_invalidations)[0].reason);
  EXPECT_EQ(EnclosingIntRect(new_caret_visual_rect),
            (*raster_invalidations)[1].rect);
  EXPECT_EQ(block, (*raster_invalidations)[1].client);
  EXPECT_EQ(PaintInvalidationReason::kCaret, (*raster_invalidations)[1].reason);

  object_invalidations =
      GetDocument().View()->TrackedObjectPaintInvalidationsAsJSON();
  ASSERT_EQ(1u, object_invalidations->size());
  JSONObject::Cast(object_invalidations->at(0))->Get("object")->AsString(&s);
  EXPECT_EQ("Caret", s);
  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Remove selection. Should invalidate the old caret.
  LayoutRect old_caret_visual_rect = new_caret_visual_rect;
  GetDocument().View()->SetTracksPaintInvalidations(true);
  Selection().SetSelectionAndEndTyping(SelectionInDOMTree());
  UpdateAllLifecyclePhases();
  EXPECT_FALSE(block->ShouldPaintCursorCaret());
  EXPECT_EQ(LayoutRect(), GetCaretDisplayItemClient().VisualRect());

  raster_invalidations = &GetRasterInvalidationTracking()->Invalidations();
  ASSERT_EQ(1u, raster_invalidations->size());
  EXPECT_EQ(EnclosingIntRect(old_caret_visual_rect),
            (*raster_invalidations)[0].rect);
  EXPECT_EQ(block, (*raster_invalidations)[0].client);

  object_invalidations =
      GetDocument().View()->TrackedObjectPaintInvalidationsAsJSON();
  ASSERT_EQ(1u, object_invalidations->size());
  JSONObject::Cast(object_invalidations->at(0))->Get("object")->AsString(&s);
  EXPECT_EQ("Caret", s);
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_F(CaretDisplayItemClientTest, CaretMovesBetweenBlocks) {
  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  auto* block_element1 = AppendBlock("Block1");
  auto* block_element2 = AppendBlock("Block2");
  UpdateAllLifecyclePhases();
  auto* block1 = ToLayoutBlock(block_element1->GetLayoutObject());
  auto* block2 = ToLayoutBlock(block_element2->GetLayoutObject());

  // Focus the body.
  GetDocument().body()->focus();
  UpdateAllLifecyclePhases();
  LayoutRect caret_visual_rect1 = GetCaretDisplayItemClient().VisualRect();
  EXPECT_EQ(1, caret_visual_rect1.Width());
  EXPECT_EQ(block1->FirstFragment().VisualRect().Location(),
            caret_visual_rect1.Location());
  EXPECT_TRUE(block1->ShouldPaintCursorCaret());
  EXPECT_FALSE(block2->ShouldPaintCursorCaret());

  // Move the caret into block2. Should invalidate both the old and new carets.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element2, 0))
          .Build());
  UpdateAllLifecyclePhases();

  LayoutRect caret_visual_rect2 = GetCaretDisplayItemClient().VisualRect();
  EXPECT_EQ(1, caret_visual_rect2.Width());
  EXPECT_EQ(block2->FirstFragment().VisualRect().Location(),
            caret_visual_rect2.Location());
  EXPECT_FALSE(block1->ShouldPaintCursorCaret());
  EXPECT_TRUE(block2->ShouldPaintCursorCaret());

  const auto* raster_invalidations =
      &GetRasterInvalidationTracking()->Invalidations();
  ASSERT_EQ(2u, raster_invalidations->size());
  EXPECT_EQ(EnclosingIntRect(caret_visual_rect1),
            (*raster_invalidations)[0].rect);
  EXPECT_EQ(block1, (*raster_invalidations)[0].client);
  EXPECT_EQ(PaintInvalidationReason::kCaret, (*raster_invalidations)[0].reason);
  EXPECT_EQ(EnclosingIntRect(caret_visual_rect2),
            (*raster_invalidations)[1].rect);
  EXPECT_EQ(block2, (*raster_invalidations)[1].client);
  EXPECT_EQ(PaintInvalidationReason::kCaret, (*raster_invalidations)[1].reason);

  std::unique_ptr<JSONArray> object_invalidations =
      GetDocument().View()->TrackedObjectPaintInvalidationsAsJSON();
  ASSERT_EQ(2u, object_invalidations->size());
  GetDocument().View()->SetTracksPaintInvalidations(false);

  // Move the caret back into block1.
  GetDocument().View()->SetTracksPaintInvalidations(true);
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element1, 0))
          .Build());
  UpdateAllLifecyclePhases();

  EXPECT_EQ(caret_visual_rect1, GetCaretDisplayItemClient().VisualRect());
  EXPECT_TRUE(block1->ShouldPaintCursorCaret());
  EXPECT_FALSE(block2->ShouldPaintCursorCaret());

  raster_invalidations = &GetRasterInvalidationTracking()->Invalidations();
  ASSERT_EQ(2u, raster_invalidations->size());
  EXPECT_EQ(EnclosingIntRect(caret_visual_rect1),
            (*raster_invalidations)[0].rect);
  EXPECT_EQ(block1, (*raster_invalidations)[0].client);
  EXPECT_EQ(PaintInvalidationReason::kCaret, (*raster_invalidations)[0].reason);
  EXPECT_EQ(EnclosingIntRect(caret_visual_rect2),
            (*raster_invalidations)[1].rect);
  EXPECT_EQ(block2, (*raster_invalidations)[1].client);
  EXPECT_EQ(PaintInvalidationReason::kCaret, (*raster_invalidations)[1].reason);

  object_invalidations =
      GetDocument().View()->TrackedObjectPaintInvalidationsAsJSON();
  ASSERT_EQ(2u, object_invalidations->size());
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_F(CaretDisplayItemClientTest, UpdatePreviousLayoutBlock) {
  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  auto* block_element1 = AppendBlock("Block1");
  auto* block_element2 = AppendBlock("Block2");
  UpdateAllLifecyclePhases();
  auto* block1 = ToLayoutBlock(block_element1->GetLayoutObject());
  auto* block2 = ToLayoutBlock(block_element2->GetLayoutObject());

  // Set caret into block2.
  GetDocument().body()->focus();
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element2, 0))
          .Build());
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  EXPECT_TRUE(block2->ShouldPaintCursorCaret());
  EXPECT_EQ(block2, CaretLayoutBlock());
  EXPECT_FALSE(block1->ShouldPaintCursorCaret());
  EXPECT_FALSE(PreviousCaretLayoutBlock());

  // Move caret into block1. Should set previousCaretLayoutBlock to block2.
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element1, 0))
          .Build());
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  EXPECT_TRUE(block1->ShouldPaintCursorCaret());
  EXPECT_EQ(block1, CaretLayoutBlock());
  EXPECT_FALSE(block2->ShouldPaintCursorCaret());
  EXPECT_EQ(block2, PreviousCaretLayoutBlock());

  // Move caret into block2. Partial update should not change
  // previousCaretLayoutBlock.
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(block_element2, 0))
          .Build());
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  EXPECT_TRUE(block2->ShouldPaintCursorCaret());
  EXPECT_EQ(block2, CaretLayoutBlock());
  EXPECT_FALSE(block1->ShouldPaintCursorCaret());
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
  UpdateAllLifecyclePhases();
  // Remove selection.
  Selection().SetSelectionAndEndTyping(SelectionInDOMTree());
  GetDocument().View()->UpdateLifecycleToLayoutClean();
  EXPECT_EQ(block1, PreviousCaretLayoutBlock());
}

TEST_F(CaretDisplayItemClientTest, CaretHideMoveAndShow) {
  GetDocument().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  Text* text = AppendTextNode("Hello, World!");
  GetDocument().body()->focus();
  UpdateAllLifecyclePhases();
  const auto* block = ToLayoutBlock(GetDocument().body()->GetLayoutObject());

  LayoutRect caret_visual_rect = GetCaretDisplayItemClient().VisualRect();
  EXPECT_EQ(1, caret_visual_rect.Width());
  EXPECT_EQ(block->Location(), caret_visual_rect.Location());

  GetDocument().View()->SetTracksPaintInvalidations(true);
  // Simulate that the blinking cursor becomes invisible.
  Selection().SetCaretVisible(false);
  // Move the caret to the end of the text.
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder().Collapse(Position(text, 5)).Build());
  // Simulate that the cursor blinking is restarted.
  Selection().SetCaretVisible(true);
  UpdateAllLifecyclePhases();

  LayoutRect new_caret_visual_rect = GetCaretDisplayItemClient().VisualRect();
  EXPECT_EQ(caret_visual_rect.Size(), new_caret_visual_rect.Size());
  EXPECT_EQ(caret_visual_rect.Y(), new_caret_visual_rect.Y());
  EXPECT_LT(caret_visual_rect.X(), new_caret_visual_rect.X());

  const auto& raster_invalidations =
      GetRasterInvalidationTracking()->Invalidations();
  ASSERT_EQ(2u, raster_invalidations.size());
  EXPECT_EQ(EnclosingIntRect(caret_visual_rect), raster_invalidations[0].rect);
  EXPECT_EQ(block, raster_invalidations[0].client);
  EXPECT_EQ(PaintInvalidationReason::kCaret, raster_invalidations[0].reason);
  EXPECT_EQ(EnclosingIntRect(new_caret_visual_rect),
            raster_invalidations[1].rect);
  EXPECT_EQ(block, raster_invalidations[1].client);
  EXPECT_EQ(PaintInvalidationReason::kCaret, raster_invalidations[1].reason);

  auto object_invalidations =
      GetDocument().View()->TrackedObjectPaintInvalidationsAsJSON();
  ASSERT_EQ(1u, object_invalidations->size());
  String s;
  JSONObject::Cast(object_invalidations->at(0))->Get("object")->AsString(&s);
  EXPECT_EQ("Caret", s);
  GetDocument().View()->SetTracksPaintInvalidations(false);
}

TEST_F(CaretDisplayItemClientTest, CompositingChange) {
  EnableCompositing();
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
  auto* editor_block = ToLayoutBlock(editor->GetLayoutObject());
  Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder().Collapse(Position(editor, 0)).Build());
  UpdateAllLifecyclePhases();

  EXPECT_TRUE(editor_block->ShouldPaintCursorCaret());
  EXPECT_EQ(editor_block, CaretLayoutBlock());
  EXPECT_EQ(LayoutRect(116, 105, 1, 1),
            GetCaretDisplayItemClient().VisualRect());

  // Composite container.
  container->setAttribute(HTMLNames::styleAttr, "will-change: transform");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(50, 50, 1, 1), GetCaretDisplayItemClient().VisualRect());

  // Uncomposite container.
  container->setAttribute(HTMLNames::styleAttr, "");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(116, 105, 1, 1),
            GetCaretDisplayItemClient().VisualRect());
}

}  // namespace blink
