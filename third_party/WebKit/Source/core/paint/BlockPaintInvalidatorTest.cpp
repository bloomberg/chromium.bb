// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

class BlockPaintInvalidatorTest : public RenderingTest {
 protected:
  void SetUp() override {
    RenderingTest::SetUp();
    enableCompositing();
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

  Text* appendTextNode(const String& data) {
    Text* text = document().createTextNode(data);
    document().body()->appendChild(text);
    return text;
  }
};

TEST_F(BlockPaintInvalidatorTest, CaretPaintInvalidation) {
  document().body()->setContentEditable("true", ASSERT_NO_EXCEPTION);
  document().page()->focusController().setActive(true);
  document().page()->focusController().setFocused(true);

  Text* text = appendTextNode("Hello, World!");
  document().view()->updateAllLifecyclePhases();
  const auto* block = toLayoutBlock(document().body()->layoutObject());

  // Focus the body. Should invalidate the new caret.
  document().view()->setTracksPaintInvalidations(true);
  document().body()->focus();
  document().view()->updateAllLifecyclePhases();
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
  document().view()->updateAllLifecyclePhases();
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
  document().view()->updateAllLifecyclePhases();
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

}  // namespace blink
