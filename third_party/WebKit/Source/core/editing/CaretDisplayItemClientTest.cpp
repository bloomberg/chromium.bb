// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/CaretDisplayItemClient.h"

#include "core/HTMLNames.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/FrameView.h"
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
    enableCompositing();
    selection().setCaretBlinkingSuspended(true);
  }

  const RasterInvalidationTracking* getRasterInvalidationTracking() const {
    // TODO(wangxianzhu): Test SPv2.
    return layoutView()
        .layer()
        ->graphicsLayerBacking()
        ->getRasterInvalidationTracking();
  }

  FrameSelection& selection() const {
    return document().view()->frame().selection();
  }

  const DisplayItemClient& caretDisplayItemClient() const {
    return selection().caretDisplayItemClientForTesting();
  }

  const LayoutBlock* caretLayoutBlock() const {
    return static_cast<const CaretDisplayItemClient&>(caretDisplayItemClient())
        .m_layoutBlock;
  }

  const LayoutBlock* previousCaretLayoutBlock() const {
    return static_cast<const CaretDisplayItemClient&>(caretDisplayItemClient())
        .m_previousLayoutBlock;
  }

  Text* appendTextNode(const String& data) {
    Text* text = document().createTextNode(data);
    document().body()->appendChild(text);
    return text;
  }

  Element* appendBlock(const String& data) {
    Element* block = document().createElement("div");
    Text* text = document().createTextNode(data);
    block->appendChild(text);
    document().body()->appendChild(block);
    return block;
  }

  void updateAllLifecyclePhases() {
    // Partial lifecycle updates should not affect caret paint invalidation.
    document().view()->updateLifecycleToLayoutClean();
    document().view()->updateAllLifecyclePhases();
    // Partial lifecycle updates should not affect caret paint invalidation.
    document().view()->updateLifecycleToLayoutClean();
  }
};

TEST_F(CaretDisplayItemClientTest, CaretPaintInvalidation) {
  document().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  document().page()->focusController().setActive(true);
  document().page()->focusController().setFocused(true);

  Text* text = appendTextNode("Hello, World!");
  updateAllLifecyclePhases();
  const auto* block = toLayoutBlock(document().body()->layoutObject());

  // Focus the body. Should invalidate the new caret.
  document().view()->setTracksPaintInvalidations(true);
  document().body()->focus();
  updateAllLifecyclePhases();
  EXPECT_TRUE(block->shouldPaintCursorCaret());

  LayoutRect caretVisualRect = caretDisplayItemClient().visualRect();
  EXPECT_EQ(1, caretVisualRect.width());
  EXPECT_EQ(block->location(), caretVisualRect.location());

  const auto* rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(1u, rasterInvalidations->size());
  EXPECT_EQ(enclosingIntRect(caretVisualRect), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(block, (*rasterInvalidations)[0].client);
  EXPECT_EQ(PaintInvalidationCaret, (*rasterInvalidations)[0].reason);

  std::unique_ptr<JSONArray> objectInvalidations =
      document().view()->trackedObjectPaintInvalidationsAsJSON();
  ASSERT_EQ(1u, objectInvalidations->size());
  String s;
  JSONObject::cast(objectInvalidations->at(0))->get("object")->asString(&s);
  EXPECT_EQ("Caret", s);
  document().view()->setTracksPaintInvalidations(false);

  // Move the caret to the end of the text. Should invalidate both the old and
  // new carets.
  document().view()->setTracksPaintInvalidations(true);
  selection().setSelection(
      SelectionInDOMTree::Builder().collapse(Position(text, 5)).build());
  updateAllLifecyclePhases();
  EXPECT_TRUE(block->shouldPaintCursorCaret());

  LayoutRect newCaretVisualRect = caretDisplayItemClient().visualRect();
  EXPECT_EQ(caretVisualRect.size(), newCaretVisualRect.size());
  EXPECT_EQ(caretVisualRect.y(), newCaretVisualRect.y());
  EXPECT_LT(caretVisualRect.x(), newCaretVisualRect.x());

  rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(2u, rasterInvalidations->size());
  EXPECT_EQ(enclosingIntRect(caretVisualRect), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(block, (*rasterInvalidations)[0].client);
  EXPECT_EQ(PaintInvalidationCaret, (*rasterInvalidations)[0].reason);
  EXPECT_EQ(enclosingIntRect(newCaretVisualRect),
            (*rasterInvalidations)[1].rect);
  EXPECT_EQ(block, (*rasterInvalidations)[1].client);
  EXPECT_EQ(PaintInvalidationCaret, (*rasterInvalidations)[1].reason);

  objectInvalidations =
      document().view()->trackedObjectPaintInvalidationsAsJSON();
  ASSERT_EQ(1u, objectInvalidations->size());
  JSONObject::cast(objectInvalidations->at(0))->get("object")->asString(&s);
  EXPECT_EQ("Caret", s);
  document().view()->setTracksPaintInvalidations(false);

  // Remove selection. Should invalidate the old caret.
  LayoutRect oldCaretVisualRect = newCaretVisualRect;
  document().view()->setTracksPaintInvalidations(true);
  selection().setSelection(SelectionInDOMTree());
  updateAllLifecyclePhases();
  EXPECT_FALSE(block->shouldPaintCursorCaret());
  EXPECT_EQ(LayoutRect(), caretDisplayItemClient().visualRect());

  rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(1u, rasterInvalidations->size());
  EXPECT_EQ(enclosingIntRect(oldCaretVisualRect),
            (*rasterInvalidations)[0].rect);
  EXPECT_EQ(block, (*rasterInvalidations)[0].client);

  objectInvalidations =
      document().view()->trackedObjectPaintInvalidationsAsJSON();
  ASSERT_EQ(1u, objectInvalidations->size());
  JSONObject::cast(objectInvalidations->at(0))->get("object")->asString(&s);
  EXPECT_EQ("Caret", s);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_F(CaretDisplayItemClientTest, CaretMovesBetweenBlocks) {
  document().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  document().page()->focusController().setActive(true);
  document().page()->focusController().setFocused(true);
  auto* blockElement1 = appendBlock("Block1");
  auto* blockElement2 = appendBlock("Block2");
  updateAllLifecyclePhases();
  auto* block1 = toLayoutBlock(blockElement1->layoutObject());
  auto* block2 = toLayoutBlock(blockElement2->layoutObject());

  // Focus the body.
  document().body()->focus();
  updateAllLifecyclePhases();
  LayoutRect caretVisualRect1 = caretDisplayItemClient().visualRect();
  EXPECT_EQ(1, caretVisualRect1.width());
  EXPECT_EQ(block1->visualRect().location(), caretVisualRect1.location());
  EXPECT_TRUE(block1->shouldPaintCursorCaret());
  EXPECT_FALSE(block2->shouldPaintCursorCaret());

  // Move the caret into block2. Should invalidate both the old and new carets.
  document().view()->setTracksPaintInvalidations(true);
  selection().setSelection(SelectionInDOMTree::Builder()
                               .collapse(Position(blockElement2, 0))
                               .build());
  updateAllLifecyclePhases();

  LayoutRect caretVisualRect2 = caretDisplayItemClient().visualRect();
  EXPECT_EQ(1, caretVisualRect2.width());
  EXPECT_EQ(block2->visualRect().location(), caretVisualRect2.location());
  EXPECT_FALSE(block1->shouldPaintCursorCaret());
  EXPECT_TRUE(block2->shouldPaintCursorCaret());

  const auto* rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(2u, rasterInvalidations->size());
  EXPECT_EQ(enclosingIntRect(caretVisualRect1), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(block1, (*rasterInvalidations)[0].client);
  EXPECT_EQ(PaintInvalidationCaret, (*rasterInvalidations)[0].reason);
  EXPECT_EQ(enclosingIntRect(caretVisualRect2), (*rasterInvalidations)[1].rect);
  EXPECT_EQ(block2, (*rasterInvalidations)[1].client);
  EXPECT_EQ(PaintInvalidationCaret, (*rasterInvalidations)[1].reason);

  std::unique_ptr<JSONArray> objectInvalidations =
      document().view()->trackedObjectPaintInvalidationsAsJSON();
  ASSERT_EQ(2u, objectInvalidations->size());
  document().view()->setTracksPaintInvalidations(false);

  // Move the caret back into block1.
  document().view()->setTracksPaintInvalidations(true);
  selection().setSelection(SelectionInDOMTree::Builder()
                               .collapse(Position(blockElement1, 0))
                               .build());
  updateAllLifecyclePhases();

  EXPECT_EQ(caretVisualRect1, caretDisplayItemClient().visualRect());
  EXPECT_TRUE(block1->shouldPaintCursorCaret());
  EXPECT_FALSE(block2->shouldPaintCursorCaret());

  rasterInvalidations =
      &getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(2u, rasterInvalidations->size());
  EXPECT_EQ(enclosingIntRect(caretVisualRect1), (*rasterInvalidations)[0].rect);
  EXPECT_EQ(block1, (*rasterInvalidations)[0].client);
  EXPECT_EQ(PaintInvalidationCaret, (*rasterInvalidations)[0].reason);
  EXPECT_EQ(enclosingIntRect(caretVisualRect2), (*rasterInvalidations)[1].rect);
  EXPECT_EQ(block2, (*rasterInvalidations)[1].client);
  EXPECT_EQ(PaintInvalidationCaret, (*rasterInvalidations)[1].reason);

  objectInvalidations =
      document().view()->trackedObjectPaintInvalidationsAsJSON();
  ASSERT_EQ(2u, objectInvalidations->size());
  document().view()->setTracksPaintInvalidations(false);
}

TEST_F(CaretDisplayItemClientTest, UpdatePreviousLayoutBlock) {
  document().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  document().page()->focusController().setActive(true);
  document().page()->focusController().setFocused(true);
  auto* blockElement1 = appendBlock("Block1");
  auto* blockElement2 = appendBlock("Block2");
  updateAllLifecyclePhases();
  auto* block1 = toLayoutBlock(blockElement1->layoutObject());
  auto* block2 = toLayoutBlock(blockElement2->layoutObject());

  // Set caret into block2.
  document().body()->focus();
  selection().setSelection(SelectionInDOMTree::Builder()
                               .collapse(Position(blockElement2, 0))
                               .build());
  document().view()->updateLifecycleToLayoutClean();
  EXPECT_TRUE(block2->shouldPaintCursorCaret());
  EXPECT_EQ(block2, caretLayoutBlock());
  EXPECT_FALSE(block1->shouldPaintCursorCaret());
  EXPECT_FALSE(previousCaretLayoutBlock());

  // Move caret into block1. Should set previousCaretLayoutBlock to block2.
  selection().setSelection(SelectionInDOMTree::Builder()
                               .collapse(Position(blockElement1, 0))
                               .build());
  document().view()->updateLifecycleToLayoutClean();
  EXPECT_TRUE(block1->shouldPaintCursorCaret());
  EXPECT_EQ(block1, caretLayoutBlock());
  EXPECT_FALSE(block2->shouldPaintCursorCaret());
  EXPECT_EQ(block2, previousCaretLayoutBlock());

  // Move caret into block2. Partial update should not change
  // previousCaretLayoutBlock.
  selection().setSelection(SelectionInDOMTree::Builder()
                               .collapse(Position(blockElement2, 0))
                               .build());
  document().view()->updateLifecycleToLayoutClean();
  EXPECT_TRUE(block2->shouldPaintCursorCaret());
  EXPECT_EQ(block2, caretLayoutBlock());
  EXPECT_FALSE(block1->shouldPaintCursorCaret());
  EXPECT_EQ(block2, previousCaretLayoutBlock());

  // Remove block2. Should clear caretLayoutBlock and previousCaretLayoutBlock.
  blockElement2->parentNode()->removeChild(blockElement2);
  EXPECT_FALSE(caretLayoutBlock());
  EXPECT_FALSE(previousCaretLayoutBlock());

  // Set caret into block1.
  selection().setSelection(SelectionInDOMTree::Builder()
                               .collapse(Position(blockElement1, 0))
                               .build());
  updateAllLifecyclePhases();
  // Remove selection.
  selection().setSelection(SelectionInDOMTree());
  document().view()->updateLifecycleToLayoutClean();
  EXPECT_EQ(block1, previousCaretLayoutBlock());
}

TEST_F(CaretDisplayItemClientTest, CaretHideMoveAndShow) {
  document().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  document().page()->focusController().setActive(true);
  document().page()->focusController().setFocused(true);

  Text* text = appendTextNode("Hello, World!");
  document().body()->focus();
  updateAllLifecyclePhases();
  const auto* block = toLayoutBlock(document().body()->layoutObject());

  LayoutRect caretVisualRect = caretDisplayItemClient().visualRect();
  EXPECT_EQ(1, caretVisualRect.width());
  EXPECT_EQ(block->location(), caretVisualRect.location());

  // Simulate that the blinking cursor becomes invisible.
  selection().setCaretVisible(false);
  // Move the caret to the end of the text.
  document().view()->setTracksPaintInvalidations(true);
  selection().setSelection(
      SelectionInDOMTree::Builder().collapse(Position(text, 5)).build());
  // Simulate that the cursor blinking is restarted.
  selection().setCaretVisible(true);
  updateAllLifecyclePhases();

  LayoutRect newCaretVisualRect = caretDisplayItemClient().visualRect();
  EXPECT_EQ(caretVisualRect.size(), newCaretVisualRect.size());
  EXPECT_EQ(caretVisualRect.y(), newCaretVisualRect.y());
  EXPECT_LT(caretVisualRect.x(), newCaretVisualRect.x());

  const auto& rasterInvalidations =
      getRasterInvalidationTracking()->trackedRasterInvalidations;
  ASSERT_EQ(2u, rasterInvalidations.size());
  EXPECT_EQ(enclosingIntRect(caretVisualRect), rasterInvalidations[0].rect);
  EXPECT_EQ(block, rasterInvalidations[0].client);
  EXPECT_EQ(PaintInvalidationCaret, rasterInvalidations[0].reason);
  EXPECT_EQ(enclosingIntRect(newCaretVisualRect), rasterInvalidations[1].rect);
  EXPECT_EQ(block, rasterInvalidations[1].client);
  EXPECT_EQ(PaintInvalidationCaret, rasterInvalidations[1].reason);

  auto objectInvalidations =
      document().view()->trackedObjectPaintInvalidationsAsJSON();
  ASSERT_EQ(1u, objectInvalidations->size());
  String s;
  JSONObject::cast(objectInvalidations->at(0))->get("object")->asString(&s);
  EXPECT_EQ("Caret", s);
  document().view()->setTracksPaintInvalidations(false);
}

TEST_F(CaretDisplayItemClientTest, CompositingChange) {
  enableCompositing();
  setBodyInnerHTML(
      "<style>"
      "  body { margin: 0 }"
      "  #container { position: absolute; top: 55px; left: 66px; }"
      "</style>"
      "<div id='container'>"
      "  <div id='editor' contenteditable style='padding: 50px'>ABCDE</div>"
      "</div>");

  document().page()->focusController().setActive(true);
  document().page()->focusController().setFocused(true);
  auto* container = document().getElementById("container");
  auto* editor = document().getElementById("editor");
  auto* editorBlock = toLayoutBlock(editor->layoutObject());
  selection().setSelection(
      SelectionInDOMTree::Builder().collapse(Position(editor, 0)).build());
  updateAllLifecyclePhases();

  EXPECT_TRUE(editorBlock->shouldPaintCursorCaret());
  EXPECT_EQ(editorBlock, caretLayoutBlock());
  EXPECT_EQ(LayoutRect(116, 105, 1, 1), caretDisplayItemClient().visualRect());

  // Composite container.
  container->setAttribute(HTMLNames::styleAttr, "will-change: transform");
  updateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(50, 50, 1, 1), caretDisplayItemClient().visualRect());

  // Uncomposite container.
  container->setAttribute(HTMLNames::styleAttr, "");
  updateAllLifecyclePhases();
  EXPECT_EQ(LayoutRect(116, 105, 1, 1), caretDisplayItemClient().visualRect());
}

}  // namespace blink
